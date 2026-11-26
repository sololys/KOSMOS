# 11 — Nashusistisk rom: parvis motkraft og diskret likevekt (`v0.3`)

Denne spesifikasjonen regulerer koblede kanaler der ingen enkeltkanal får lov til å skyve ustabilitet over på naboen eller høste gevinst ved ensidig avvik.

---

## 1. Parvis 3. lov uten kanselleringsbløff ($C_{ij} = C_{ji}$)

En dobbeltsum $\sum_{i \neq j} (C_{ij} q_j - C_{ji} q_i)$ over alle ordnede par $(i, j)$ kansellerer seg selv algebraisk til `0` for enhver matrise $C$ og fanger derfor aldri asymmetrisk kraftinjeksjon.

I `CORE/kosmos_nash_reciprocity.c` håndheves ekte mekanisk gjensidighet **par for par** for alle $0 \le i < j < K$:

1. **Symmetrisk koblingsmatrise:** $C_{ij} = C_{ji}$ for hvert eneste par $(i, j)$.
2. **Parvis kraftbalanse:** For målte koblingskrefter $F_{ij}$ kreves $F_{ij} + F_{ji} = 0$ for hvert par $(i, j)$.

Brudd på parvis gjensidighet fanges umiddelbart som **`TRAP` (`0x5A`)**, mens degenerert egenstivhet $C_{ii} \le 0$ gir **`KILL` (`0x00`)**.

---

## 2. Strengt diskret Nash-minimum i 192-bits heltall

Med kvadratisk kanalpotensial

$$U_i(q) = C_{ii} q_i^2 + 2 \sum_{j \neq i} C_{ij} q_i q_j - 2 B_i q_i$$

øker spenningskostnaden strengt ved begge ensidige heltallssteg $\delta q_i \in \{-1, +1\}$:

$$U_i(q_i + 1, q_{-i}) - U_i(q) > 0 \quad \land \quad U_i(q_i - 1, q_{-i}) - U_i(q) > 0$$

hvis og bare hvis alle $i \in \{0, \dots, K-1\}$ oppfyller den divisjonsfrie skalarulikheten:

$$2 \cdot \left\lvert \sum_{j=0}^{K-1} C_{ij} q_j - B_i \right\rvert < C_{ii}$$

Dersom minst én kanal bryter ulikheten, lønner det seg for kanalen å flytte seg alene; slusen nekter da forløsning og blir stående i **`HOLD` (`0x3C`)** til felles likevekt er nådd.
