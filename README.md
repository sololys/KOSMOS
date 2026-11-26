# KOSMOS

[![ORCID](https://img.shields.io/badge/ORCID-0009--0006--0431--6637-A6CE39?style=flat-square)](https://orcid.org/0009-0006-0431-6637)
[![License](https://img.shields.io/badge/License-Apache_2.0-4c566a?style=flat-square)](LICENSE)

> Faserommet kan vendes ved $0.00\text{ V DC}$.  
> Historien kan bare forlenges gjennom vitne.  
> Kontinuerlig bevegelse krysser aldri terskelen alene.

**KOSMOS** ($\mathfrak{K}$) er en deterministisk univers- og realiseringsmodell skåret ut i heltalls-C11 (`0 float`), SystemVerilog IEEE 1800-2012 RTL og verifiserbare bevistester. Modellen samler tre soner i én sammenhengende kropp: et vendbart tredimensjonalt symplektisk faserom i hvile (`HOLD`), en vitnebundet to-takts sluse ($\Omega$), og en uforanderlig, prefiks-ordnet historikk med SHA-256-kjede.

* **Forfatter:** Marius Egerhei Torjusen (`ORCID: 0009-0006-0431-6637`)
* **Lisens:** Apache-2.0
* **Bevisgrense:** `L1 Deterministisk Programvare & Syntetiserbar RTL` · `INSPIRED_BY != IMPLEMENTS != PROVES`

---

## 0. Terskelen mellom benken og kvitteringen

På arbeidsbenken kan en kloss snus rundt i hånden hundre ganger uten at noe utenfor rommet har flyttet seg. Så lenge hovedbryteren står åpen med fem millimeter tørr luft mellom kobberklossene og $0.00\text{ V}$ på skinna, koster et vilt forslag bare regnetid. En bane kan speiles baklengs, måles mot veggen og legges rolig tilbake på bordet hvis den er skjev. Ingenting i terrenget har brent fast.

Fellen i nesten all digital modellering er å bruke samme tall og samme minnecelle for tanken før bryteren og handlingen etterpå. Da glir et uferdig utkast rett ut i jernet bare fordi en klokke tikket ut, eller fordi et gjennomsnitt så pent ut på skjermen mens én skarp spiss sprengte grensen. Enda verre er troen på at en utført handling kan angres ved å trekke fra samme tall i neste linje, som om en utskrevet kvittering på papir forsvinner hvis kassaapparatet slås av.

I **KOSMOS** er kandidaten inne i rommet og hendelsen utenfor skilt med et rent topologisk kutt. Posisjonen $q = (x, y, z)$ hviler på et tredimensjonalt gulv uten kunstig nullpunkt, mens spenningen og bevegelsen $p = (p_x, p_y, p_z)$ henger over hvert punkt som dets ledsager. Inne i dette rommet vender fasespeilet $J(q, p) = (q, -p)$ bevegelsen eksakt tilbake ved nytt slag, uten at den historiske adressen rører seg en millimeter. For å krysse dørstokken ut av rommet kreves to adskilte grep: først et uavhengig forhåndsvitne ($W_{\mathrm{PRE}}$) som slipper ett steg ut på broen, og deretter et kjedet ettervitne ($W_{\mathrm{POST}}$) som bekrefter hva som faktisk traff bakken. Forsvinner strømmen midt på broen, stenges slusen i uavklart lås frem til et menneske ved spaken gjør opp regnskapet med ny signatur i loggen.

---

## 1. Den samlede konstruksjonen ($\mathfrak{K}$)

$$\mathfrak{K} = \bigl(E,\; B,\; \omega,\; J,\; \mathcal{A},\; \mathcal{C}_4,\; \Omega\bigr)$$

Totalrommet for kandidater er en tellbar disjunkt sum over alle endelige historikkprefikser $h \in B = A^*$:

$$E = \bigsqcup_{h \in B} \bigl(\{h\} \times T^*Q \times U \times C_h\bigr), \qquad \mathfrak{R}_\Omega = E \sqcup B$$

Et punkt i kandidatrommet har fem koordinater $x = (h, q, p, u, s)$: historisk prefiks $h \in A^*$, posisjon $q \in Q$, impuls $p \in T_q^*Q$, pådrag $u \in U$, og kompatibel fremtidsgren $s \in C_h \subseteq A^{\mathbb{N}}$.

```text
+------------------------------------------------------------------------+
|  KOSMOS : K = (E, B, omega, J, A, C_4, Omega)                          |
+------------------------------------------------------------------------+
|  CHAMBER E (HOLD : u = 0, 0.00 V DC, pi o gamma = const)               |
|                                                                        |
|  Base Q     : 3D Affine space over Euclidean displacements (no origin) |
|  Body T*Q   : Symplectic phase z = (q, p), omega = sum dq_i ^ dp_i     |
|  Mirror J   : Antisymplectic involution J(q, p) = (q, -p), J*w = -w    |
|  Control U  : Exact integer L-inf Banach box (poison INT64_MIN fenced) |
|  Bench C_4  : Belnap-Dunn Chu pairing P x W -> {?, 0, 1, CONFLICT}     |
|  Seam Loop  : Holonomy defect Delta_seam == 0 required before gate     |
+-----------------------------------+------------------------------------+
                                    |
                          [ OMEGA SLUICE GATE ]
                   Stroke 1 : W_PRE  -> OPEN (IN_FLIGHT)
                   Stroke 2 : W_POST -> REALIZED (or UNRESOLVED_LOCK)
                                    |
+-----------------------------------+------------------------------------+
|  DISCRETE HISTORY B = A* & CANTOR CONTINUATION C_h subset A^N          |
|                                                                        |
|  Prefix Law : h_{n+1} = h_n || e_n  (SHA-256 chained, 0 overwrite)     |
|  Cylinder   : C_{h || e} subset C_h (exact integer prefix depth l)     |
|  Verdicts   : OPEN (0xA5) | HOLD (0x3C, 0 mV) | KILL (0x00, 0 mV)      |
+------------------------------------------------------------------------+
```

### Byggesteinene og deres presise oppgaver

| Rom i $\mathfrak{K}$ | Konkret jobb i modellen | Hva som holdes utenfor |
| :--- | :--- | :--- |
| **Affine space** ($Q$ over 3D Euclidean) | Posisjon $q = (q_x, q_y, q_z)$ uten kunstig origo; kun differanser $q_2 - q_1$ er vektorer. | Rent vektorrom med påtvunget $(0,0,0)$. |
| **Fiber + Symplectic + Liouville space** ($T^*Q, \omega$) | Kobler sted $q$ og spenning $p$ med $\omega = \sum dq_i \wedge dp_i$ og bevart fasevolum i modellen. | Løse variabellister uten paring av kraft og vei. |
| **Hausdorff ($T_2$) + Banach $L^\infty$** ($U, \mathcal{A}$) | Skiller tilstander rent og måler verste enkeltavvik $\lvert x_i - r_i \rvert \le \tau_i$ uten divisjon eller flyttall. | $L^2\text{-snitt}$ som skjuler farlige enkeltpulser. |
| **Chu space over Belnap–Dunn $\mathcal{FOUR}$** ($\mathcal{C}_4$) | Prøver kandidater $P$ mot vitner $W$ med fire utfall: `?`, `0`, `1` og `CONFLICT` ($\top$). | Treverdig tabell som gjemmer vitnekonflikt i `?`. |
| **Loop space** (sømkontroll $\Delta_{\text{seam}}$) | Fanger ikke-lukkende sløyfer rundt kammeret som hard feil i stedet for å glatte over glippen. | Taus avrunding av globale sømavvik. |
| **Sierpiński space** ($\mathbb{S} = \{0, 1\}$) | Klassifiserer åpne vitnepredikater topologisk (ikke påstått som fysisk tilbakelås). | Magisk tro på at topologi alene hindrer tilbakeskriving. |
| **Locally finite + Sequence + Cantor + Polish space** | Diskret historikk $B = A^*$, krympende sylindere $C_{h \parallel e} \subsetneq C_h$ og komplett separabelt totalrom $E$. | Tiden lagt ned som ferdig fjerde gulvplanke. |

---

## 2. Kosmos-teoremet og Karbor-skjerpingene

1. **Kosmos-teoremet ($\pi \circ \gamma = \text{konstant}$):** Projeksjonen $\pi : E \to B$ gitt ved $\pi(h, q, p, u, s) = h$ er kontinuerlig mot et diskret, lokalt endelig basisrom $B$. Enhver kontinuerlig bane $\gamma : [0, 1] \to E$ tvinges dermed til å ligge i ett og samme historiske fiber. Ingen glatt deformasjon i `HOLD` kan snike seg over i en ny historisk hendelse; overgangen krever sluseoperasjonen $\Omega$.
2. **Symmetrisk heltallsgitter uten toerkomplement-felle:** I vanlig 64-bits heltallsaritmetikk mangler `INT64_MIN` ($-2^{63}$) positiv motpart, slik at $-p$ gir udefinert oppførsel og bryter $J^2 = I$. **KOSMOS** gjerder ut `INT64_MIN` som ulovlig giftverdi (`KOSMOS_POISON_MIN`) og låser faserommet til det eksakt symmetriske gitteret $[-M, +M]^6$ ($M = 2^{63}-1$), der $J(J(z)) = z$ og $J^*\omega = -\omega$ holder bit-eksakt i 128-bits heltall.
3. **Skille mellom kandidatavvisning (`HOLD`) og hard grense (`KILL`):** En vill idé inne i tenkerommet skal avvises til `HOLD` ($u = 0$), ikke sprenge hovedsikringen. Bare brudd på anleggets harde ramme $\mathcal{A}_{\text{hard}}$, motstridende vitner (`CONFLICT`), åpen sløyfesprekk ($\Delta_{\text{seam}} \neq 0$), repriseforsøk eller giftverdier utløser låst `KILL` ($0.00\text{ V}$).
4. **To-takts realisering (`OPEN != REALIZED`) og kaldstart-lås (`v0.2`):** Godkjent $W_{\mathrm{PRE}}$ gir dommen `OPEN` (`0xA5`), skriver binær WAL-post med `fsync()` og flytter slusen til `IN_FLIGHT`. Forsvinner strømmen før $W_{\mathrm{POST}}$, gjenoppstår porten ved kaldstart fra disk direkte i `UNRESOLVED` med $u = 0$, og gjenoppretting krever engangs `HMAC-SHA256`-signatur over gjeldende `head_hash`.
5. **Kanalisolasjon, pikosekund-flytid, 128-bits hjørnevakt og dual-rail (`v0.2`):** 1 024 lokalt endelige kanaler ($\deg \le 4$) med isolert `TRAP` (`0x5A`), Alexandrov-tidsvindu $[\tau_{\min} + \delta\tau_{\text{cal}}, \tau_{\max} - \delta\tau_{\text{cal}}]$, rotfri 128-bits kulevakt som fanger hjørnefeller som $(80, 80, 10)$ mot $100^2$, og komplementær dual-rail SystemVerilog-sluse (`00`/`11` gir kombinatorisk `crowbar_trip`).

---

## 3. Mappeinndeling

```text
KOSMOS/
|-- LICENSE
|-- Makefile
|-- MANIFEST.sha256
|-- README.md
|-- KOSMOS_MANIFEST.md
|-- SPEC/
|   |-- 00_SPACE.md
|   |-- 01_SYMPLECTIC_BODY.md
|   |-- 02_PHASE_MIRROR.md
|   |-- 03_BOUNDARY.md
|   |-- 04_CHU_WITNESS.md
|   |-- 05_OMEGA_GATE.md
|   |-- 06_HISTORY.md
|   |-- 07_CHANNEL_CONE_ENERGY.md
|   `-- 08_DUAL_RAIL_AND_WAL_RECOVERY.md
|-- CORE/
|   |-- kosmos_space.h
|   |-- kosmos_v02.h
|   |-- kosmos_sha256.h
|   |-- kosmos_sha256.c
|   |-- kosmos_mirror.c
|   |-- kosmos_boundary.c
|   |-- kosmos_gate.c
|   |-- kosmos_history.c
|   |-- kosmos_wal.c
|   |-- kosmos_channel_sluice.c
|   |-- kosmos_tdc_window.c
|   `-- kosmos_energy_128.c
|-- RTL/
|   |-- kosmos_gate.sv
|   |-- tb_kosmos_gate.sv
|   |-- kosmos_dual_rail_gate.sv
|   `-- tb_kosmos_dual_rail_gate.sv
`-- TESTS/
    |-- kosmos_ctypes.py
    |-- test_core_c11.c
    |-- test_involution.py
    |-- test_boundary.py
    |-- test_witness.py
    |-- test_prefix.py
    |-- test_no_unauthorized_commit.py
    |-- test_v02_extensions.py
    |-- test_round2_adversarial.py
    `-- verify_surface.py
```

---

## 4. Bygg og verifikasjon

```bash
make verify
```

`make verify` kjører hele kjeden uten eksterne pakker:

* **C11-kjerne med ASan/UBSan:** Kompilerer `CORE/` og `TESTS/test_core_c11.c` med `gcc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -fsanitize=address,undefined`.
* **SystemVerilog RTL & syntesesjekk:** Kjører `iverilog -g2012 -Wall` på `RTL/kosmos_gate.sv` + `RTL/tb_kosmos_gate.sv` og verifiserer syntetiserbarhet gjennom `yosys`.
* **De fem Python-bruddprøvene mot `libkosmos.so`:** Verifiserer speilinvolusjon, $L^\infty\text{-grenser}$, Belnap–Dunn Chu-vitner, prefikslås med Cantor-sylindere, og sperring mot uautorisert eller uavklart realisering.
* **Overflate- og seglkontroll:** Verifiserer `0 float` i kjerne og RTL, `0` lekkede stier, kollisjonsfri MathJax, rette `text`-rammer og gyldig `MANIFEST.sha256`.

---

## 5. Bevisgrense (`L1 != PROVES`)

* **Verifisert lokalt (`L1`):** Eksakt heltallsgeometri på $[-M, +M]^6$, 128-bits antisymplektisk identitet $J^*\omega = -\omega$, divisjonsfri $L^\infty\text{-kontroll}$, to-takts vitnelås (`PRE` / `POST`), kryptografisk kjedet prefikshistorikk (FIPS 180-4 SHA-256), og syntetiserbar SystemVerilog RTL-paritet.
* **Utenfor programvarens rekkevidde:** Fysisk tapsfrihet eksisterer ikke i silisium; enhver reell sletting og svitsjing dissiperer varme. Programvarekjeden i `kosmos_history.c` er en append-only kryptografisk logg i minne, ikke fysisk WORM-maskinvare, og fysisk nødkutt krever mekanisk luftgap på `Level 01`.
