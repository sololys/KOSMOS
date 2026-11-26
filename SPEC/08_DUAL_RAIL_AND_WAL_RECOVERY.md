# 08 — Dual-Rail, WAL-før-puls og revnet-hale-karantene (`v0.2.1`)

Denne spesifikasjonen dokumenterer de tre sikkerhetslåsene som ble sveiset inn etter den uavhengige runde-2-revisjonen av `f6d524e`.

---

## 1. Revnet `KWAL`-hale uten tap av prefiks eller `last_nonce`

Dersom strømmen brytes midt under skriving av en 128-bytes `KWAL`-ramme, blir det liggende $r \in \{1, \dots, 127\}$ ufullstendige byte bakerst på filen etter $k$ fullførte rammer.

`kosmos_gate_cold_boot()` håndterer dette i strengt rekkefølge uten å slette eller overskrive én eneste eksisterende byte:

1. **Full avspilling av gyldig prefiks først:** Alle $k = \lfloor \text{filstørrelse} / 128 \rfloor$ helskrevne blokker verifiseres mot SHA-256 og spilles inn i `gate->history` **før** halen behandles. Dermed bevares både `last_nonce`, historikkprefikset og eventuell uavklart `OPEN_AUTHORIZED`-tilstand (`unresolved_latched = 1`) i minnet.
2. **Forover-polstring og `KTRN`-sertifikat (`version = 2`):** De manglende $128 - r$ bytene i den revne blokken fylles ut forover med `0xEE`, og en ny 128-bytes karantenepost (`magic = "KTRN"`, `version = 2`, `symbol = EVT_HARD_KILL`, `payload_hash = SHA256(polstret_revnet_blokk)`) kjedet til `head_hash` tvinges til disk med `fsync()`.
3. **Vern mot reprise etter gjenoppretting:** Porten starter i `KOSMOS_STATE_KILL` (`KOSMOS_ERR_TAMPER`). Fordi `last_nonce` fra prefikset er bevart i både RAM og `KTRN`-rammen, vil ethvert forsøk på å gjenbruke en eldre `auth_nonce` etter signert `Level 01`-gjenoppretting umiddelbart utløse ny `KILL` (`KOSMOS_ERR_REPLAY`).

---

## 2. Klebrig `unresolved_latched` tvers gjennom `KILL`

En linjefeil eller et grensebrudd (`KILL`) må aldri kunne brukes til å viske ut spor etter en tidligere påbegynt handling som fortsatt mangler `W_POST`.

Både C11-kjernen (`CORE/kosmos_gate.c`, `CORE/kosmos_wal.c`) og RTL-modulen (`RTL/kosmos_dual_rail_gate.sv`) holder derfor en separat klebrig lås `unresolved_latched` sammen med `pending_pre_hash` / `pending_tag_q`:

$$\text{KILL med } (\text{unresolved\_latched} = 1) \xrightarrow{\text{L01-signatur \#1}} \text{UNRESOLVED} \xrightarrow{\text{L01-signatur \#2}} \text{HOLD}$$

* Når en `00`/`11`-feil på dual-rail-bussen utløser `STATE_KILL` mens porten står i `STATE_UNRESOLVED` eller `STATE_IN_FLIGHT`, beholdes både `nvm_out_unresolved = 1` og `nvm_out_pending_tag` uendret i FRAM-skyggen.
* Første `Level 01`-signatur kvitterer kun ut `STATE_KILL` ned til `STATE_UNRESOLVED`; en ny, unik signatur bundet til det oppdaterte `head_hash` kreves for å kvittere ut den uavklarte handlingen til `STATE_HOLD`.

---

## 3. Bekreftet `WAL-før-puls` i maskinvare

`RTL/kosmos_dual_rail_gate.sv` og `RTL/kosmos_v03_physics_gate.sv` holder aktuatorutgangen av mens FRAM lagrer forhåndsposten.

1. **T1 (forhåndslås):** Gaten presenterer `IN_FLIGHT`, `unresolved = 1`, nonce, historikklengde, digest og tag på `nvm_out_*`. Utgangen er av.
2. **Vent på FRAM:** Minnekontrolleren presenterer en gyldig `nvm_in_*`-post først etter varig lagring. Gaten venter så lenge posten mangler eller et felt avviker. Minnekontrolleren må holde posten stabil over samplingsflanken.
3. **T2 (utløsning):** Når gaten sampler en gyldig, identisk post, utløser den én klokkesyklus med `act_enable_p = 1` og `verdict_code = 0xA5`. Crowbar har fortsatt prioritet.

Ved strømbrudd før T2 har utgangen vært av. Ved strømbrudd under T2 finnes forhåndsposten allerede i FRAM, og oppstart med denne posten gir `STATE_UNRESOLVED` med utgangen av.
