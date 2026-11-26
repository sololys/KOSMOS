# 07 — Kaldstart-WAL, kanalisolasjon, flytid, hjørnevakt og dual-rail (`v0.2`)

Denne modulen utvider grunnmuren fra `v0.1` (`c3df7a5`) på fem punkter utledet av falsifiseringsrunden mot kaldstart mellom `PRE` og `POST`, flerkanals isolasjon, kausal flytid, 128-bits energigrenser og komplementær kretslogikk.

---

## 1. Kaldstart over `PRE` / `POST` og signert `Level 01`-gjenoppretting

En tilstandsmaskin som kun lever i arbeidsminnet mister hukommelsen ved strømbrudd dersom ny oppstart nullstiller tilstanden til `HOLD`. I `CORE/kosmos_wal.c` lukkes denne glippen i tre ledd:

1. **Synkron fil-WAL før utløsning:** Hver historisk hendelse i $B = A^*$ skrives som en fast 128-bytes binærpost (`magic = "KWAL"`, indeks, syklus, symbol, dom, vitne-ID, `prev_hash`, `payload_hash`, `entry_hash`) og tvinges til lagringsmediet med `fsync()` før kallet returnerer.
2. **Kaldstart-rekonstruksjon (`kosmos_gate_cold_boot`):** Fra helt nullstilt RAM leses hele WAL-kjeden inn fra fil og kontrolleres mot SHA-256 fra rotankeret $0^{32}$:
   * Dersom filen er avkortet midt i en post eller én bit er endret, starter porten direkte i `KOSMOS_STATE_KILL` ($u = 0$).
   * Dersom siste fullførte hendelse før strømbruddet var `KOSMOS_EVT_OPEN_AUTHORIZED` (strømmen gikk etter `PRE`, før `POST`), starter porten i **`KOSMOS_STATE_UNRESOLVED`** ($u = 0$), eksponerer den uavklarte posten for revisjon, skriver `KOSMOS_EVT_OUTCOME_UNRESOLVED` med `fsync()`, og sperrer alle nye `OPEN`-forsøk.
3. **Engangssignert `Level 01`-gjenoppretting (`HMAC-SHA256`):** Gjenoppretting krever et 32-bytes `HMAC-SHA256`-segl beregnet over `(head_hash || cycle || state)`. Fordi `head_hash` endres ved hver eneste post i loggen, er en gyldig signatur bundet til akkurat det feiltilfellet som kvitteres ut og kan aldri spilles av på nytt senere.

---

## 2. Lokalt endelig kanalgraf og `TRAP`-karantene

For et flerkanalssystem med $N = 1024$ kanaler og lokalt endelig koblingsgraf $G = (V, E_G)$ med gradgrense $\deg(k) \le d_{\max} = 4$:

* Hver kanal $k \in \{0, \dots, 1023\}$ bærer egen tilstand (`HOLD = 0x3C`, `OPEN = 0xA5`, `TRAP = 0x5A`, `KILL = 0x00`), egen monontont stigende `last_nonce` og eget 1-syklus pådrag.
* Et lokalt avvik i kanal $k$ (eksempelvis tidsavvik eller lokalt kandidatbrudd) setter kun kanal $k$ i karantenetilstanden **`TRAP` (`0x5A`, $u_k = 0$)** i $O(1)$ tid.
* Både nabokanalene $j \in N(k)$ og øvrige kanaler beholder sin tilstand og sine utganger uforstyrret, så lenge felles anleggsgrense $\mathcal{A}_{\text{hard}}$ ikke er brutt.

---

## 3. Alexandrov-tidsvindu med kalibrert usikkerhet

En fysisk måling av flytid $\tau_{\text{meas}}$ i heltallige pikosekunder utstyres med nominelt kausalitetsvindu $[\tau_{\min}, \tau_{\max}]$ og kalibrert måleusikkerhet $\delta\tau_{\text{cal}} \ge 0$. For å utelukke falsk godkjenning ved målestøy kreves det indre sikkerhetsvinduet:

$$\tau_{\min} + \delta\tau_{\text{cal}} \le \tau_{\text{meas}} \le \tau_{\max} - \delta\tau_{\text{cal}}$$

Ved $\delta\tau_{\text{cal}} = 0$ ligger den skarpe digitalgrensen eksakt mellom eksempelvis $133\,425\text{ ps}$ (godkjent) og $133\,426\text{ ps}$ (avvist).

**Ufravikelig vitnekrav mot ekko:** Et signal eller kabel-ekko som treffer midt inne i tidsvinduet gir **aldri** alene rett til `OPEN`. Uten fersk `auth_nonce` og gyldig Belnap–Dunn Chu-vitne (`VERIFIED`) avvises pakken fortsatt.

---

## 4. 128-bits hjørnevakt og logisk Landauer-regnskap

En ren $L^\infty\text{-boks}$ med grense $R = 100$ slipper gjennom hjørnevektoren $(80, 80, 10)$ fordi hver enkelt akse ligger under $100$, selv om kvadratnormen $80^2 + 80^2 + 10^2 = 12\,900$ sprenger kulebudsjettet $100^2 = 10\,000$. `CORE/kosmos_energy_128.c` supplerer derfor $L^\infty\text{-veggen}$ med en divisjonsfri, rotfri 128-bits heltallssjekk:

$$\sum_{i=1}^{3} w_i (x_i - r_i)^2 \le E_{\max}$$

beregnet eksakt med overflytsvakt og full paritet mot rasjonal referanseregning i Python (`fractions.Fraction`).

Samtidig føres en heltallig teller for kappede kandidatgrener og tilhørende informasjonsteoretisk nedre grense $\Delta Q_{\text{logisk, min}} = N_{\text{slettede bit}} \cdot e_{\text{bit}}$.

> **Bevisgrense (`L1 != kalorimetri`):** Telleren angir kun den teoretiske Landauer-nedregrensen for logisk slettede bit i modellen, ikke faktisk elektrisk varme målt i silisium eller kobber.

---

## 5. Komplementær dual-rail og grensen mot fysisk maskinvare

I `RTL/kosmos_dual_rail_gate.sv` kodes hvert styre- og domsbit på et komplementært trådpar `(rail_p, rail_n)`:

* `2'b01` = logisk `0`
* `2'b10` = logisk `1`
* `2'b00` (ledningsbrudd / spenningsfall) eller `2'b11` (kortslutning / innsprøytet puls) = **symmetribrudd**

Enhver tilstand der `(rail_p ^ rail_n)` avviker fra `1` utløser kombinatorisk `crowbar_trip = 1` på null klokkeslag og tvinger utgangsparene til feilsikker frakobling. Modulen modellerer også ikke-flyktig skyggelager (`nvm_*`) slik at varm reset mens en handling står i `IN_FLIGHT` låser maskinvaren i `STATE_UNRESOLVED` ved oppstart.

> **Bevisgrense (`L1/L2` simulering mot `L3` benk):** RTL-simulering og `fsync`-WAL beviser at styringslogikken krever kutt ved skinnefeil og bevarer `UNRESOLVED` over omstart av prosess. Fysisk galvanisk luftgap, analog tyristor-crowbar og maskinvarelåst WORM-minne må verifiseres ved egen måling på fysisk benk.
