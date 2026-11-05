# Gaggia Classic Pro — PID sur RAMPS 1.4 + Arduino Mega

> ⚠️ **WORK IN PROGRESS — projet en cours.** Le firmware et cette doc
> évoluent encore : chaque modification est publiée avec son entrée dans
> [`CHANGELOG.md`](CHANGELOG.md). Ne branche rien sous 120 V sans avoir
> relu la section Câblage et les sécurités ci-dessous.

Contrôleur PID + pression pour ta Classic Pro 2021, avec le hardware
d'imprimante 3D que tu as déjà. Firmware : `gaggia-ramps.ino`.

## Verdict RAM

Oui, ça fit large. Le buffer du LCD graphique 128x64 = 1024 o.
Le Mega a 8192 o de SRAM : **compilé = 4 826 o utilisés (~59 %)**,
47,0 Ko de flash sur 258 Ko. Le Nano (2048 o) aurait été juste —
le Mega, aucun problème.

## Ce que ça fait

- **5 PROFILS** (nom + temp brew + pression cible chacun), sauvegardés
  en EEPROM. Défauts : P1 Espresso 93 °C, P2 Ristretto 94 °C,
  P3 Lungo 92 °C, P4 Decaf 91 °C, P5 Doux 90 °C, pression cible 9 bar.
  **La temp vapeur est GLOBALE (145 °C)** — la même pour tous les cafés.
  **Le PID est GLOBAL** (hardware : chaudière + élément) — un seul jeu
  de Kp/Ki/Kd pour toute la machine, pas un par profil.
- **Pression cible par profil** — pour l'instant cible d'affichage
  seulement : sans dimmer, la pression réelle est fixée par l'OPV
  mécanique (autour de 12 bar d'origine sur la Pro, réglable à 9 bar
  avec la vis). Avec le dimmer (phase 2), la même cible pilotera la pompe.
- **Écran principal : TOURNER = changer de profil, CLIQUER = menu config.**
- **Cibles + live à l'écran** : temp chaudière live (gros) + consigne,
  pression live (gros) + barre graphique, temp groupe, timer de shot.
- **Menu config** : choisir le profil à éditer, nom éditable lettre par
  lettre à la molette, temp brew, pression cible, vapeur (globale),
  Kp/Ki/Kd, autotune, sauver.
- **AUTOTUNE PID** (relais + Ziegler-Nichols) : règle le PID global tout
  seul, ~3 cycles d'oscillation autour de la consigne (~5-10 min),
  annulable d'un clic. Résultat sauvé en global.
- **PID chaudière** : consigne brew / vapeur, réglables au menu.
- **Architecture « la crème » : 2 SSR + 1 DIMMER.** Chauffage sur SSR-40DA
  (D8), solénoïde 3-way SEUL sur SSR-25DA (D9), pompe sur dimmer RobotDyn
  (ZC = D3, gate = D11). **La pression cible du profil est RÉELLE** : un
  2e PID pilote le dimmer en boucle fermée sur le capteur XDB401.
