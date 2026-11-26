# 12 — Kausal horisont, luftspalte, entropitak og Gödel-vitne (`v0.3`)

Denne spesifikasjonen skiller lokal klokketid inne i noden (`SPEC/10`) fra relativistisk og asynkron kobling **mellom** adskilte noder, og setter harde skranker mot uendelig intern rekursjon.

---

## 1. Minkowski-lyskjegle mellom noder ($\Delta r^2 \le c^2 \Delta t^2$)

Mellom to romlig adskilte hendelser med forskyvning $(\Delta x, \Delta y, \Delta z)$ og tidsavstand $\Delta t \ge 0$ ved lyshastighet $c_{\mathrm{tick}} > 0$ i gitterenheter per tikk er det skalerte intervallet:

$$ds^2 = \bigl(\Delta x^2 + \Delta y^2 + \Delta z^2\bigr) - (c_{\mathrm{tick}} \cdot \Delta t)^2$$

* **$ds^2 > 0$ (romlig / superluminal injeksjon, inkludert $\Delta r \neq 0$ ved $\Delta t = 0$):** Krever uendelig arbeid og utløser **`KILL` (`0x00`)**.
* **Splitt-hjerne uten signert flette-vitne:** Tvinger noden i **`HOLD` (`0x3C`)** (ingen taus overskriving med «siste klokke vinner»).

---

## 2. Luftspalte-karantene (`AWAITING_SYNC`)

Over en fysisk luftspalte (`airgap_sealed == 1`) finnes ingen delt minnetabell:

* Uten gyldig opphavskvittering (`ancestry_receipt_valid == 0`) holdes mottaker i **`HOLD` (`0x3C`)**.
* Påstand om mottatt kvittering over åpnet barriere (`airgap_sealed == 0`) med null fysisk åpningsarbeid ($W_{\mathrm{lift}} = 0$) dømmes som forfalskning og gir **`KILL` (`0x00`)**.

---

## 3. Regenerativt entropitak og Gödels vitnegrense

1. **Substratets opptakstak ($\dot{S}_{\mathrm{gen}} \le \dot{S}_{\mathrm{sink}}$):** Dersom entropiproduksjonen overstiger substratets opptaksevne ($\dot{S}_{\mathrm{gen}} > \dot{S}_{\mathrm{sink}}$), utløses **`KILL` (`0x00`)**.
2. **Gödels eksterne vitnekrav:** Et lukket formelt system kan ikke avgjøre egne grensetilfeller gjennom uendelig indre selvreferanse:
   * Overstiger indre søkeiterasjoner taket ($N_{\mathrm{iter}} > N_{\max}$) uten eksternt vitne ($W_{\mathrm{ext}} = 0$), fanges prosessen i **`TRAP` (`0x5A`)** før den brenner kraft uten fremdrift.
   * Ved påvist uavgjørbar grense uten $W_{\mathrm{ext}}$ hviler slusen i **`HOLD` (`0x3C`)** til eksternt vitne foreligger.

---

## 4. Samlet prioritetslov i `kosmos_v03_physics_gate` (`XOR == 0`)

Både C11-referansen (`kosmos_eval_v03_top_gate`) og SystemVerilog-modulen (`RTL/kosmos_v03_physics_gate.sv`) evaluerer nøyaktig samme deterministiske prioritetskjede med Hamming-separerte dommer:

$$\text{KILL (`0x00`)} \succ \text{TRAP (`0x5A`)} \succ \text{HOLD (`0x3C`)} \succ \text{OPEN (`0xA5`)}$$

kombinert med to-syklus `WAL-før-puls` og klebrig `unresolved_latched` fra `SPEC/08`.
