# 00 — Rommet $E$ og det diskrete kuttet

## 1. Definisjon av kandidatrommet

La $A$ være et endelig hendelsesalfabet med $\lvert A \rvert = 8$ distinkte overgangssymboler:

$$A = \{0, 1, 2, 3, 4, 5, 6, 7\}$$

* **Historisk basisrom ($B = A^*$):** Mengden av alle endelige prefikser $h = (e_0, e_1, \dots, e_{n-1})$, utstyrt med den diskrete, lokalt endelige topologien.
* **Fremtidsrom ($S = A^{\mathbb{N}}$):** Mengden av alle uendelige fortsettelser med produkttopologi (et kompakt, totalt usammenhengende Cantor-rom).
* **Kompatibel fremtidssylinder ($C_h$):** For hvert $h \in B$ er $C_h = \{s \in S : h \preceq s\}$ både åpen og lukket i $S$.
* **Faserom og kontrollrom ($T^*Q \times U$):** Posisjon og impuls $z = (q, p) \in T^*Q$ sammen med kontrollpådrag $u \in U$ utstyrt med $L^\infty\text{-topologi}$.

Det samlede kandidatrommet er den topologiske summen:

$$E = \bigsqcup_{h \in B} \bigl(\{h\} \times T^*Q \times U \times C_h\bigr)$$

og det fulle realiseringsuniverset skiller kandidater fra bekreftet historie som en disjunkt sum:

$$\mathfrak{R}_\Omega = E \sqcup B$$

Siden $B$ er tellbar og hver faktor $\{h\} \times T^*Q \times U \times C_h$ i det kontinuerlige idealbildet er et produkt av Polish-rom, er $E$ et Polish-rom og spesielt Hausdorff ($T_2$).

---

## 2. Kosmos-teoremet (Topologisk umulighet av snikovergang)

La projeksjonen $\pi : E \to B$ være gitt ved $\pi(h, q, p, u, s) = h$.

**Teorem:** For enhver kontinuerlig bane $\gamma : [0, 1] \to E$ gjelder

$$\pi \circ \gamma = \text{konstant}.$$

**Bevis:** Enhetsintervallet $[0, 1]$ er sammenhengende, og sammensetningen $\pi \circ \gamma : [0, 1] \to B$ er kontinuerlig fordi $E$ bærer sumtopologien over $B$. Bildet $(\pi \circ \gamma)([0, 1])$ er derfor en ikke-tom sammenhengende delmengde av det diskrete rommet $B$. De eneste ikke-tomme sammenhengende delmengdene av et diskret rom er ettpunktsmengdene $\{h\}$. Dermed er $\pi(\gamma(t)) = \pi(\gamma(0))$ for alle $t \in [0, 1]$. $\blacksquare$

**Arkitektonisk konsekvens:** Ingen kontinuerlig bevegelse, optimalisering eller speiling inne i `HOLD` kan flytte tilstanden til et nytt historisk prefiks. Enhver historisk endring må utføres som en eksplisitt diskret overgang gjennom slusen $\Omega$.
