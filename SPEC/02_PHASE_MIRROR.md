# 02 — Fasespeilet ($J$)

## 1. Antisymplektisk involusjon

Inne i hvert historiske fiber defineres fasespeilet $J : T^*Q \to T^*Q$ ved tidsreversering av impulsen:

$$J(q, p) = (q, -p)$$

Speilet oppfyller to eksakte algebraiske lover:

1. **Involusjon:**
   $$J\bigl(J(q, p)\bigr) = J(q, -p) = (q, p) \implies J^2 = I$$
2. **Antisymplektisk tilbakehenting:**
   $$(J^*\omega)\bigl(\delta z^{(1)}, \delta z^{(2)}\bigr) = \omega\bigl(J\,\delta z^{(1)}, J\,\delta z^{(2)}\bigr) = \sum_{i=1}^{3} \Bigl(\delta q_i^{(1)}(-\delta p_i^{(2)}) - (-\delta p_i^{(1)})\delta q_i^{(2)}\Bigr) = -\omega\bigl(\delta z^{(1)}, \delta z^{(2)}\bigr)$$

Dermed snur speilet orienteringen på $\omega$ ($J^*\omega = -\omega$), samtidig som det absoluttbevarer tre-volumet $\lvert \omega \wedge \omega \wedge \omega \rvert$.

---

## 2. Hvorfor `KOSMOS_POISON_MIN` er påkrevd i maskinvare og C11

I standard 64-bits toerkomplement er intervallet $[-2^{63}, 2^{63}-1]$ skjevt med én enhet: verdien $p_i = -2^{63}$ har ingen positiv representant $+2^{63}$. Uttrykket `-p_i` gir signert heltallsoverflyt (udefinert oppførsel i C11) og returnerer i maskinvare seg selv (`0x8000000000000000`), som ville skapt et falskt fikspunkt der $p_i \neq 0$ men $J(q, p) = (q, p)$.

I `CORE/kosmos_mirror.c` og `RTL/kosmos_gate.sv` avvises enhver innverdi lik minimums-sentinellen (`INT64_MIN` i C11, `32'sh8000_0000` i RTL) med umiddelbar feilkode før negasjon utføres. På hele det lovlige gitteret $[-M, +M]^6$ holder dermed $J(J(z)) = z$ og $J^*\omega = -\omega$ bit-for-bit uten unntak.

---

## 3. Bevisgrense for reversibilitet

At $(h, q, p, u, s) \leftrightarrow (h, q, -p, u, s)$ er eksakt reversibel inne i modellen betyr at **historikken $h$ ikke endres** når en kandidat speiles i `HOLD`. Det påstår ikke at selve prosessoren arbeider uten elektrisk varmeutvikling.
