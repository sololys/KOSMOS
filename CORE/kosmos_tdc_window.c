/*
 * KOSMOS v0.2 — Alexandrov Picosecond Time-of-Flight Window
 * CORE/kosmos_tdc_window.c (0 float, 0 heap)
 *
 * Enforces inner calibrated causality window:
 *   tau_min_ps + cal_uncert_ps <= meas_tof_ps <= tau_max_ps - cal_uncert_ps
 * and requires independent Chu + nonce verification so an echo inside the
 * lightcone window never authorizes OPEN on its own.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kosmos_v02.h"
#include <string.h>

kosmos_status_t kosmos_tdc_validate(const kosmos_tdc_window_t *win) {
    uint64_t eff_min;
    uint64_t eff_max;

    if (win == NULL) {
        return KOSMOS_ERR_NULL;
    }
    if (win->tau_min_ps > win->tau_max_ps) {
        return KOSMOS_ERR_DOMAIN;
    }
    if (win->cal_uncert_ps > (UINT64_MAX - win->tau_min_ps)) {
        return KOSMOS_ERR_OVERFLOW;
    }
    if (win->cal_uncert_ps > win->tau_max_ps) {
        return KOSMOS_ERR_DOMAIN;
    }
    eff_min = win->tau_min_ps + win->cal_uncert_ps;
    eff_max = win->tau_max_ps - win->cal_uncert_ps;
    if (eff_min > eff_max) {
        return KOSMOS_ERR_DOMAIN;
    }
    return KOSMOS_OK;
}

kosmos_status_t kosmos_tdc_check(const kosmos_tdc_window_t *win,
                                 uint64_t meas_tof_ps,
                                 uint8_t *inside_out) {
    kosmos_status_t st;
    uint64_t eff_min;
    uint64_t eff_max;

    if (inside_out != NULL) {
        *inside_out = 0u;
    }
    if (win == NULL || inside_out == NULL) {
        return KOSMOS_ERR_NULL;
    }
    st = kosmos_tdc_validate(win);
    if (st != KOSMOS_OK) {
        return st;
    }

    eff_min = win->tau_min_ps + win->cal_uncert_ps;
    eff_max = win->tau_max_ps - win->cal_uncert_ps;
    *inside_out = (meas_tof_ps >= eff_min && meas_tof_ps <= eff_max) ? 1u : 0u;
    return KOSMOS_OK;
}

kosmos_status_t kosmos_tdc_gated_authorize(kosmos_gate_t *gate,
                                           const kosmos_tdc_window_t *win,
                                           uint64_t meas_tof_ps,
                                           const int64_t hard_meas[KOSMOS_GUARD_DIM],
                                           const kosmos_phase_t *cand_phase,
                                           const kosmos_ctrl_t *cand_ctrl,
                                           kosmos_witness_pre_t *w_pre,
                                           kosmos_ctrl_t *u_out,
                                           uint8_t *verdict_out) {
    uint8_t tof_ok = 0u;
    kosmos_status_t st;

    if (u_out != NULL) {
        memset(u_out, 0, sizeof(*u_out));
    }
    if (verdict_out != NULL) {
        *verdict_out = KOSMOS_VERDICT_KILL;
    }
    if (gate == NULL || win == NULL || hard_meas == NULL || cand_phase == NULL ||
        cand_ctrl == NULL || w_pre == NULL || u_out == NULL || verdict_out == NULL) {
        return KOSMOS_ERR_NULL;
    }

    st = kosmos_tdc_check(win, meas_tof_ps, &tof_ok);
    if (st != KOSMOS_OK) {
        return st;
    }

    if (tof_ok == 0u) {
        /* Outside calibrated Alexandrov lightcone window -> reject at 0 mV without advancing nonce */
        *verdict_out = (gate->state == (uint32_t)KOSMOS_STATE_KILL)
                           ? KOSMOS_VERDICT_KILL
                           : KOSMOS_VERDICT_HOLD;
        return KOSMOS_OK;
    }

    /* Inside ToF window is necessary, never sufficient: full Chu + nonce + boundary gate follows */
    return kosmos_gate_authorize(gate, hard_meas, cand_phase, cand_ctrl, w_pre, u_out, verdict_out);
}
