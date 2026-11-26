# 06 — Diskret historikk ($B = A^*$) og Cantor-innsnevring

## 1. Enveis prefikslov

All irreversibilitet i **KOSMOS** ligger i oppdateringsloven på historikkrommet $B = A^*$:

$$h_{n+1} = h_n \parallel e_n, \qquad e_n \in A \implies h_n \prec h_{n+1}$$

* Ingen sletting (`pop`), ingen overskriving av eldre indeks, og ingen tilbakestilling av lengden $\lvert h \rvert$.
* Selv når en `Level 01`-operatør låser opp porten etter `KILL` eller `UNRESOLVED`, fjernes aldri feilhendelsen; gjenopprettingen legges til som en ny hendelse $e_{n} = \text{EVT\_L01\_RECOVERY}$ etter feilen.

---

## 2. Innsnevring av Cantor-sylinderen ($C_{h \parallel e} \subsetneq C_h$)

For to fortsettelser $s, t \in S = A^{\mathbb{N}}$ er den felles prefikslengden $\ell(s, t) \in \mathbb{N}_0 \cup \{\infty\}$ gitt ved første indeks der $s_k \neq t_k$. Ultrametrikken $d_S(s, t) = 2^{-\ell(s, t)}$ avgjøres i `CORE/kosmos_history.c` helt uten flyttall:

$$d_S(s, t) < d_S(s, r) \iff \ell(s, t) > \ell(s, r)$$

Når en ny hendelse $e_n \in A$ føyes til historikken $h_n$, snevres mengden av kompatible fremtidsbaner strengt inn:

$$C_{h_n \parallel e_n} \subsetneq C_{h_n}$$

fordi $\lvert A \rvert = 8 \ge 2$ sikrer at minst sju søstersylindere $C_{h_n \parallel b}$ ($b \neq e_n$) kuttes bort ved hvert steg.

---

## 3. Kryptografisk kjedet logg (FIPS 180-4 SHA-256)

Hver post $k$ i `kosmos_history_t` forsegles over sin forgjenger med eksakt SHA-256 (`CORE/kosmos_sha256.c`):

$$H_k = \mathrm{SHA256}\bigl(H_{k-1} \parallel k \parallel \text{cycle}_k \parallel e_k \parallel \text{verdict}_k \parallel \text{witness\_id}_k \parallel \text{payload\_hash}_k\bigr)$$

med rotanker $H_{-1} = 0^{32}$. Funksjonen `kosmos_history_verify_chain()` kontrollerer både sekvensiell indeks, monoton klokkesyklus og hver eneste SHA-256-kobling fra rot til hode. Manipulasjon av én eneste bit i tidligere historikk fanges umiddelbart.
