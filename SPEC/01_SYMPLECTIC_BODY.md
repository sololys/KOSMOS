# 01 — Den symplektiske kroppen ($T^*Q, \omega$)

## 1. Tredimensjonalt affint gulv ($Q$)

Konfigurasjonsrommet $Q$ er et tredimensjonalt affint rom modellert over det euklidske forskyvningsrommet $V \cong \mathbb{R}^3$:

* To posisjoner $q_1, q_2 \in Q$ kan **ikke** legges sammen; bare differansen $\Delta q = q_2 - q_1 \in V$ er en veldefinert forskyvningsvektor.
* En posisjon $q \in Q$ kan flyttes langs en vektor $v \in V$ til $q' = q + v \in Q$.

Dette fjerner ethvert privilegert origo på gulvet. I C11-kjernen (`CORE/kosmos_space.h`) håndheves dette gjennom egne typer `kosmos_point3_t` (posisjon) og `kosmos_vec3_t` (forskyvning).

---

## 2. Kotangentbunten og den kanoniske 2-formen

Over hvert punkt $q \in Q$ henger impuls- og spenningsfiberen $T_q^*Q$. Sammen utgjør $z = (q, p)$ det seksdimensjonale faserommet $M = T^*Q$ med den kanoniske symplektiske 2-formen:

$$\omega = \sum_{i=1}^{3} dq_i \wedge dp_i$$

Evaluert på to tangentforskyvninger $\delta z^{(1)} = (\delta q^{(1)}, \delta p^{(1)})$ og $\delta z^{(2)} = (\delta q^{(2)}, \delta p^{(2)})$ gir formen det bilineære, skjevsymmetriske paringsuttrykket:

$$\omega\bigl(\delta z^{(1)}, \delta z^{(2)}\bigr) = \sum_{i=1}^{3} \Bigl(\delta q_i^{(1)} \delta p_i^{(2)} - \delta p_i^{(1)} \delta q_i^{(2)}\Bigr)$$

Under Hamiltonsk flyt i modellen bevares fasevolumet $\omega \wedge \omega \wedge \omega$ (Liouvilles teorem).

---

## 3. Heltallsrepresentasjon uten avrunding (`0 float`)

I den kjørbare kjernen diskretiseres hver koordinat i faste heltallsenheter (eksempelvis nanometre og mikronewtonsekunder) over det symmetriske intervallet:

$$\mathbb{Z}_M = [-M, +M], \qquad M = 2^{63} - 1$$

Toerkomplementets asymmetriske bunnverdi `INT64_MIN` ($-2^{63}$) er utelukket som `KOSMOS_POISON_MIN`. Den bilineære formen $\omega(\delta z^{(1)}, \delta z^{(2)})$ akkumuleres eksakt i en portabel 128-bits heltallstype (`kosmos_i128_t`) uten bruk av kompilatorspesifikke utvidelser eller flyttall.
