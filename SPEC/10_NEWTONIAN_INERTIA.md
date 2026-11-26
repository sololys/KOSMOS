# 10 — Nitonisk rom: lokal treghet, arbeid og retten til hvile (`v0.3`)

Denne spesifikasjonen fastsetter mekanisk treghet **inne i én lokal klokkedrevet node** (i motsetning til utveksling mellom adskilte noder i `SPEC/12`).

---

## 1. Ingen masseløse hopp ($\Delta t > 0, m_i \ge 1$)

Ingen fysisk konfigurasjon $q \in \mathbb{Z}_M^3$ kan endre posisjon på null klokkeslag eller med null treghetsmasse. For tre påfølgende tilstander $q_{t-2}, q_{t-1}, q_t$ adskilt med $\Delta t \in \mathbb{N}$ lokale klokkeslag og diagonale masser $m = (m_x, m_y, m_z)$:

* Dersom $\Delta t = 0$, $m_i \le 0$, $F_{\max,i} \le 0$ eller noen koordinat inneholder `KOSMOS_POISON_MIN`, utløses **`KILL` (`0x00`)**.

---

## 2. Impulstak i 192-bits overflytssikker aritmetikk

La trinnforskyvningene være $\Delta q_{1,i} = q_{t-1,i} - q_{t-2,i}$ og $\Delta q_{2,i} = q_{t,i} - q_{t-1,i}$, og akselerasjonsleddet $\Delta^2 q_i = \Delta q_{2,i} - \Delta q_{1,i}$. For hver akse $i \in \{1, 2, 3\}$ kreves:

$$m_i \cdot \lvert \Delta^2 q_i \rvert \le F_{\max,i} \cdot \Delta t$$

Brudd på impulstaket fanges som ufysisk akselerasjon og gir **`TRAP` (`0x5A`)**.

---

## 3. Eksakt doblet kinetisk arbeid og retten til hvile

En naiv heltallsformel $\Delta E_{\mathrm{kin}} = m((\Delta q_2)^2 - (\Delta q_1)^2) / 2$ kutter bort restleddet slik at $m = 1, \Delta q_1 = 0, \Delta q_2 = 1$ gir `1 / 2 == 0` gratis bevegelsesenergi.

`CORE/kosmos_newtonian_inertia.c` unngår all divisjon ved å føre energisjekken eksakt i **doblede enheter**:

$$W_{2\mathrm{x}} = \sum_{i=1}^{3} m_i \Bigl((\Delta q_{2,i})^2 - (\Delta q_{1,i})^2\Bigr)$$

Med gjenværende energibudsjett $E_{\mathrm{rem}}$, hvilegrense $W_{\min}$ og entropitoll $Q_{\mathrm{toll}}$ kreves:

$$E_{\mathrm{rem}} \ge W_{\min} \quad \land \quad 2 E_{\mathrm{rem}} \ge \max(0, W_{2\mathrm{x}}) + 2 Q_{\mathrm{toll}}$$

Faller energien under noen av disse to grensene, trer **retten til hvile** i kraft og holder porten i **`HOLD` (`0x3C`, $u = 0$)**.
