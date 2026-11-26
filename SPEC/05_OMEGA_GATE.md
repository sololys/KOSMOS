# 05 — Sluseporten ($\Omega$) og to-takts realisering

## 1. De fire låsetilstandene og de tre dommene

Slusen $\Omega$ styrer koblingen mellom kandidatrommet $E$ og historikken $B$ gjennom fire indre låsetilstander og tre ytre dommer med stor innbyrdes Hamming-avstand:

* **`KOSMOS_VERDICT_OPEN` (`0xA5`, `1010_0101`):** Autoriserer ett steg ut i `IN_FLIGHT` under gyldig $W_{\mathrm{PRE}}$.
* **`KOSMOS_VERDICT_HOLD` (`0x3C`, `0011_1100`):** Bevarer tilstanden med $u_{\text{out}} = 0$ (`0 mV`).
* **`KOSMOS_VERDICT_KILL` (`0x00`, `0000_0000`):** Frakobler aktuator ved $u_{\text{out}} = 0$ (`0 mV`) og låser porten til eksplisitt `Level 01`-gjenoppretting.

Enveis-loven håndheves i hver eneste evaluering:

$$u_{\text{out}} \neq 0 \implies \bigl(\text{dom} = \text{OPEN} \land x \in \mathcal{A}_{\text{hard}} \cap \mathcal{A}_{\text{cand}} \land r(x, W_{\mathrm{PRE}}) = \mathsf{T} \land \Delta_{\text{seam}} = 0\bigr)$$

Det motsatte gjelder aldri: en lovlig tilstand $x \in \mathcal{A}$ uten ferskt vitne $W_{\mathrm{PRE}}$ blir liggende trygt i `HOLD` med $u_{\text{out}} = 0$.

---

## 2. Takt 1 og Takt 2 (`OPEN != REALIZED`)

```text
  [ HOLD ] ---(gyldig W_PRE, ny nonce, innenfor A)---> [ IN_FLIGHT (OPEN) ]
     ^                                                        |
     |---(kjedet W_POST med pre_hash og outcome_ok == 1)------+
                                                              |
     [ UNRESOLVED_LOCK (0 mV) ] <--(manglende/feil W_POST)----+
```

1. **Takt 1 (`kosmos_gate_authorize`):**
   * Krever tilstand `HOLD`, strengt stigende klokkesyklus (`cycle > last_cycle`), strengt stigende `auth_nonce > last_nonce` (hindrer gjenbruk av gammel autorisasjon), `witness_id != 0`, $\Delta_{\text{seam}} = 0$, $x \in \mathcal{A}_{\text{hard}} \cap \mathcal{A}_{\text{cand}}$ og `KOSMOS_CHU_VERIFIED`.
   * Beregner `pending_pre_hash` over tilstand, pådrag og $W_{\mathrm{PRE}}$, skriver `KOSMOS_EVT_OPEN_AUTHORIZED` til historikken, setter tilstanden til `IN_FLIGHT` og slipper ut `u_out = u_cand`.
   * Så lenge slusen står i `IN_FLIGHT`, avvises ethvert nytt autorisasjonsforsøk.
2. **Takt 2 (`kosmos_gate_confirm_post` / `kosmos_gate_power_loss`):**
   * Krever `tether_pre_hash == pending_pre_hash`, `witness_id != 0`, `cycle >= open_cycle` og `outcome_ok == 1`. Da skrives `KOSMOS_EVT_POST_REALIZED` til historikken, og slusen går tilbake til `HOLD` med $u_{\text{out}} = 0$.
   * Ved strømbrudd, tidsavbrudd eller feil/manglende $W_{\mathrm{POST}}$ går slusen i `KOSMOS_STATE_UNRESOLVED`, kutter $u_{\text{out}} = 0$ og logger `KOSMOS_EVT_OUTCOME_UNRESOLVED`. Porten later aldri som en påbegynt handling ikke fant sted.
