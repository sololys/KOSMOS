# KOSMOS — Arkitekturmanifest (Karbor-utgave v0.1)

$$\mathfrak{K} = \bigl(E,\; B,\; \omega,\; J,\; \mathcal{A},\; \mathcal{C}_4,\; \Omega\bigr)$$

Dette manifestet fastsetter hva som ble beholdt fra utkast I og II, og hvilke fem hull som ble skåret bort og herdet under sammensmiingen til kjørbar C11 og SystemVerilog RTL.

---

## 1. De fem Karbor-rettelsene over Kandidat II

| Punkt | Hull i tidligere utkast | Herdet løsning i `KOSMOS` v0.1 |
| :--- | :--- | :--- |
| **1. Toerkomplement-fellen i $J$** | I 64-bits heltall gir $-(\text{INT64\_MIN})$ udefinert oppførsel (UB) og bryter $J(J(z)) = z$. | `INT64_MIN` gjerdes ut som `KOSMOS_POISON_MIN`. Faserommet hviler på det eksakt symmetriske gitteret $[-M, +M]^6$ ($M = 2^{63}-1$), med 128-bits verifikasjon av $J^*\omega = -\omega$. |
| **2. Divisjon og $\mathbb{R}$ i $L^\infty\text{-veggen}$** | Normalisering $g_i(x) = (x_i - r_i)/\tau_i$ krever enten flyttall eller heltallsdivisjon som runder av `15/10` ned til `1`. | Divisjonsfri, fortegnssikker avstand i offset-binær `uint64_t`: $\lvert x_i - r_i \rvert \le \tau_i$ med krav $\tau_i > 0$. |
| **3. Kandidat-avslag mot anleggs-`KILL`** | Ethvert avvik $\lVert g(x) \rVert_\infty > 1$ utløste `KILL`, slik at utforskning av dristige kandidater i `HOLD` ville låst maskinen. | Streng deling mellom $\mathcal{A}_{\text{cand}}$ (avviser forslaget til `HOLD` med $u = 0$) og $\mathcal{A}_{\text{hard}}$ (låser `KILL` ved faktisk grensebrudd). |
| **4. Vitnekonflikt i $\{0, ?, 1\}$** | Når vitne $w_1$ melder `1` og vitne $w_2$ melder `0`, mangler treverdig tabell et eget tegn for selvmotsigelse. | Chu-evalueringen løftes til Belnap–Dunn $\mathcal{FOUR} = \{\bot, \mathsf{F}, \mathsf{T}, \top\}$ (`0b00..0b11`) paret med sømkontroll $\Delta_{\text{seam}} = 0$ fra sløyferommet. Konflikt gir øyeblikkelig `KILL`. |
| **5. Mellomtilstand mellom `OPEN` og `REALIZED`** | Kravet `OPEN != REALIZED` manglet en eksplisitt tilstandslås mens handlingen er under utførelse. | To-takts tilstandsmaskin (`HOLD -> IN_FLIGHT -> HOLD / UNRESOLVED / KILL`) med kryptografisk binding mellom `pre_hash` og `W_POST`. |

---

## 2. Påbygg i `v0.2` etter kaldstart- og benkrevisjon

| Modul | Bruddflate som tettes | Verifisert mekanisme (`L1` C11 + RTL) |
| :--- | :--- | :--- |
| **Kaldstart-WAL & Signert L01** | Strømbrudd med nullstilt RAM mellom `PRE` og `POST`, samt gjenbruk av statisk L01-nøkkel. | `fsync`-basert binær WAL (`kosmos_wal.c`) som starter rett i `UNRESOLVED` etter ufullført `PRE`, og engangs `HMAC-SHA256` over `(head_hash, cycle, state)`. |
| **Lokalt endelig kanalgraf** | Én feilende kanal drar ned nabokanaler eller mangler isolert karantene. | 1 024 kanaler med $\deg(k) \le 4$ der lokalt avvik setter kun kanal $k$ i `TRAP` (`0x5A`, $u_k = 0$) i $O(1)$ tid. |
| **Alexandrov-flytid (TDC)** | Pakker utenfor lyskjeglen eller ekko inne i vinduet uten eget vitne. | Heltallig pikosekundsjekk $[\tau_{\min} + \delta\tau_{\text{cal}}, \tau_{\max} - \delta\tau_{\text{cal}}]$ der ekko innenfor vinduet fortsatt krever Chu-vitne og fersk nonce. |
| **128-bits hjørnevakt & Landauer** | Hjørnevektor $(80, 80, 10)$ passerer $L^\infty \le 100$, men sprenger $100^2 = 10\,000$. | Eksakt 128-bits heltallig kvadratsum uten rot eller divisjon, pluss eksplisitt logisk Landauer-nedregrense (`L1 != fysisk kalorimetri`). |
| **Dual-rail & NVRAM-skygge** | Enkelttråds bitfeil (`0 -> 1`) og tap av `IN_FLIGHT` over varm reset i RTL. | Komplementære trådpar (`01`/`10`, mens `00`/`11` løser ut kombinatorisk `crowbar_trip`), to-syklus `WAL-før-puls` og klebrig `unresolved_latched` i NVRAM (`v0.2.1`). |