- **Pré-infusion par profil** : pression (3 bar) + durée (5 s, 0 = off),
  réglables au menu. Le solénoïde reste ouvert pendant toute la
  pré-infusion, seule la pompe tourne à basse puissance. Capteur de
  pression HS → pleine pompe (comportement d'avant).
- **Profilage complet par profil** : pré-infusion → **rampe** (3 s, 0 =
  directe) → extraction (pression cible) → **déclin** (6 bar à partir
  de X s d'extraction, 0 = off). Consigne interpolée pendant la rampe.
- **Débitmètre = diagnostic permanent** (jamais la référence de dose) :
  débit instantané (mL/s, fenêtre glissante 1 s) + volume total d'eau,
  affichés pendant le shot. Arrêt si plus aucun débit pendant 5 s
  d'extraction (débitmètre présent). La dose en tasse, c'est la load cell.
- **Mode grammes = référence de dose, avec anticipation apprise.**
  La cible est le poids FINAL dans la tasse. Le contrôleur apprend
  **tau** (secondes d'écoulement) par profil, EMA alpha 0,2 :
  `cutoff = cible - tau × g/s` (g/s mesurés sur 2 s glissantes).
  Après le shot, stabilisation de la balance (8 s max), puis tau est
  mis à jour et sauvé en EEPROM. Après quelques shots, une cible de
  36,0 g donne un poids final très proche de 36,0 g.
- **Écran de shot** : pression, pompe %, débit mL/s, eau totale mL,
  tasse g + cible, temps, phase (PREINF / RAMPE / BREW / DECLIN / STAB.).
  **Résumé après shot** : poids final, overshoot, tau appris (mode
  grammes) ou Pmax/Pmoy et températures (autres modes).
- **Sécurités** : pression max 12,5 bar (FAULT, chauffage + pompe coupés),
  temps max 90 s (tous modes, manuel inclus), perte du signal balance
  (mode grammes → arrêt « BALANCE HS »), pas de débit 5 s (arrêt
  « PAS DE DEBIT »).
- **Tout se règle sur la machine** (pas de SD, pas d'USB, pas de WiFi) :
  gains du PID pression (P-Kp/P-Ki/P-Kd) et tau éditables au menu ;
  historique des 10 derniers shots consultable au LCD ; courbe de
  pression du dernier shot affichée à l'écran.
- **Arrêt auto du shot** (menu, global) : Manuel / Minuteur / Débitmètre /
  Balance. Le profil porte la cible du mode configuré : durée (27 s),
  volume (40 ml) ou poids (36 g). Écran en direct : `12/27s`, `18/40ml`,
  `12/36g`. Sécurité 60 s sur les modes capteur.
- **Boutons d'origine réutilisés** : BREW = extraction, STEAM = mode vapeur.
- **Sécurités** : sonde chaudière HS ou > 165 °C → chauffage coupé + écran FAULT.
  Le fusible thermique d'origine reste en série avec l'élément : jamais le bypasser.
- **Sauvegarde** : profils + profil actif gardés en EEPROM.

## Liste des pièces

| Pièce | Détail | Tu l'as ? |
|---|---|---|
| Arduino Mega 2560 + RAMPS 1.4 | de ton imprimante | ✅ |
| LCD graphique 128x64 ST7920 | RepRapDiscount Full Graphic Smart Controller (souvent fourni avec le kit RAMPS) | ✅ si dans ton kit |
| Alim 12 V, 2 A+ | de ton imprimante (elle alimente le RAMPS + Mega + SSR) | ✅ |
| SSR-40DA + dissipateur | élément chauffant (~12 A à 120 V) | à acheter |
| SSR-25DA | solénoïde 3-way SEUL (~0,5 A) | à acheter |
| Dimmer AC RobotDyn 1 canal (avec zero-cross, 110 V) | pompe — contrôle de phase, c'est lui qui règle la pression | à acheter |
| 2× thermistor 100k NTC, beta 3950, **à cosse annulaire (œillet M3/M4)** | un sur la chaudière (PID), un sur la tête de groupe (affichage) | à acheter |
| Capteur pression **XDB401, 0–1.2 MPa, G1/4, 5 V, 0.5–4.5 V** | le standard Gaggiuino — OBLIGATOIRE avec le dimmer (boucle fermée) | à acheter |
| Té 6 mm + raccord 6 mm barb × 1/4" + bout de tuyau HP + colliers | pour brancher le capteur sur le tuyau pompe → chaudière | à acheter |
| Fil, cosses, gaine thermo, ruban PTFE | | à acheter |

**Le 3e thermistor (T2) est optionnel** : la vapeur se lit sur la sonde
chaudière, la consigne change juste. Deux thermistors suffisent.

**Le SSR de pompe n'existe plus** : avec le dimmer, la pompe n'a pas de
SSR — le dimmer coupe le 120 V directement (contrôle de phase). Les deux
SSR servent au chauffage et au solénoïde.

## Le capteur de pression — le choix

**XDB401, 0–1.2 MPa (0–12 bar), filetage G1/4 mâle, alim 5 V,
sortie 0.5–4.5 V, 3 fils : rouge = 5 V, noir = GND, jaune = signal.**
C'est le capteur du projet Gaggiuino : facile à trouver (~15–25 $ sur
AliExpress/eBay, cherche « XDB401 1.2MPa G1/4 0.5-4.5V »), pas cher,
et il se visse sur un Té standard.

**Installation** : sur le tuyau orange tressé entre la sortie de pompe
et l'entrée chaudière, via un Té 6 mm + raccord 6 mm barb × 1/4".
Le filetage peut arriver en NPT ou en BSP selon le vendeur — vérifie
avant de commander les raccords, et mets du ruban PTFE. Teste les
fuites à froid avant de remettre sous tension : 9 bar, ça pardonne pas.

**Côté logiciel** : 0.5 V = 0 bar, 4.5 V = 12 bar. Si le capteur est
débranché, l'écran affiche `--` mais la machine continue de fonctionner
(la pression est affichage seulement en v1, elle ne bloque rien).

## Câblage (RAMPS 1.4)

| Fonction | Broche RAMPS | Branchement |
|---|---|---|
| SSR élément | D8 (MOSFET lit) | sortie D8 → SSR-40DA (+/−), SSR → élément 120 V (fusible thermique en série, intact) |
| SSR solénoïde | D9 (MOSFET lit 1) | sortie D9 → SSR-25DA, SSR → solénoïde 3-way 120 V SEUL |
| Dimmer pompe | D3 (ZC) + D11 (gate) | RobotDyn : ZC → D3, gate → D11, VCC 5 V, GND ; côté AC : dimmer en série avec la pompe 120 V |
| Thermistor chaudière | T0 (A13) | cosse annulaire vissée sur la chaudière |
| Thermistor groupe | T1 (A14) | cosse annulaire vissée sur la tête de groupe |
| Capteur pression | AUX-2, **A10** | rouge 5 V, noir GND, jaune → A10 |
| Bouton BREW d'origine | D19 | contacts du bouton → D19 + GND (fils 120 V débranchés) |
| Bouton STEAM d'origine | D18 | contacts du bouton → D18 + GND (fils 120 V débranchés) |
| LCD 12864 | EXP1 / EXP2 | comme sur l'imprimante (SPI : MOSI 51, SCK 52, CS 23, RESET 17) |
| Encodeur LCD | EXP1 | A=31, B=33, bouton=35 (standard RAMPS) |

**Les MOSFETs du RAMPS ne commutent que du DC 12/24 V** : ils pilotent
l'entrée DC des SSR (3–32 V), et ce sont les SSR qui coupent le 120 V.
Ne branche jamais le 120 V direct sur le RAMPS.

**Bouton BREW** : le code accepte un interrupteur à verrouillage (le
bouton de la Pro) ET un bouton momentané (chaque pression bascule on/off).

## Flasher

1. Arduino IDE → installer la librairie **U8g2** (Gestionnaire de librairies).
2. Ouvrir `gaggia-ramps.ino`, carte **Arduino Mega 2560**, bon port COM.
3. Brancher le Mega en USB (RAMPS alimenté en 12 V), téléverser.
4. Si l'écran reste vide : règle le potentiomètre de contraste au dos
   du LCD. En dernier recours, la version logicielle du constructeur
   U8g2 est en commentaire dans le code.

## Utilisation

- Allume : le PID monte à la consigne du profil actif, l'écran dit PRET
  quand c'est stable (±1 °C).
- **Tourner la molette (écran principal) : change de profil (P1→P5).**
- **Cliquer (écran principal) : ouvre le menu config.**
- BREW : extraction, timer + pression en direct. Re-clique pour arrêter
  (ou relâche si ton bouton est à verrouillage).
- STEAM enfoncé : consigne vapeur, vapeur comme d'habitude (le bouton
  de la valve reste manuel).

### Menu config (tourner = sélection, clic = entrer)

| Item | Action |
|---|---|
| Profil | tourner en édition = changer le profil édité (P1–P5) |
| Nom | clic = édition lettre par lettre (tourner = lettre, clic = caractère suivant, après le 10e = retour) |
| Temp brew | 85–100 °C, pas de 0,5 |
| Pression | 6–12 bar, pas de 0,5 (cible du profil) |
| Arrêt auto | **Manuel / Minuteur / Débitmètre / Balance** (mode global, voir ci-dessous) |
| Cible shot | la cible du mode configuré : **Durée** 10–60 s, **Volume** 10–120 ml, **Poids** 10–120 g (manuel = pas de cible) |
| Pré-inf bar | 1–6 bar, pas de 0,5 (pression de pré-infusion du profil) |
| Pré-inf s | 0–10 s, pas de 1 (durée de pré-infusion, 0 = désactivée) |
| Rampe s | 0–10 s, pas de 1 (montée pré-infusion → pression cible, 0 = directe) |
| Déclin bar | 1–12 bar, pas de 0,5 (pression de fin de shot) |
| Déclin s | 0–60 s, pas de 1 (secondes d'extraction avant le déclin, 0 = off) |
| Tau | 0–2,00 s, pas de 0,05 (anticipation du mode grammes, voir ci-dessous) |
| P-Kp / P-Ki / P-Kd | gains du PID pression (15 / 1 / 0 par défaut, voir ci-dessous) |
| Historique | écran : les 10 derniers shots (tourner = naviguer) |
| Courbe | écran : courbe de pression du dernier shot (cible en pointillés) |
| Vapeur (globale) | 130–155 °C, pas de 1 |
| Kp / Ki / Kd | réglage manuel du PID **global** (valable pour tous les profils) |
| Autotune PID | clic = lance l'autotune (voir ci-dessous) |
| Sauver | écrit tout en EEPROM et quitte le menu |

Timeout 10 s sans toucher la molette = sortie du menu sans sauver
(les modifs restent en RAM jusqu'au prochain redémarrage).

### Arrêt auto du shot — 3 modes (menu « Arrêt auto », global)

Le mode se choisit au menu ; le profil montre ensuite la cible du mode
configuré (la « Cible shot » change de libellé et d'unité toute seule).
Pendant le shot, l'écran montre la progression : `12/27s`, `18/40ml`
ou `12/36g`. Appuyer sur BREW pendant un shot auto l'arrête avant la cible.

| Mode | Ce qu'il faut | Comment ça marche |
|---|---|---|
| **Manuel** (défaut) | rien | BREW démarre/arrête, c'est toi qui contrôles le volume |
| **Minuteur** | rien | la pompe s'arrête après la durée du profil (10–60 s) |
| **Débitmètre** | débitmètre hall sur D2 | compte les impulsions, arrête au ml du profil |
| **Balance** | load cell + HX711 (D4/D5) | pèse le café dans la tasse, arrête au gramme du profil |

- **Débitmètre** : signal sur **D2** (interruption INT0, `INPUT_PULLUP` —
  le capteur hall est collecteur ouvert). Constante `FLOW_PULSES_PER_ML`
  (défaut 1,0 imp/ml) **à calibrer** : fais couler 100 ml d'eau (pompe
  seule, sans café), note les impulsions au série, divise par 100.
  Un YF-S201 (~0,45 imp/ml) est trop grossier pour 40 ml — préfère un
  débitmètre espresso (~1–2 imp/ml).
- **Balance** : load cell sous le bec + HX711, **DOUT = D4, SCK = D5**,
  lecture bit-bang (gain 128). Le firmware fait la **tare au démarrage
  du shot** : la tasse doit être en place avant d'appuyer sur BREW.
  Constante `HX711_COUNTS_PER_G` (défaut 100) **à calibrer** : pose un
  poids connu (ex. 100 g), lis le brut au série, divise par le poids.
- **Sécurité** : en modes Débitmètre/Balance, arrêt forcé à **60 s** si
  le capteur ne répond pas (débranché, panne). **N'active pas un mode
  sans son matériel installé.**

### Dimmer + pré-infusion — comment ça marche

Le dimmer coupe chaque demi-vague du 120 V (contrôle de phase) : la pompe
tourne en continu, juste moins fort — la pression tient stable au lieu
d'osciller. Un 2e PID (`pidPress`, Kp=15, Ki=1, Kd=0 par défaut) lit le
capteur XDB401 et ajuste le niveau du dimmer (0–100 %) pour tenir la
cible : pression de pré-infusion pendant la phase pré-infusion, puis
pression cible du profil pendant l'extraction. **Ces gains sont des
valeurs de départ : la pression répond vite, attends-toi à tuner Kp
(plus haut = réaction plus forte, trop haut = oscillation).** Si le
capteur est HS, le firmware met le dimmer à 100 % (pleine pompe).

Déroulement d'un shot : BREW → solénoïde ouvert + pré-infusion à la
pression du profil (défaut 3 bar, 5 s) → rampe vers la pression cible
(défaut 3 s) → extraction à la pression cible (9 bar) → déclin à la
pression de fin (défaut off, ex. 6 bar à partir de 20 s d'extraction) →
arrêt auto (minuteur/ml/grammes) ou BREW → dimmer à 0 + solénoïde
fermé (le groupe se ventile). Le niveau minimum du dimmer est clampé
à 25 % (`DIM_MIN_LEVEL`) : en bas de ça, la pompe vibrante cale.

### Débitmètre — diagnostic, pas dose

Le débitmètre est installé en permanence et mesuré à chaque shot, mais il
ne sert jamais à décider la dose : il mesure le débit d'eau de la pompe.
L'écran montre le débit instantané (mL/s, fenêtre glissante 1 s) et le
volume total d'eau envoyée — utile pour voir si le débit s'effondre en
fin de shot ou si la pompe a un problème. Sécurité : si le débitmètre
est présent (au moins une impulsion vue) et que plus rien ne coule
pendant 5 s d'extraction, le shot s'arrête (« PAS DE DEBIT »). Sans
débitmètre branché, cette sécurité est inactive (D2 tirée au +5 V,
aucune impulsion).

### Mode grammes — anticipation apprise (tau)

La cible est le **poids final dans la tasse**, pas le poids au moment où
la pompe s'arrête : après la coupure, l'eau continue de traverser le
puck. Le contrôleur apprend **tau** (secondes d'écoulement) par profil :

- Pendant le shot : `cutoff = cible - tau × g/s` (g/s = vitesse
  d'augmentation du poids, mesurée sur 2 s glissantes ; anticipation
  bornée à 5 g max). Si le shot coule vite, on coupe plus tôt.
- Après le shot : stabilisation de la balance (8 s max, stable si le
  poids bouge de moins de 0,05 g pendant 1 s). Poids final mesuré →
  `overshoot = final - poids à la coupure` → `tau mesuré = overshoot / g/s`
  → `tau = 0,8 × tau + 0,2 × tau mesuré` (EMA). Tau est sauvé en EEPROM
  à chaque shot. Valeur de départ : 0,4 s.
- La balance fait sa tare au démarrage du shot : **la tasse doit être en
  place avant d'appuyer sur BREW**. Appuyer sur BREW pendant la
  stabilisation repart un shot (la tare sera fausse — attends la fin
  de la stabilisation, max 8 s).
- Le résumé après shot montre le poids final, l'overshoot et le tau
  appris : après quelques shots, 36,0 g visés = 36,0 g obtenus.
- **Protection de l'apprentissage** : tau n'est mis à jour que si le shot
  s'est terminé normalement en mode grammes. Sont exclus : arrêt manuel,
  FAULT, arrêt de sécurité (pas de débit, balance HS, 90 s), balance
  instable (stabilisation timeout), poids final aberrant (overshoot
  < 0 g ou > 4 g). Sinon le résumé affiche « Tau non appris ».

### Débitmètre — diagnostics (avertissement seulement)

En plus du débit instantané et du volume total, le débitmètre sert à
diagnostiquer le puck — **sans jamais arrêter un shot pour ça** (une
mouture très fine donne légitimement un débit faible) :

| Condition (5 s soutenues, en extraction) | Avertissement affiché |
|---|---|
| Pression ≥ 8 bar + débit < 0,5 mL/s | `RESTRICTIF` (puck très restrictif) |
| Débit > 3 mL/s + pression < 4 bar | `PERMISSIF` (puck trop permissif / fuite) |
| Pompe commandée + débit ≈ 0 | `PAS DE DEBIT` → **arrêt** (réservoir vide, bouchon, pompe désamorcée) |

Les avertissements s'affichent sur l'écran de shot (à la place de la
phase) et sur le résumé. Seuils de départ — à tuner avec les logs réels.

### Logs série CSV — pour tuner sur données réelles

**Tout se règle sur la machine, sans rien brancher.** Le LCD est
l'interface principale :

- **Menu → Historique** : les 10 derniers shots, persistés en EEPROM
  (survivent au power cycle). Chaque shot : durée, Pmax/Pmoy, eau
  totale, poids final, overshoot, tau. Tourner = naviguer, clic = retour.
- **Menu → Courbe** : courbe de pression du dernier shot (échantillon
  toutes les 250 ms), avec la cible du profil en pointillés — c'est
  l'écran pour régler le PID pression : dépassement, oscillation,
  temps de montée, tout s'y voit.
- **Menu → P-Kp / P-Ki / P-Kd** : gains du PID pression, éditables
  directement (sauvés avec « Sauver »). Point de départ : Kp=15, Ki=1,
  Kd=0. La pression répond vite : monte Kp si la pression monte trop
  lentement, baisse-le si elle oscille ; un peu de Ki pour corriger
  l'erreur résiduelle.
- **Menu → Tau** : voir et forcer le tau appris du profil actif
  (0–2,00 s). L'apprentissage automatique continue de le mettre à jour
  après chaque shot normal.

Le log série CSV reste disponible en option (brancher l'USB, moniteur
à 115200 baud) : une ligne toutes les 250 ms pendant le shot, sinon
1/s. En-tête au démarrage :

```text
ms,profil,tBoilerC,pBar,pTarget,dimLevel,flowMlS,waterMl,cupG,phase,shotS
```

`phase` : 0=idle, 1=pré-infusion, 2=rampe, 3=extraction, 4=déclin.
Copie 5 à 10 shots dans un tableur : ça donne la réponse du dimmer
(ULKA + boucle de pression) et l'overshoot réel, pour régler Kp du PID
pression et valider tau. Piste future (pas avant les essais) : apprendre
tau en fonction du débit final en g/s plutôt qu'une constante par profil.

### Autotune PID — comment ça marche

1. Mets-toi sur le profil à tuner (écran principal, tourner) — la
   consigne de l'autotune sera la temp brew de ce profil, mais le
   résultat (Kp/Ki/Kd) est **global** : il s'applique à tous les profils.
2. Menu → Autotune PID → clic. La chaudière doit être proche de la
   consigne (laisse-la se stabiliser avant, sinon le 1er cycle est
   du bruit — le code l'ignore de toute façon).
3. Le firmware fait osciller la chaudière autour de la consigne en
   tout-ou-rien (relais 0/100 %) et mesure 3 cycles : période Tu et
   amplitude. Méthode Ziegler-Nichols : Kp = 0,6·Ku, Ki = 2·Kp/Tu,
   Kd = Kp·Tu/8.
4. Résultat affiché (Kp/Ki/Kd) et sauvé (global). Clic
   pendant l'autotune = annuler (anciens réglages gardés). Timeout
   10 min = annulé.
5. Si le résultat te semble agressif (overshoot), divise Ki par 2 —
   c'est le réglage Z-N classique, un peu « chaud » pour une chaudière.

Debug : le port série sort une ligne CSV toutes les 500 ms
(`ms, profil, Tchaud, Tgroupe, Pbar, consigne, PID%, etat, timer`) —
pratique pour voir les oscillations de l'autotune.

## Les deux « éléments » — ta question

La Classic n'a qu'**UN seul élément chauffant**. Les deux thermostats
d'origine (brew 107 °C / vapeur ~140 °C) commandent le même élément —
c'est juste deux seuils. Un seul SSR pour le chauffage, donc.

- **Les 2 thermistors : oui, utilisés ensemble** — chaudière = PID,
  groupe = affichage (et référence pour le temp surfing si tu veux).
- **Les 2 SSR : oui, actifs ensemble pendant un shot** — chauffage (PID)
  + pompe en même temps.

## Phase 2 (plus tard)

- **Dimmer RobotDyn 4 A** sur la pompe (D9 → dimmer au lieu du SSR)
  = profiling de pression avec le capteur comme boucle fermée.
- **Drivers stepper du RAMPS** = pompe rotative (NEMA 23 + driver
  externe costaud — un A4988 est limite, voir discussion).
- **Load cell sous le drip tray** = pesée du shot en direct.
