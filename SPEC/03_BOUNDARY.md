# 03 — Ytterveggen og $L^\infty\text{-boksen}$ ($\mathcal{A}$)

## 1. Hvorfor $L^\infty$ og ikke $L^2$

En $L^2\text{-norm}$ summerer kvadrater og tillater at én livsfarlig enkeltverdi kompenseres av nullavvik i nabokanalene. Sikkerhets- og kontrollrommet i **KOSMOS** bruker utelukkende $L^\infty\text{-bokser}$ (komplette Banach-rom i det kontinuerlige idealbildet), der **hver enkelt kanal** må holde seg innenfor sin egen harde toleranse $\tau_i > 0$:

$$\mathcal{A}(r, \tau) = \bigl\{x \in \mathbb{Z}_M^m : \forall i \in \{1, \dots, m\},\; \lvert x_i - r_i \rvert \le \tau_i\bigr\}$$

Én eneste overskridelse $\lvert x_k - r_k \rvert > \tau_k$ feller hele vurderingen momentant.

---

## 2. Divisjonsfri, overflytssikker heltallssjekk

En naiv normalisering $\lvert x_i - r_i \rvert / \tau_i \le 1$ bryter sammen på to måter: flyttallsdivisjon bryter `0 float`-kravet, mens heltallsdivisjon kutter desimaler slik at `15 / 10 == 1` slipper gjennom til tross for $50\,\%$ overlast. Og en direkte subtraksjon `x_i - r_i` i `int64_t` gir signert overflyt når $x_i$ og $r_i$ har motsatt fortegn nær $\pm 2^{63}$.

`CORE/kosmos_boundary.c` løser begge problemene eksakt ved å avbilde $\mathbb{Z}_M$ ordensbevarende inn i `uint64_t` via offset-binær flip $\phi(v) = (\text{uint64\_t})v \oplus 2^{63}$:

$$d_i = \max\bigl(\phi(x_i), \phi(r_i)\bigr) - \min\bigl(\phi(x_i), \phi(r_i)\bigr) \in [0, 2^{64}-1]$$

og sjekke direkte $d_i \le (\text{uint64\_t})\tau_i$ etter å ha bekreftet $\tau_i > 0$ og fravær av `KOSMOS_POISON_MIN`. Ugyldig boks ($\tau_i \le 0$ eller `count == 0`) gir alltid fail-closed avslag.

---

## 3. To lag av grenser: Kandidat-avslag mot Anleggs-`KILL`

**KOSMOS** skiller strengt mellom to grensebokser:

1. **Kandidatboks ($\mathcal{A}_{\text{cand}}$):** Prøver et foreslått utkast $(q_{\text{cand}}, p_{\text{cand}}, u_{\text{cand}})$ inne i `HOLD`. Brudd her avviser kandidaten og holder porten i `HOLD` med $u_{\text{out}} = 0$, uten å sprenge hovedlåsen.
2. **Hard anleggsboks ($\mathcal{A}_{\text{hard}}$):** Prøver den målte tilstanden og absolutte sikkerhetsskranker. Brudd her utløser låst `KILL` (`0x00`, $u_{\text{out}} = 0$) i samme klokkeslag.

For fysisk bruk utenfor `L1`-modellen kreves i tillegg dokumentert samplingsbåndbredde og uavhengig maskinvarevern for å fange analoge topper mellom klokkeslagene.