---

## 3. Universelle naturlover i `v0.3` (Ren universmodell)

| Modul | Fysisk virkefelt | Invariant i C11 og RTL (`0 float`, `XOR == 0`) |
| :--- | :--- | :--- |
| **`SPEC/09` · Cellekalkyle & Lenz** | 3D-felt på hjørner, kanter, flater og bokser | $D_1(D_0 f) \equiv 0$, $D_2(D_1 e) \equiv 0$, 7-punkts heltalls-Laplacian uten sjakkbrett-felle, og Faraday–Lenz-lås $\mathcal{E} \cdot \Delta \Phi_B \le 0$. |
| **`SPEC/10` · Nitonisk rom** | Lokal treghet inne i én node | $\Delta t > 0$, $m_i \ge 1$, $\lvert m_i \Delta^2 q_i \rvert \le F_{\max,i} \Delta t$, eksakt doblet kinetisk arbeid (ingen `1/2 == 0` gratisenergi) og retten til hvile (`HOLD`). |
| **`SPEC/11` · Nashusistisk rom** | Koblede kanaler og gjensidighet | Parvis 3.-lovs-symmetri $C_{ij} == C_{ji}$ og $F_{ij} + F_{ji} == 0$ (uten død kanselleringssum) pluss strengt Nash-punkt $2\lvert \sum_j C_{ij} q_j - B_i \rvert < C_{ii}$. |
| **`SPEC/12` · Horisont & Gödel** | Mellom noder og mot evighetsløkker | Minkowski-lyskjegle $\Delta r^2 \le c^2 \Delta t^2$, luftspalte-lås med åpningsarbeid, entropitak $\dot{S}_{\mathrm{gen}} \le \dot{S}_{\mathrm{sink}}$ og Gödel-iterasjonstak mot selvreferanse. |

---

## 4. Grunnlovene i granitt

1. **Involusjon og antisymplektisk speil:** $J(q, p) = (q, -p)$ oppfyller $J^2 = I$ og $J^*\omega = -\omega$ på hele $[-M, +M]^6$.
2. **Topologisk frakobling:** Enhver kontinuerlig bane $\gamma : [0, 1] \to E$ oppfyller $\pi \circ \gamma = \text{konstant}$.
3. **Enveis aktuering:** $u_{\text{out}} \neq 0 \implies (\text{dom} = \text{OPEN} \land x \in \mathcal{A}_{\text{hard}} \cap \mathcal{A}_{\text{cand}} \land r(x, W_{\mathrm{PRE}}) = \mathsf{T} \land \Delta_{\text{seam}} = 0)$, aldri $\iff$.
4. **Monoton prefikshistorikk:** $h_{n+1} = h_n \parallel e_n$ med strengt krympende Cantor-sylinder $C_{h_{n+1}} \subsetneq C_{h_n}$ og uforanderlig SHA-256-kjede.
5. **Suveren tilbakestilling (`Level 01`):** `KILL` og `UNRESOLVED` kan ikke slettes av programvare; autorisert `Level 01`-gjenoppretting forlenger historikken med en ny hendelse (`EVT_L01_RECOVERY`) og fører kammeret tilbake til `HOLD` ($0.00\text{ V}$).
