# 04 — Vitnebenken ($\mathcal{C}_4$) og sømkontrollen

## 1. Chu-rom over Belnap–Dunn $\mathcal{FOUR}$

Vitnebenken modelleres som et Chu-rom $\mathcal{C}_4 = (P, r, W)$ der kandidatene $P$ prøves mot vitnene $W$ gjennom evalueringen:

$$r : P \times W \longrightarrow \mathcal{FOUR} = \{\bot, \mathsf{F}, \mathsf{T}, \top\}$$

I 2-bits maskinrepresentasjon (`kosmos_chu_val_t`) tilsvarer hvert bit én uavhengig evidensretning (`bit 0 = motbevist`, `bit 1 = verifisert`):

| Kode | Symbol | Navn | Virkning ved fold over vitner $W$ |
| :--- | :--- | :--- | :--- |
| `0b00` | $\bot$ (`?`) | `KOSMOS_CHU_UNKNOWN` | Utilstrekkelig evidens; porten blir stående i `HOLD` ($u = 0$). |
| `0b01` | $\mathsf{F}$ (`0`) | `KOSMOS_CHU_REFUTED` | Kandidat motbevist; avvises til `HOLD` ($u = 0$). |
| `0b10` | $\mathsf{T}$ (`1`) | `KOSMOS_CHU_VERIFIED` | Alle påkrevde vitner bekreftet uten motbevis; kvalifiserer for `OPEN`. |
| `0b11` | $\top$ (`!`) | `KOSMOS_CHU_CONFLICT` | Ett vitne bekrefter mens et annet motbeviser; utløser låst `KILL` ($u = 0$). |

Sammenslåing over et panel av vitner $w_1, \dots, w_k$ skjer ved bitvis OR på de to evidensbitene. Dermed kan et positivt vitne aldri overdøve et negativt vitne: `0b10 | 0b01 == 0b11` (`CONFLICT`).

---

## 2. Sløyfesømmen ($\Delta_{\text{seam}}$)

Fra sløyferommet (`Loop space`) tar `KOSMOS` inn en heltallig holonomisjekk `seam_defect` ($\Delta_{\text{seam}}$). Lokale lapper kan hver for seg se konsistente ut mens sløyfen rundt hele kammeret nekter å lukke seg ($\Delta_{\text{seam}} \neq 0$). En åpen søm glattes aldri over: $\Delta_{\text{seam}} \neq 0$ tvinger porten direkte i `KILL`.

---

## 3. Sierpiński-rommets faktiske plass

Sierpiński-rommet $\mathbb{S} = \{0, 1\}$ med åpne mengder $\{\varnothing, \{1\}, \{0, 1\}\}$ klassifiserer den asymmetriske observasjonen «vitne har truffet» ($\{1\}$ er åpen, hvile $\{0\}$ er lukket). Det utgjør klassifikatoren for åpne predikater på benken, mens selve forbudet mot å spole historien tilbake håndheves av overgangsloven i `SPEC/05_OMEGA_GATE.md` og `SPEC/06_HISTORY.md`.
