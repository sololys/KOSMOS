# 07 — Kanaler, tidsvindu og energibudsjett

Dette er en digital modell. Måling av flytid, kutt og varme hører til L3 HOLD.

- Kanaler har ID 0–999. Et statisk, enkelt, urettet nabokart godtas når alle
  grader er høyst den oppgitte grensen d_max (0–999). Selvkanter og doble
  kanter avvises. Kartet brukes ved innlasting. Ved kjøring er TRAP en direkte
  indeksert kanaltilstand med O(1) oppslag og oppdatering. En TRAP endrer bare
  kanalen som er adressert, også når den har naboer. Kun en global hardfeil
  setter den felles KILL-låsen.
- Det oppgitte, kalibrerte tidsvinduet bruker ikke-negative heltall i ps.
  Endepunktene er med. Et omvendt vindu eller en prøve utenfor vinduet
  avvises til HOLD før PRE. Et godkjent tidspunkt er ett nødvendig vilkår.
- Hjørnevakten summerer de seks absolutte avvikene fra referansen eksakt og
  krever at summen er høyst den oppgitte grensen. L-infinity-vakten krever
  samtidig at hvert enkelt avvik er høyst sin positive toleranse. Eksakt
  summersammenligning har ingen avkorting nær UINT64_MAX. Akkumulatoren er
  128 bit og har over 40 ledige vaktbiter ved det største mulige avviket.
- Hver registrert grenkapping teller én gang ved digital OPEN. Prøven
  oppgir separat hvor mange logiske biter som faktisk skal slettes. Summene
  for kappinger og bitantall har hver sin uint64-grense. For oppgitt konstant
  temperatur T i millikelvin er den formelle Landauer-nedre grensen
  Q_min = N_bits * (1380649/10^29) * (T/1000) * ln(2) joule.
  Uttrykket lagres som heltallsfaktorer med ln(2) beholdt symbolsk. En
  kappet fremtidsgren står for et valg i historikkmodellen. Det kreves et
  eget slettingsregnskap for å knytte kappingen til et fysisk bitantall.
- Frisk start har null tellere, tom karantene og HOLD. Gjenvinning ved
  kaldstart og varig journallagring er fortsatt åpne L3-spørsmål.

Grenseverdier for digital paritet er 0, 999, 1000 for kanal, begge ender
av tidsvinduet, samt nær UINT64_MAX for hjørnesum og tellere.
