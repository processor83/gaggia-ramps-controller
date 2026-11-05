# Changelog — Gaggia Classic Pro PID (RAMPS 1.4)

Historique des versions du firmware `gaggia-ramps.ino`. Chaque modification
du firmware est publiée ici avant (ou avec) le push sur GitHub.

Le format s'inspire de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/).

## [Non publié]

- Photos des pièces ajoutées au tableau « Liste des pièces » du README.
- Correction BOM en attente (non appliquée) : le capteur XDB401 est à
  19,28 $ sur AliExpress en vente 11.11 jusqu'au 18 nov 2026, contre
  29,37 $ au BOM — le total obligatoire (~62 $ CAD) serait ~10 $ plus bas
  à ce prix. À appliquer seulement après décision.

## [0.1.0] — 2026-11-05

Version initiale publiée sur GitHub. Firmware compilé (4 826 o de SRAM,
~59 % du Mega ; 47,0 Ko de flash). Matériel : Arduino Mega 2560 + RAMPS 1.4 + LCD
graphique 128x64 ST7920, 2 SSR, 2 thermistors 100k NTC, capteur de
pression XDB401 0–1,2 MPa G1/4 0,5–4,5 V.

### Ajouté

- 5 profils (nom + temp brew + pression cible chacun), sauvés en EEPROM ;
  température vapeur globale (145 °C) et PID global (un seul jeu Kp/Ki/Kd).
- Architecture 2 SSR + 1 dimmer RobotDyn : chauffage SSR-40DA (D8),
  solénoïde 3-way seul SSR-25DA (D9), pompe sur dimmer (ZC D3, gate D11)
  — la pression cible du profil devient réelle via un 2e PID (15/1/0).
- Pré-infusion par profil (3 bar, 5 s, 0 = off), rampe (3 s) et déclin
  (6 bar) ; pression cible par profil (9 bar par défaut).
- Arrêt auto du shot, mode global au menu : Manuel, Minuteur (27 s),
  Débitmètre (40 ml), Balance (36 g).
- Mode grammes avec anticipation apprise (tau, EMA) : coupure à
  `cible − tau × g/s`, tau mis à jour seulement après un shot normal.
- Débitmètre = diagnostic permanent (débit mL/s + volume total) ;
  arrêt si plus aucun débit pendant 5 s d'extraction.
- Autotune PID (relais + Ziegler-Nichols), annulable d'un clic.
- Historique des 10 derniers shots en EEPROM + courbe de pression du
  dernier shot au LCD ; gains du PID pression et tau réglables au menu.
- Écran de shot (pression, pompe %, débit, eau, tasse + cible, temps,
  phase) et résumé après shot (poids final, overshoot, tau).
- Sécurités : 12,5 bar max (FAULT), 90 s max, « BALANCE HS »,
  « PAS DE DEBIT » ; sonde chaudière HS ou > 165 °C → chauffage coupé.
- Diagnostics puck restrictif / permissif en avertissement seulement.
- Logs série CSV (115200 baud) pour tuner sur données réelles.

### Notes

- Débitmètre et load cell non achetés : les modes Débitmètre / Balance
  ne s'activent qu'avec le matériel installé.
- Le dimmer RobotDyn d'origine est discontinué : seuls des clones
  existent (7–10 $ US), fonctionnellement identiques.
- BOM : ~62 $ CAD obligatoire (+ TPS ≈ 65 $), ~67 $ avec optionnels
  (voir `BOM.md`, prix AliExpress du 5 nov 2026).
