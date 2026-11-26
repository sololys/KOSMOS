# 09 — Tredimensjonal cellekalkyle og Faraday–Lenz-lås (`v0.3`)

Denne spesifikasjonen løfter **Cellular space** inn fra reservelisten og gir `KOSMOS` romlige differensialoperatorer (`grad`, `curl`, `div`, `laplacian`) i eksakte heltall uten sjakkbrett-blindflekk og uten divisjon.

---

## 1. Hvorfor operatorene bor på ulike deler av 3D-cellen

Plasseres både skalarfelt og vektorfelt i samme gitterpunkt med sentraldifferanse $(f_{i+1} - f_{i-1}) / 2h$, får et alternerende sjakkbrettmønster $+A, -A, +A, -A$ null i derivert og sklir usett gjennom kontrollen.

På et forskjøvet tredimensjonalt cellegitter $C^0 \xrightarrow{D_0} C^1 \xrightarrow{D_1} C^2 \xrightarrow{D_2} C^3$ bor hver størrelse på sitt geometriske element:

* **$0\text{-celler}$ (hjørner):** Skalarfelt $f_{x,y,z} \in \mathbb{Z}_M$ ($\binom{3}{0} = 1$ verdi per celle).
* **$1\text{-celler}$ (kanter):** Diskret **gradient** $(D_0 f)$ langs kantene $+x, +y, +z$ ($\binom{3}{1} = 3$ komponenter):
  $$(D_0 f)_x = f_{x+1,y,z} - f_{x,y,z}, \quad (D_0 f)_y = f_{x,y+1,z} - f_{x,y,z}, \quad (D_0 f)_z = f_{x,y,z+1} - f_{x,y,z}$$
* **$2\text{-celler}$ (flater):** Diskret **curl** $(D_1 e)$ som omløpssum rundt sideflatene $yz, zx, xy$ ($\binom{3}{2} = 3$ komponenter, som kun sammenfaller med kantdimensjonen i $n = 3$ romdimensjoner):
  $$(D_1 e)_x = (e_{z,x,y+1,z} - e_{z,x,y,z}) - (e_{y,x,y,z+1} - e_{y,x,y,z})$$
  og syklisk for $y$ og $z$.
* **$3\text{-celler}$ (volum):** Diskret **divergens** $(D_2 b)$ ut gjennom klossens seks sideflater ($\binom{3}{3} = 1$ verdi):
  $$(D_2 b)_{x,y,z} = (b_{x,x+1,y,z} - b_{x,x,y,z}) + (b_{y,x,y+1,z} - b_{y,x,y,z}) + (b_{z,x,y,z+1} - b_{z,x,y,z})$$

---

## 2. Eksakte null-identiteter og heltalls-Laplacian

Fordi operatorene kun bruker fortegnene $\{-1, 0, +1\}$ uten divisjon, holder begge de klassiske kjedeidentitetene **bit-eksakt** på alle gyldige heltallsfelt:

$$D_1(D_0 f) \equiv 0 \quad (\mathrm{curl}\,\mathrm{grad} = 0), \qquad D_2(D_1 e) \equiv 0 \quad (\mathrm{div}\,\mathrm{curl} = 0)$$

Den diskrete 7-punkts **Laplacianen** på hjørnene er:

$$(\Delta_{\mathbb{Z}} f)_{x,y,z} = \bigl(f_{x+1,y,z} + f_{x-1,y,z} + f_{x,y+1,z} + f_{x,y-1,z} + f_{x,y,z+1} + f_{x,y,z-1}\bigr) - 6 f_{x,y,z}$$

der $\Delta_{\mathbb{Z}} f > 0$ markerer bunnen av en lokal skål, $\Delta_{\mathbb{Z}} f < 0$ en topp, og $\Delta_{\mathbb{Z}} f = 0$ en harmonisk salflate.

---

## 3. Faraday–Lenz-fortegnslås

På hver $2\text{-celle}$ er Faradays induksjonslov $(D_1 E) = -\Delta B$. For en målt fluksendring $\Delta \Phi_B$ og indusert spenning $\mathcal{E}$ med tak $\mathcal{E}_{\max} > 0$ kreves Lenz-motstand ($\mathcal{E} \cdot \Delta \Phi_B \le 0$):

* Samme fortegn ($\mathcal{E} > 0 \land \Delta \Phi_B > 0$ eller $\mathcal{E} < 0 \land \Delta \Phi_B < 0$) bryter energibevaring og gir umiddelbar **`KILL` (`0x00`)**.
* Overskredet spenningstak $\lvert \mathcal{E} \rvert > \mathcal{E}_{\max}$ gir **`TRAP` (`0x5A`)**.
