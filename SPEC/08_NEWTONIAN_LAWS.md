# 08 — Tre mekaniske lover som digitale overgangsregler

Treghet. Uten godkjent W_PRE er projeksjonen pi o gamma konstant, og den
digitale kommandoen er null. OPEN gir én digital tillatelse. Historikken
bruker rotkjernens PRE/POST-kontrakt for å skille tillatelse fra realisering.
Modulen mottar resultatet av en W_PRE-kontroll. Den autentiserer ingen vitner.

Kraft. På hver av tre akser er p_next = p + F * n_clock. Gjeldende p, F
og p_next tilhører [-M,+M], M = 2^63 - 1. n_clock er et uint64-tall.
All kontroll bruker heltall. Gyldig kraft krever også tidsvinduet i Del 07.

Motkraft. F1 + F2 = 0 per akse, med F = F1. INT64_MIN i en fortegnsverdi
er en hard feil. Poeng og referanser har seks signerte koordinater. De
eksakte avvikene mot de usignerte grensene følger Del 07. En gyldig
dual-rail-kode har 1 av 2 skinner høy for hver av 64 biter og dermed
Hamming-vekt 64. Komplementbrudd, poison, motkraftbrudd, moment utenfor
gitteret og romgrensebrudd gir KILL. KILL nullstiller digital kommando
og blir stående til en autorisert Level 01-reset og samtidig frisk hardvakt.
En reset gir HOLD og autoriserer aldri et forslag i samme steg.

## Felles digital observasjon

En transaksjon bærer kanal, lokal trap-markering, godkjent W_PRE, TDC-prøve
med tidsvindu, to skinner, p og to kraftvektorer, klokketall, de seks
punkt-/referanseparene med toleranser og hjørnegrense, slettet bitantall,
slettebitbudsjett og autorisert reset. Tall i grense- og klokkegrensesnitt
er uint64. Kanal er usignert 16-bit. Flaggenes verdier er 0 og 1.

Hver transaksjon evalueres i denne rekkefølgen:

1. Hardfeil (motkraft, poison, rail, moment og begge romvaktene) setter KILL.
2. Ved eksisterende KILL kvitterer frisk, autorisert reset til HOLD. Andre
   transaksjoner gir KILL. En reset uten aktiv KILL gir også HOLD.
3. Ugyldig kanal gir HOLD. Lokal TRAP setter bare den adresserte kanalen
   i karantene. En kanal i karantene gir TRAP.
4. Manglende W_PRE, ugyldig TDC-prøve, kappe-teller som ville overflyte,
   eller slettesum utenfor bitbudsjettet gir HOLD.
5. Ellers gir transaksjonen OPEN, p_next, én ekstra kapping og den oppgitte
   bitmengden lagt til akkumulert slettet bitantall.

Domkodene er HOLD=0, OPEN=1, TRAP=2, KILL=3. I andre dommer enn OPEN er
kommandoen tre signerte nuller. Observasjoner er dom, de tre
kommandokoordinatene, kappe-teller og slettet bitantall. Kanaltilstand og
KILL-lås forblir i samme instans mellom transaksjoner. En digital OPEN i
denne påbyggsvakten er innspill til rotkjerneporten. Den styrer ingen
fysisk utgang og har ingen L3-status.
