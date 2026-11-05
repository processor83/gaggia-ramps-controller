/*
 * Gaggia Classic Pro — PID + pression sur Arduino Mega + RAMPS 1.4
 * VERSION 2 — profils, autotune PID, LCD graphique 128x64 ST7920
 * =================================================================
 * Materiel : Arduino Mega 2560 + RAMPS 1.4 + LCD graphique 128x64 ST7920
 *            (RepRapDiscount Full Graphic Smart Controller)
 *            2x SSR, 2x thermistor 100k NTC (beta 3950), capteur pression
 *            XDB401 0-1.2 MPa, G1/4, 0.5-4.5 V
 *
 * NOUVEAUTES v2 :
 *  - 5 PROFILS (nom + temp brew + pression cible chacun). La temp
 *    vapeur est GLOBALE (la meme pour tous les cafes), comme le PID
 *    (hardware : chaudiere + element) — un seul jeu de Kp/Ki/Kd.
 *    Ecran principal : TOURNER la molette = changer de profil,
 *    CLIQUER = ouvrir le menu config. Le profil actif est sauvegarde.
 *  - Menu config : editer le profil actif (nom lettre par lettre a la
 *    molette, temperatures, Kp/Ki/Kd), autotune PID, sauver & quitter.
 *  - AUTOTUNE PID (methode relais, Ziegler-Nichols) : regle le PID
 *    global tout seul. Annulable d'un clic.
 *
 * NOUVEAUTES v3 :
 *  - Le profil controle aussi la PRESSION CIBLE (9 bar par defaut).
 *    Sans dimmer, c'est une cible d'affichage (ligne de reference sur
 *    la courbe + comparaison dans le resume) ; la pression reelle est
 *    fixee par l'OPV mecanique. Avec le dimmer (phase 2), la meme
 *    cible pilotera la pompe.
 *  - COURBE DE PRESSION en direct pendant le shot (vue extraction).
 *  - RESUME DE SHOT apres extraction : duree, Pmax/Pmoy, T min/max,
 *    + hint de dial-in. Le volume (ml) reste MANUEL : c'est toi qui
 *    demarres/arreter avec le bouton BREW, pas d'arret automatique.
 *  - BARRE PID : duty cycle du chauffage en direct sur l'ecran principal.
 *  - ARRET AUTO du shot (4 modes, GLOBAL, au menu) : Manuel,
 *    Minuteur (duree par profil), Debitmetre (ml par profil),
 *    Balance (grammes par profil). Le profil montre la cible du
 *    mode configure. Debitmetre = D2 (interruption), balance HX711
 *    bit-bang D4/D5. Securite 60 s sur les modes capteur.
 *  - ARCHITECTURE "LA CREME" : 2 SSR + 1 DIMMER. Chauffage sur SSR-40DA
 *    (D8), solenoide 3-way SEUL sur SSR-25DA (D9), pompe sur DIMMER
 *    RobotDyn (ZC = D3, gate = D11, controle de phase Timer1). La
 *    pression cible du profil devient REELLE : un 2e PID pilote le
 *    dimmer en boucle fermee sur le capteur XDB401. PRE-INFUSION par
 *    profil (pression + duree) : le solenoide reste ouvert pendant
 *    toute la pre-infusion, seule la pompe tourne a basse puissance.
 *  - PROFILAGE COMPLET : pre-infusion -> RAMPE (duree) -> extraction
 *    (pression cible) -> DECLIN (pression finale a partir de X s
 *    d'extraction). Consigne de pression interpolee pendant la rampe.
 *  - DEBITMETRE = DIAGNOSTIC PERMANENT (jamais la reference de dose) :
 *    debit instantane (mL/s, fenetre glissante 1 s), volume total d'eau,
 *    detection d'anomalie (pas de debit 5 s en extraction -> arret).
 *    Avertissements seulement (jamais d'arret) : puck RESTRICTIF
 *    (pression >= 8 bar + debit < 0,5 mL/s, 5 s) et puck PERMISSIF
 *    (debit > 3 mL/s + pression < 4 bar, 5 s). Seuils a tuner.
 *  - MODE GRAMMES = REFERENCE DE DOSE, avec ANTICIPATION APPRISE :
 *    l'arret vise le poids FINAL dans la tasse, pas le poids a la
 *    coupure. On apprend tau (secondes d'ecoulement) par profil, EMA
 *    alpha 0,2 : cutoff = cible - tau * g/s (g/s mesures sur 2 s
 *    glissantes avant la coupure). Stabilisation balance 8 s max apres
 *    le shot, puis mise a jour de tau en EEPROM. PROTECTION : tau n'est
 *    mis a jour que si le shot s'est termine normalement en mode
 *    grammes (pas d'arret manuel, pas de FAULT, pas d'arret de
 *    securite, balance stable, overshoot 0-4 g). Sinon "Tau non appris".
 *  - SECURITES : pression max 12,5 bar (FAULT), temps max 90 s (tous
 *    modes), perte du signal balance (mode grammes), pas de debit.
 *  - TOUT SE REGLE SUR LA MACHINE (pas de SD, pas d'USB, pas de WiFi) :
 *    gains du PID pression et tau editables au menu ; historique des
 *    10 derniers shots (duree, Pmax/Pmoy, eau, poids final, overshoot,
 *    tau) consultable au LCD ; courbe de pression du dernier shot
 *    affichee a l'ecran. Le log serie CSV reste dispo en option.
 *  - LOG SERIE CSV (250 ms pendant le shot) : ms,profil,tBoilerC,pBar,
 *    pTarget,dimLevel,flowMlS,waterMl,cupG,phase,shotS — pour tuner le
 *    PID pression et tau sur 5-10 shots reels. Piste future (pas avant
 *    les essais) : tau en fonction du debit final en g/s.
 *  - ECRAN DE SHOT : pression, pompe %, debit mL/s, eau totale mL,
 *    tasse g + cible, temps. Resume apres shot : poids final, overshoot,
 *    tau appris.
 *  - Ecran principal : cibles + valeurs live (temp chaudiere, pression,
 *    temp groupe, timer de shot).
 *
 * Principe :
 *  - La chaudiere est pilotee en PID via un SSR (D8). Le thermostat
 *    de brew d'origine est contourne ; le fusible thermique d'origine
 *    RESTE en serie avec l'element (securite materielle, jamais bypasser).
 *  - La pompe + le solenoide 3-way sont pilotes ensemble via un SSR (D9).
 *  - Le bouton BREW d'origine devient une entree logique (D19).
 *  - Le bouton STEAM d'origine devient une entree logique (D18) :
 *    enfonce = mode vapeur, relache = mode brew.
 *
 * RAM : buffer LCD 1024 o, total ~3 Ko sur 8192 o du Mega. Ca fit large.
 * Librairie requise : U8g2. Carte : Arduino Mega 2560.
 *
 * ATTENTION 120 V : les SSR commutent le secteur. Verifie deux fois
 * le cablage, gaine thermo sur toutes les connexions secteur, et ne
 * travaille jamais sur la machine branchee.
 */

// ===================== Includes =====================
#include <Arduino.h>
#include <EEPROM.h>
#include <U8g2lib.h>
#include <math.h>
#include <string.h>

// Prototypes (le .ino brut compile aussi hors de l'IDE Arduino)
void inMenuStop();
void recordShot();
void stopShot();
void startShot();
void switchProfile(int idx);
void applyProfile();
void startAutotune();
void cancelAutotune();
void finishAutotune();
void autotuneStep(float t, unsigned long now);
void menuHandle(int8_t d, bool clicked, unsigned long now);
void adjustItem(int8_t item, int8_t d);
void trimName(char* s);

// ===================== Pins (RAMPS 1.4) =====================
#define PIN_HEATER_SSR    8    // D8  MOSFET lit  -> SSR element (SSR-40DA + dissipateur)
#define PIN_SOLENOID_SSR  9    // D9  MOSFET lit1 -> SSR solenoide 3-way SEUL (SSR-25DA)
#define PIN_DIM_ZC        3    // D3 (INT1) : dimmer RobotDyn, zero-cross
#define PIN_DIM_GATE      11   // D11 : dimmer RobotDyn, gate TRIAC
#define PIN_STEAM_SW     18    // bouton vapeur d'origine, actif LOW
#define PIN_BREW_SW      19    // bouton brew d'origine, actif LOW
#define PIN_THERM_BOILER A13   // T0 chaudiere
#define PIN_THERM_GROUP  A14   // T1 groupe
#define PIN_PRESSURE     A10   // AUX-2, signal capteur pression
#define PIN_ENC_A        31    // encodeur LCD
#define PIN_ENC_B        33
#define PIN_ENC_BTN      35    // bouton encodeur
#define PIN_FLOW         2     // D2 (INT0) : debitmetre (hall, collecteur ouvert)
#define PIN_HX711_DOUT   4     // D4 : HX711 DOUT (load cell sous le bec)
#define PIN_HX711_SCK    5     // D5 : HX711 SCK

// LCD graphique 128x64 ST7920 — SPI materiel Mega (MOSI 51, SCK 52), CS 23, RESET 17
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, 23, 17);
// Si l'ecran reste vide, essaie la version logicielle (decommente la ligne
// suivante et commente celle du dessus) :
// U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, 23, 17, 16, U8X8_PIN_NONE);

// ===================== Capteurs =====================
#define THERM_R0       100000.0f  // thermistor 100k NTC
#define THERM_BETA     3950.0f
#define THERM_PULLUP   4700.0f    // pullup 4.7k du RAMPS
#define PRESS_V0       0.5f       // 0 bar  = 0.5 V
#define PRESS_VMAX     4.5f       // 12 bar = 4.5 V
#define PRESS_BAR_MAX  12.0f
#define TEMP_FAULT_MAX 165.0f     // coupure securite chaudiere

// ===================== PID =====================
#define PID_WINDOW_MS  1000UL     // fenetre de modulation du SSR (1 s)
#define LOOP_TEMP_MS   250UL      // periode de lecture capteurs / PID
#define PID_KP_DEF     30.0f      // PID par defaut (avant autotune)
#define PID_KI_DEF     2.0f
#define PID_KD_DEF     50.0f

// ===================== Profils / EEPROM =====================
#define EEPROM_MAGIC   0x6A22     // change a chaque changement de layout
#define N_PROFILES     5
#define NAME_LEN       10
const char NAME_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Modes d'arret auto (defini ici car utilise par loadDefaults)
enum ShotMode { SHOT_MANUAL = 0, SHOT_TIMER, SHOT_FLOW, SHOT_WEIGHT };

struct Profile {
  char  name[NAME_LEN + 1];  // 10 caracteres + \0
  float brewTemp;            // consigne brew
  float pressureTarget;      // pression cible du profil (bar)
  uint16_t shotSecs;         // mode minuteur : duree cible (s)
  uint16_t shotMl;           // mode debitmetre : volume cible (ml)
  uint16_t shotGrams;        // mode balance : poids cible (g)
  float preinfPressure;      // pre-infusion : pression cible (bar)
  uint8_t preinfSecs;        // pre-infusion : duree (s, 0 = desactivee)
  uint8_t rampSecs;          // rampe preinf -> cible (s, 0 = directe)
  float declinePressure;     // declin : pression de fin de shot (bar)
  uint8_t declineSecs;       // declin : secondes d'extraction avant le declin (0 = off)
  float overshootTau;        // anticipation mode grammes : secondes d'ecoulement apprises (EMA)
  // Pas de temp vapeur ici : la vapeur est la meme pour tous les
  // cafes, c'est un reglage GLOBAL (voir Settings).
  // Le PID n'est PAS dans le profil non plus : il depend du hardware
  // (chaudiere + element), identique pour tous les profils.
};

// ---- Historique des shots (sur la machine, pas besoin d'USB/SD) ----
// 10 derniers shots, persistes en EEPROM. Un shot = duree, Pmax/Pmoy,
// eau totale, poids final, overshoot, tau. La courbe detaillee (250 ms)
// du dernier shot est gardee en RAM (curveBuf) pour l'ecran "Courbe".
#define HIST_N 10
struct ShotRecord {
  uint16_t durS;       // duree (s)
  uint8_t pMax10;      // Pmax x10 (bar)
  uint8_t pAvg10;      // Pmoy x10 (bar)
  uint8_t waterMl;     // eau totale (mL, arrondi)
  uint16_t finalG10;   // poids final x10 (g, 0 = pas de balance)
  int8_t over10;       // overshoot x10 (g, signe)
  uint8_t tau100;      // tau x100 (s)
};

struct Settings {
  uint16_t magic;
  uint8_t  active;           // profil actif 0..4
  float kp, ki, kd;          // PID GLOBAL (hardware)
  float steamTemp;           // consigne vapeur GLOBALE (145 C par defaut)
  float pkp, pki, pkd;       // PID pression GLOBAL (tunable au menu)
  ShotRecord history[HIST_N];
  uint8_t histIdx;           // prochaine position d'ecriture (anneau)
  uint8_t histCount;         // nombre de shots enregistres (max HIST_N)
  uint8_t shotMode;          // mode d'arret auto GLOBAL (voir ShotMode)
  Profile  profiles[N_PROFILES];
};

Settings settings;

void loadDefaults() {
  settings.magic = EEPROM_MAGIC;
  settings.active = 0;
  settings.kp = PID_KP_DEF;
  settings.ki = PID_KI_DEF;
  settings.kd = PID_KD_DEF;
  settings.steamTemp = 145.0f;
  settings.pkp = 15.0f; settings.pki = 1.0f; settings.pkd = 0.0f;  // PID pression
  memset(settings.history, 0, sizeof(settings.history));
  settings.histIdx = 0; settings.histCount = 0;
  settings.shotMode = SHOT_MANUAL;
  const char* names[N_PROFILES] = {"Espresso", "Ristretto", "Lungo", "Decaf", "Doux"};
  const float brews[N_PROFILES] = {93.0f, 94.0f, 92.0f, 91.0f, 90.0f};
  for (uint8_t i = 0; i < N_PROFILES; i++) {
    strncpy(settings.profiles[i].name, names[i], NAME_LEN);
    settings.profiles[i].name[NAME_LEN] = '\0';
    settings.profiles[i].brewTemp = brews[i];
    settings.profiles[i].pressureTarget = 9.0f;
    settings.profiles[i].shotSecs = 27;
    settings.profiles[i].shotMl = 40;
    settings.profiles[i].shotGrams = 36;
    settings.profiles[i].preinfPressure = 3.0f;
    settings.profiles[i].preinfSecs = 5;
    settings.profiles[i].rampSecs = 3;
    settings.profiles[i].declinePressure = 6.0f;
    settings.profiles[i].declineSecs = 0;  // off par defaut
    settings.profiles[i].overshootTau = 0.4f;
  }
}

// ===================== Arret auto du shot =====================
// Mode GLOBAL (au menu) : le profil montre la cible du mode configure.
#define SHOT_MAX_SECS 60  // securite : arret force a 60 s (modes capteur)

// Debitmetre : impulsions par ml. A CALIBRER : compter les impulsions
// pour 100 ml d'eau (pompe seule, sans cafe) et diviser par 100.
// Defaut 1,0 imp/ml. (YF-S201 ~0,45 imp/ml : trop grossier pour 40 ml,
// preferer un debitmetre espresso ~1-2 imp/ml.)
#define FLOW_PULSES_PER_ML 1.0f
volatile unsigned long flowPulses = 0;
void flowISR() { flowPulses++; }
float liveMl = 0;      // ml ecoules depuis le debut du shot (mode debitmetre)

// Load cell HX711 (bit-bang, gain 128, canal A) : counts par gramme.
// A CALIBRER : poser un poids connu (ex. 100 g) sur la cellule, lire
// le brut serie, diviser par le poids.
#define HX711_COUNTS_PER_G 100.0f
long hx711Tare = 0;
float liveGrams = 0;   // grammes dans la tasse (mode balance)

// ===================== Dimmer pompe (RobotDyn, controle de phase) =====================
// Le dimmer coupe chaque demi-vague du 120 V : la pompe tourne en
// continu, juste moins fort -> pression stable et reglable.
// ZC (D3) declenche une interruption a chaque passage a zero ; Timer1
// (prescaler 8 -> 0,5 us/tick) tire le TRIAC apres dimDelayUs.
// Niveau 0 = pompe arretee, 100 = pleine puissance. Demi-cycle 60 Hz = 8333 us.
#define DIM_MIN_LEVEL 25  // sous ~25 % la pompe vibrante cale : le PID clamp ici
volatile uint8_t dimLevel = 0;       // 0..100 %
volatile uint16_t dimDelayUs = 8333;

void dimmerSet(uint8_t level) {
  if (level > 100) level = 100;
  dimLevel = level;
  dimDelayUs = (uint16_t)(100 - level) * 83;
}

void zcISR() {  // zero-cross (D3, RISING) — installe via attachInterrupt
  if (dimLevel == 0) return;
  TCNT1 = 0;
  OCR1A = dimDelayUs * 2;  // 0,5 us/tick
  TIFR1 |= (1 << OCF1A);
  TIMSK1 |= (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect) {  // tire le TRIAC
  TIMSK1 &= ~(1 << OCIE1A);
  digitalWrite(PIN_DIM_GATE, HIGH);
  delayMicroseconds(60);
  digitalWrite(PIN_DIM_GATE, LOW);
}

// ===================== Etats =====================
enum Fault { F_NONE, F_THERM_BOILER, F_OVERTEMP, F_OVERPRESSURE };
Fault fault = F_NONE;
bool brewing = false;
bool steamMode = false;
unsigned long shotStartMs = 0;
// Phases du shot (dimmer) : pre-infusion puis extraction
enum ShotPhase { PH_IDLE, PH_PREINF, PH_RAMP, PH_BREW, PH_DECLINE };
ShotPhase shotPhase = PH_IDLE;
unsigned long phaseStartMs = 0;
unsigned long lastTempMs = 0;
unsigned long lastDispMs = 0;
unsigned long lastSerMs = 0;
unsigned long winStartMs = 0;
float pidOut = 0.0f;       // 0..100 %
float tBoiler = 0, tGroup = 0, pBar = 0;
bool pOk = true, gOk = true;

// ---- Courbe de pression (vue extraction) : 120 ech. x 250 ms = 30 s ----
#define CURVE_N 120
float curveBuf[CURVE_N];
uint8_t curveLen = 0;
void curvePush(float v) {
  if (curveLen < CURVE_N) curveBuf[curveLen++] = v;
  else { memmove(curveBuf, curveBuf + 1, (CURVE_N - 1) * sizeof(float)); curveBuf[CURVE_N - 1] = v; }
}

// ---- Stats du shot (resume apres extraction) ----
struct ShotStats {
  bool valid = false;
  float maxP = 0, sumP = 0;
  uint16_t count = 0;
  float minT = 1000, maxT = -1000;
  unsigned long durMs = 0;
};
ShotStats shot;
bool showSummary = false;
unsigned long summaryMs = 0;
unsigned long lastShotSecs = 0;

// ---- Debitmetre : diagnostic permanent (toutes les extractions) ----
// Jamais la reference de dose : il mesure le debit d'eau de la pompe
// (debit instantane, volume total, anomalies). La dose en tasse, c'est
// la load cell. Fenetre glissante 1 s sur les impulsions cumulees.
#define FLOW_HIST_N 4  // 4 ticks x 250 ms = 1 s
unsigned long flowHist[FLOW_HIST_N] = {0, 0, 0, 0};
uint8_t flowHistIdx = 0;
float flowRateMlS = 0;   // debit instantane (mL/s)
bool flowSeen = false;   // au moins une impulsion vue pendant ce shot
uint16_t noFlowTicks = 0;

// ---- Balance : anticipation d'arret (overshoot appris, mode grammes) ----
// Le poids final depasse le poids a la coupure (l'eau continue de couler).
// On apprend tau (secondes d'ecoulement) par profil : overshoot ~= tau * g/s.
// EMA alpha 0,2 : converge en ~5 shots, ne reagit pas trop a un shot bizarre.
#define TAU_EMA_ALPHA 0.2f
#define WEIGHT_HIST_N 8  // 8 echantillons x 250 ms = 2 s
float weightHist[WEIGHT_HIST_N] = {0};
uint8_t weightHistIdx = 0;
float flowRateGs = 0;    // vitesse d'augmentation du poids (g/s, 2 s glissantes)
float weightAtCutoff = 0, flowRateGsAtCutoff = 0;
bool scalePresent = false;
uint8_t scaleBadTicks = 0;
bool settling = false;   // stabilisation de la balance apres le shot
unsigned long settleStartMs = 0;
float settlePrev = 0;
uint8_t settleStableTicks = 0;
float finalWeight = 0, overshootMeasured = 0;
char stopReason[16] = "";  // "" / "PAS DE DEBIT" / "BALANCE HS" / "90s MAX"
bool manualStop = false;   // l'utilisateur a arrete le shot lui-meme
char shotWarn[16] = "";    // "" / "RESTRICTIF" / "PERMISSIF" (avertissement seulement)
uint16_t warnRestrTicks = 0, warnPermTicks = 0;
bool tauLearned = false;   // tau mis a jour a ce shot (sinon "non appris")
float pTargetLive = 0;     // consigne de pression courante (pour le log serie)

// PID
struct Pid {
  float kp, ki, kd;
  float integ = 0, prevErr = 0;
  unsigned long lastMs = 0;
  Pid(float p, float i, float d) : kp(p), ki(i), kd(d) {}
  float compute(float sp, float pv, unsigned long now) {
    float dt = (now - lastMs) / 1000.0f;
    if (dt <= 0.001f) dt = 0.001f;
    if (dt > 5.0f) dt = 5.0f;
    float err = sp - pv;
    integ += err * dt;
    integ = constrain(integ, -200.0f, 200.0f);  // anti-windup
    float deriv = (err - prevErr) / dt;
    float out = kp * err + ki * integ + kd * deriv;
    prevErr = err;
    lastMs = now;
    return constrain(out, 0.0f, 100.0f);
  }
  void reset(unsigned long now) { integ = 0; prevErr = 0; lastMs = now; }
};
Pid pid(PID_KP_DEF, PID_KI_DEF, PID_KD_DEF);
// PID pression (dimmer) : gains en EEPROM, editables au menu
// (la pression repond vite : surtout du P, un peu de I, pas de D)
Pid pidPress(15.0f, 1.0f, 0.0f);

void applyProfile() {
  // Le PID est global : changer de profil ne change que la consigne.
  // On reset juste l'integrale pour eviter un saut a la commutation.
  pid.reset(millis());
}
void switchProfile(int idx) {
  idx = ((idx % N_PROFILES) + N_PROFILES) % N_PROFILES;
  if (idx == settings.active) return;
  settings.active = (uint8_t)idx;
  applyProfile();
  EEPROM.put(0, settings);   // ne reecrit que l'octet change
}

// ===================== Autotune PID (relais + Ziegler-Nichols) =====================
// Fait osciller la chaudiere autour de la consigne (relais 0/100 %),
// mesure la periode Tu et l'amplitude a, puis :
//   Ku = 4*d / (PI*a)   avec d = 50 (amplitude du relais autour de 50 %)
//   Kp = 0.6*Ku | Ki = 2*Kp/Tu | Kd = Kp*Tu/8
// Le 1er cycle est ignore (montee en temperature). 3 cycles mesures.
struct Tune {
  bool active = false;
  bool showResult = false;
  bool cancelled = false;
  unsigned long startMs = 0;
  unsigned long resultMs = 0;
  float sp = 0;
  bool heaterOn = true;
  bool lookingForMax = true;
  float curMax = -1000, curMin = 1000;
  unsigned long lastPeakMs = 0;
  int cycles = 0;
  float tuSum = 0, ampSum = 0;
  float resKp = 0, resKi = 0, resKd = 0;
};
Tune tune;

void startAutotune() {
  inMenuStop();
  Profile &p = settings.profiles[settings.active];
  tune.active = true;
  tune.showResult = false;
  tune.cancelled = false;
  tune.startMs = millis();
  tune.sp = p.brewTemp;
  tune.heaterOn = true;
  tune.lookingForMax = true;
  tune.curMax = -1000; tune.curMin = 1000;
  tune.lastPeakMs = 0;
  tune.cycles = 0; tune.tuSum = 0; tune.ampSum = 0;
  if (brewing) stopShot();
  digitalWrite(PIN_HEATER_SSR, HIGH);
}
void cancelAutotune() {
  tune.active = false;
  tune.cancelled = true;
  tune.showResult = true;
  tune.resultMs = millis();
  digitalWrite(PIN_HEATER_SSR, LOW);
  pid.reset(millis());
}
void finishAutotune() {
  float tu = tune.tuSum / tune.cycles;    // periode moyenne (s)
  float a = tune.ampSum / tune.cycles;    // amplitude moyenne (C)
  float ku = (4.0f * 50.0f) / (3.14159f * a);
  float kp = constrain(0.6f * ku, 1.0f, 200.0f);
  float ki = constrain(2.0f * kp / tu, 0.05f, 20.0f);
  float kd = constrain(kp * tu / 8.0f, 0.0f, 500.0f);
  // PID GLOBAL : sauve une seule fois, valable pour tous les profils.
  settings.kp = kp; settings.ki = ki; settings.kd = kd;
  pid.kp = kp; pid.ki = ki; pid.kd = kd;
  pid.reset(millis());
  EEPROM.put(0, settings);
  tune.resKp = kp; tune.resKi = ki; tune.resKd = kd;
  tune.active = false;
  tune.cancelled = false;
  tune.showResult = true;
  tune.resultMs = millis();
  digitalWrite(PIN_HEATER_SSR, LOW);
}
void autotuneStep(float t, unsigned long now) {
  const float NB = 0.5f;   // bande de bruit autour de la consigne
  if (t > tune.sp + NB) tune.heaterOn = false;
  else if (t < tune.sp - NB) tune.heaterOn = true;
  digitalWrite(PIN_HEATER_SSR, tune.heaterOn ? HIGH : LOW);

  if (tune.lookingForMax) {
    if (t > tune.curMax) tune.curMax = t;
    if (t < tune.curMax - 0.3f) {        // le pic est passe
      if (tune.lastPeakMs != 0) {
        float period = (now - tune.lastPeakMs) / 1000.0f;
        float amp = (tune.curMax - tune.curMin) / 2.0f;
        if (period > 5.0f && period < 600.0f && amp > 0.3f) {
          tune.cycles++;
          tune.tuSum += period;
          tune.ampSum += amp;
        }
      }
      tune.lastPeakMs = now;
      tune.curMin = t;
      tune.lookingForMax = false;
      if (tune.cycles >= 3) finishAutotune();
    }
  } else {
    if (t < tune.curMin) tune.curMin = t;
    if (t > tune.curMin + 0.3f) {        // la vallee est passee
      tune.curMax = t;
      tune.lookingForMax = true;
    }
  }
  if (now - tune.startMs > 600000UL) cancelAutotune();  // timeout 10 min
}

// ===================== Menu =====================
enum MenuItem { MI_PROFILE, MI_NAME, MI_BREW, MI_PRESSURE, MI_SHOTMODE, MI_SHOTTARGET, MI_PREINF_P, MI_PREINF_T, MI_RAMP, MI_DECLINE_P, MI_DECLINE_T, MI_TAU, MI_STEAM, MI_KP, MI_KI, MI_KD, MI_PKP, MI_PKI, MI_PKD, MI_TUNE, MI_HIST, MI_CURVE, MI_SAVE };
#define MENU_COUNT 23
bool inMenu = false;
int8_t menuSel = 0;        // 0..8, navigation
int8_t editItem = -1;      // -1 = navigation, sinon item en cours d'edition
int8_t menuTop = 0;        // 1re ligne visible (defilement, 4 lignes)
bool editingName = false;
bool inHistory = false;    // ecran historique des shots
bool inCurve = false;      // ecran courbe du dernier shot
uint8_t histSel = 0;       // 0 = shot le plus recent
uint8_t nameCursor = 0;    // 0..9
unsigned long menuLastActMs = 0;

void inMenuStop() { inMenu = false; editItem = -1; editingName = false; inHistory = false; inCurve = false; }

void trimName(char* s) {
  int8_t n = (int8_t)strlen(s);
  while (n > 0 && s[n - 1] == ' ') s[--n] = '\0';
  if (n == 0) { s[0] = 'P'; s[1] = '\0'; }
}

// Ajuste la valeur de l'item en cours d'edition (+/- d)
void adjustItem(int8_t item, int8_t d) {
  Profile &p = settings.profiles[settings.active];
  switch (item) {
    case MI_PROFILE: switchProfile(settings.active + d); break;
    case MI_BREW:  p.brewTemp  = constrain(p.brewTemp + d * 0.5f, 85.0f, 100.0f); break;
    case MI_STEAM: settings.steamTemp = constrain(settings.steamTemp + d * 1.0f, 130.0f, 155.0f); break;
    case MI_PRESSURE: p.pressureTarget = constrain(p.pressureTarget + d * 0.5f, 6.0f, 12.0f); break;
    case MI_SHOTMODE: settings.shotMode = (uint8_t)((settings.shotMode + d + 4) % 4); break;
    case MI_SHOTTARGET:
      if (settings.shotMode == SHOT_TIMER) p.shotSecs = constrain(p.shotSecs + d, 10, 60);
      else if (settings.shotMode == SHOT_FLOW) p.shotMl = constrain(p.shotMl + d, 10, 120);
      else if (settings.shotMode == SHOT_WEIGHT) p.shotGrams = constrain(p.shotGrams + d, 10, 120);
      break;
    case MI_PREINF_P: p.preinfPressure = constrain(p.preinfPressure + d * 0.5f, 1.0f, 6.0f); break;
    case MI_PREINF_T: p.preinfSecs = constrain(p.preinfSecs + d, 0, 10); break;
    case MI_RAMP: p.rampSecs = constrain(p.rampSecs + d, 0, 10); break;
    case MI_DECLINE_P: p.declinePressure = constrain(p.declinePressure + d * 0.5f, 1.0f, 12.0f); break;
    case MI_DECLINE_T: p.declineSecs = constrain(p.declineSecs + d, 0, 60); break;
    case MI_TAU: p.overshootTau = constrain(p.overshootTau + d * 0.05f, 0.0f, 2.0f); break;
    case MI_KP: settings.kp = constrain(settings.kp + d * 1.0f, 1.0f, 200.0f); pid.kp = settings.kp; break;
    case MI_KI: settings.ki = constrain(settings.ki + d * 0.1f, 0.0f, 20.0f);  pid.ki = settings.ki; break;
    case MI_KD: settings.kd = constrain(settings.kd + d * 5.0f, 0.0f, 500.0f); pid.kd = settings.kd; break;
    case MI_PKP: settings.pkp = constrain(settings.pkp + d * 1.0f, 1.0f, 200.0f); pidPress.kp = settings.pkp; break;
    case MI_PKI: settings.pki = constrain(settings.pki + d * 0.1f, 0.0f, 20.0f);  pidPress.ki = settings.pki; break;
    case MI_PKD: settings.pkd = constrain(settings.pkd + d * 5.0f, 0.0f, 500.0f); pidPress.kd = settings.pkd; break;
    default: break;
  }
}

void menuHandle(int8_t d, bool clicked, unsigned long now) {
  if (d != 0 || clicked) menuLastActMs = now;

  // --- Ecran historique : tourner = naviguer, clic = retour ---
  if (inHistory) {
    if (d != 0 && settings.histCount > 0) {
      int n = settings.histCount;
      int s = ((int)histSel + d) % n;
      if (s < 0) s += n;
      histSel = (uint8_t)s;
    }
    if (clicked) inHistory = false;
    return;
  }
  // --- Ecran courbe : clic = retour ---
  if (inCurve) {
    if (clicked) inCurve = false;
    return;
  }
  // --- Edition du nom : tourner = lettre, clic = caractere suivant ---
  if (editingName) {
    Profile &p = settings.profiles[settings.active];
    if (d != 0) {
      const char* pos = strchr(NAME_CHARS, p.name[nameCursor]);
      int idx = pos ? (int)(pos - NAME_CHARS) : 0;
      int n = (int)strlen(NAME_CHARS);
      idx = ((idx + d) % n + n) % n;
      p.name[nameCursor] = NAME_CHARS[idx];
    }
    if (clicked) {
      nameCursor++;
      if (nameCursor >= NAME_LEN) { editingName = false; trimName(p.name); }
    }
    return;
  }
  // --- Edition d'une valeur : tourner = ajuster, clic = confirmer ---
  if (editItem >= 0) {
    if (d != 0) adjustItem(editItem, d);
    if (clicked) editItem = -1;
    return;
  }
  // --- Navigation : tourner = selection, clic = entrer ---
  if (d != 0) {
    menuSel = (int8_t)((menuSel + d) % MENU_COUNT);
    if (menuSel < 0) menuSel += MENU_COUNT;
    if (menuSel < menuTop) menuTop = menuSel;
    if (menuSel >= menuTop + 4) menuTop = menuSel - 3;
  }
  if (clicked) {
    switch (menuSel) {
      case MI_NAME: editingName = true; nameCursor = 0; break;
      case MI_TUNE: startAutotune(); break;
      case MI_HIST: inHistory = true; histSel = 0; break;
      case MI_CURVE: inCurve = true; break;
      case MI_SAVE: EEPROM.put(0, settings); inMenuStop(); break;
      default: editItem = menuSel; break;
    }
  }
}

// ===================== Lecture thermistor =====================
// Retourne la temperature en C, ok=false si sonde ouverte ou en court-circuit.
float readThermistorC(uint8_t pin, bool &ok) {
  long sum = 0;
  for (uint8_t i = 0; i < 8; i++) { sum += analogRead(pin); delay(1); }
  int adc = (int)(sum / 8);
  if (adc <= 5 || adc >= 1018) { ok = false; return 0.0f; }
  ok = true;
  float r = THERM_PULLUP * adc / (1023.0f - adc);
  float t = 1.0f / (1.0f / 298.15f + log(r / THERM_R0) / THERM_BETA) - 273.15f;
  return t;
}

// ===================== Lecture pression =====================
// XDB401 0-1.2 MPa : 0.5 V = 0 bar, 4.5 V = 12 bar. ok=false si capteur debranche.
float readPressureBar(bool &ok) {
  long sum = 0;
  for (uint8_t i = 0; i < 16; i++) { sum += analogRead(PIN_PRESSURE); delay(1); }
  float v = (sum / 16.0f) * 5.0f / 1023.0f;
  if (v < 0.25f || v > 4.9f) { ok = false; return 0.0f; }
  ok = true;
  float bar = (v - PRESS_V0) * PRESS_BAR_MAX / (PRESS_VMAX - PRESS_V0);
  return constrain(bar, 0.0f, PRESS_BAR_MAX);
}

// ===================== Encodeur =====================
// +1 / -1 sur front descendant de A. Si le menu va a l'envers, inverse le signe.
int8_t encDelta() {
  static uint8_t lastA = HIGH;
  uint8_t a = digitalRead(PIN_ENC_A);
  if (a != lastA) {
    lastA = a;
    if (a == LOW) return digitalRead(PIN_ENC_B) ? +1 : -1;
  }
  return 0;
}

// Bouton encodeur : true sur un clic (debounce 50 ms)
bool encClicked() {
  static uint8_t last = HIGH;
  static unsigned long lastMs = 0;
  uint8_t b = digitalRead(PIN_ENC_BTN);
  bool clicked = false;
  if (b == LOW && last == HIGH && (millis() - lastMs) > 50) {
    clicked = true;
    lastMs = millis();
  }
  last = b;
  return clicked;
}

// ===================== Load cell HX711 (bit-bang) =====================
// Lecture brute 24 bits, gain 128, canal A. Retourne 0 si pas de HX711.
long hx711ReadRaw() {
  unsigned long t0 = millis();
  while (digitalRead(PIN_HX711_DOUT) == HIGH) {
    if (millis() - t0 > 100) return 0;  // pas de HX711 branche
  }
  long v = 0;
  for (uint8_t i = 0; i < 24; i++) {
    digitalWrite(PIN_HX711_SCK, HIGH);
    v = (v << 1) | digitalRead(PIN_HX711_DOUT);
    digitalWrite(PIN_HX711_SCK, LOW);
  }
  digitalWrite(PIN_HX711_SCK, HIGH);  // 25e impulsion : gain 128
  digitalWrite(PIN_HX711_SCK, LOW);
  if (v & 0x800000) v |= 0xFF000000;  // signe 24 bits
  return v;
}
float currentWeightGrams() {
  return (float)(hx711ReadRaw() - hx711Tare) / HX711_COUNTS_PER_G;
}
// Lecture balance avec validite : raw == 0 = timeout HX711 (pas de signal).
// En cas d'echec, garde la derniere valeur (ne renvoie jamais de saut).
float readWeightGrams(bool &ok) {
  long raw = hx711ReadRaw();
  ok = (raw != 0);
  if (ok) scalePresent = true;
  if (!ok) return liveGrams;
  return (float)(raw - hx711Tare) / HX711_COUNTS_PER_G;
}
float currentMl() {
  unsigned long p;
  noInterrupts(); p = flowPulses; interrupts();
  return (float)p / FLOW_PULSES_PER_ML;
}

// ===================== Shot =====================
// Extraction 100 % MANUELLE : le bouton BREW demarre/arreter, c'est
// toi qui controles le volume (ml) a l'oeil. Pas d'arret automatique.
void startShot() {
  if (fault != F_NONE || tune.active) return;
  // Interrompre une stabilisation en cours : la tasse n'est plus "propre"
  // pour une tare, mais l'utilisateur a choisi de repartir tout de suite.
  if (settling) { settling = false; showSummary = false; }
  Profile &p = settings.profiles[settings.active];
  brewing = true;
  shotStartMs = millis();
  phaseStartMs = shotStartMs;
  shotPhase = (p.preinfSecs > 0) ? PH_PREINF : PH_BREW;
  showSummary = false;
  stopReason[0] = '\0';
  manualStop = false;
  shotWarn[0] = '\0';
  warnRestrTicks = 0; warnPermTicks = 0;
  tauLearned = false;
  curveLen = 0;
  shot.maxP = 0; shot.sumP = 0; shot.count = 0;
  shot.minT = 1000; shot.maxT = -1000; shot.valid = false;
  liveMl = 0; liveGrams = 0; flowRateMlS = 0; flowRateGs = 0;
  finalWeight = 0; overshootMeasured = 0;
  flowSeen = false; noFlowTicks = 0; scaleBadTicks = 0;
  for (uint8_t i = 0; i < FLOW_HIST_N; i++) flowHist[i] = 0;
  flowHistIdx = 0;
  for (uint8_t i = 0; i < WEIGHT_HIST_N; i++) weightHist[i] = 0;
  weightHistIdx = 0;
  noInterrupts(); flowPulses = 0; interrupts();
  if (settings.shotMode == SHOT_WEIGHT) {
    hx711Tare = hx711ReadRaw();  // la tasse doit etre en place avant BREW
    scalePresent = (hx711Tare != 0);
  }
  pidPress.reset(millis());
  digitalWrite(PIN_SOLENOID_SSR, HIGH);  // solenoide ouvert tout le shot
  // La pompe (dimmer) est pilotee par la boucle pression (voir loop).
}
void stopShot() {
  if (brewing) {
    shot.durMs = millis() - shotStartMs;
    lastShotSecs = shot.durMs / 1000;
    shot.valid = (shot.durMs > 1000);
    // Stabilisation balance (mode grammes, balance presente) : le poids
    // final se mesure apres que les dernieres gouttes soient tombees.
    if (settings.shotMode == SHOT_WEIGHT && scalePresent && shot.valid) {
      settling = true;
      settleStartMs = millis();
      settlePrev = liveGrams;
      settleStableTicks = 0;
      showSummary = false;  // le resume s'affiche apres stabilisation
    } else {
      recordShot();  // historique (pas de stabilisation : stats finales)
      showSummary = shot.valid;
      summaryMs = millis();
    }
  }
  brewing = false;
  shotPhase = PH_IDLE;
  dimmerSet(0);
  digitalWrite(PIN_SOLENOID_SSR, LOW);  // ventile le groupe
}

// ===================== Bouton BREW =====================
// Supporte interrupteur a verrouillage (Pro) ET bouton momentane :
//  - verrouillage : enfonce = extraction, relache = arret
//  - momentane : chaque pression bascule extraction on/off
void pollBrewSwitch() {
  static uint8_t prev = HIGH;
  static unsigned long fallMs = 0;
  static bool latched = false;
  uint8_t pin = digitalRead(PIN_BREW_SW);
  unsigned long now = millis();
  if (pin == LOW && prev == HIGH) { fallMs = now; latched = false; }
  if (pin == LOW && !latched && (now - fallMs > 500)) {
    latched = true;               // reste enfonce : interrupteur a verrouillage
    if (!brewing) startShot();
  }
  if (pin == HIGH && prev == LOW) {
    if (!latched) {               // pression courte : bouton momentane -> bascule
      if (brewing) { manualStop = true; stopShot(); } else startShot();
    } else if (brewing) {
      manualStop = true;
      stopShot();                 // relachement du verrouillage -> arret
    }
    latched = false;
  }
  prev = pin;
}

// ===================== LCD =====================
// Petit helper : affiche une chaine calee a droite (font 6x10)
void printRight(const char* s, uint8_t y) {
  u8g2.setCursor(128 - (uint8_t)strlen(s) * 6, y);
  u8g2.print(s);
}

void drawMain() {
  Profile &p = settings.profiles[settings.active];
  float sp = steamMode ? settings.steamTemp : p.brewTemp;
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  // Ligne 1 : profil + etat
  char prof[16];
  snprintf(prof, sizeof(prof), "P%d %s", settings.active + 1, p.name);
  u8g2.setCursor(0, 10);
  u8g2.print(prof);
  u8g2.setCursor(92, 10);
  if (fault != F_NONE) u8g2.print("FAULT");
  else if (steamMode) u8g2.print("VAPEUR");
  else if (brewing) u8g2.print("SHOT");
  else if (fabs(tBoiler - sp) < 1.0f) u8g2.print("PRET");
  else u8g2.print("CHAUF.");

  // Temp chaudiere (live, gros) + pression (live, gros)
  u8g2.setFont(u8g2_font_10x20_tf);
  u8g2.setCursor(0, 33);
  u8g2.print(tBoiler, 1);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(44, 33);
  u8g2.print("C");
  u8g2.setFont(u8g2_font_10x20_tf);
  u8g2.setCursor(72, 33);
  if (pOk) u8g2.print(pBar, 1);
  else u8g2.print("--");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(106, 33);
  u8g2.print("bar");

  // Cibles + live groupe + timer
  u8g2.setCursor(0, 44);
  u8g2.print("cible ");
  u8g2.print(sp, 1);
  u8g2.print("C");
  unsigned long s = brewing ? (millis() - shotStartMs) / 1000 : 0;
  char tbuf[16];
  if (brewing && settings.shotMode == SHOT_TIMER) snprintf(tbuf, sizeof(tbuf), "%lu/%us", s, p.shotSecs);
  else if (brewing && settings.shotMode == SHOT_FLOW) snprintf(tbuf, sizeof(tbuf), "%d/%uml", (int)liveMl, p.shotMl);
  else if (brewing && settings.shotMode == SHOT_WEIGHT) snprintf(tbuf, sizeof(tbuf), "%d/%ug", (int)liveGrams, p.shotGrams);
  else snprintf(tbuf, sizeof(tbuf), "T %lus", s);
  printRight(tbuf, 44);

  // Barre de pression 0-12 bar
  u8g2.drawFrame(0, 48, 128, 8);
  if (pOk) u8g2.drawBox(1, 49, (uint8_t)(pBar / PRESS_BAR_MAX * 126.0f), 6);
  u8g2.setCursor(0, 63);
  u8g2.print("GRP ");
  if (gOk) { u8g2.print(tGroup, 1); u8g2.print("C"); }
  else u8g2.print("--");
  u8g2.setCursor(58, 63);
  u8g2.print("6 bar");
  u8g2.setCursor(116, 63);
  u8g2.print("12");

  u8g2.sendBuffer();
}

void drawMenu() {
  Profile &p = settings.profiles[settings.active];
  const char* labels[MENU_COUNT] = {
    "Profil", "Nom", "Temp brew", "Pression", "Arret auto", "Cible shot", "Pre-inf bar", "Pre-inf s", "Rampe s", "Declin bar", "Declin s", "Tau", "Vapeur", "Kp", "Ki", "Kd", "P-Kp", "P-Ki", "P-Kd", "Autotune PID", "Historique", "Courbe", "Sauver"
  };
  char val[16];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 10);
  u8g2.print("CONFIG  (clic=entrer)");
  for (uint8_t r = 0; r < 4; r++) {
    int8_t item = menuTop + r;
    if (item >= MENU_COUNT) break;
    uint8_t y = 22 + r * 12;
    // Valeur de l'item
    val[0] = '\0';
    switch (item) {
      case MI_PROFILE: snprintf(val, sizeof(val), "P%d %s", settings.active + 1, p.name); break;
      case MI_NAME:    snprintf(val, sizeof(val), "%s", p.name); break;
      case MI_BREW:    dtostrf(p.brewTemp, 0, 1, val); strcat(val, "C"); break;
      case MI_STEAM:   dtostrf(settings.steamTemp, 0, 0, val); strcat(val, "C"); break;
      case MI_PRESSURE: dtostrf(p.pressureTarget, 0, 1, val); strcat(val, " bar"); break;
      case MI_SHOTMODE: {
        const char* modes[] = {"Manuel", "Minuteur", "Debitmetre", "Balance"};
        strcpy(val, modes[settings.shotMode]);
        break;
      }
      case MI_SHOTTARGET:
        if (settings.shotMode == SHOT_TIMER) snprintf(val, sizeof(val), "%u s", p.shotSecs);
        else if (settings.shotMode == SHOT_FLOW) snprintf(val, sizeof(val), "%u ml", p.shotMl);
        else if (settings.shotMode == SHOT_WEIGHT) snprintf(val, sizeof(val), "%u g", p.shotGrams);
        else strcpy(val, "Manuel");
        break;
      case MI_PREINF_P: dtostrf(p.preinfPressure, 0, 1, val); strcat(val, " bar"); break;
      case MI_PREINF_T: snprintf(val, sizeof(val), "%u s", p.preinfSecs); break;
      case MI_RAMP: snprintf(val, sizeof(val), "%u s", p.rampSecs); break;
      case MI_DECLINE_P: dtostrf(p.declinePressure, 0, 1, val); strcat(val, " bar"); break;
      case MI_DECLINE_T: snprintf(val, sizeof(val), "%u s", p.declineSecs); break;
      case MI_TAU: dtostrf(p.overshootTau, 0, 2, val); strcat(val, " s"); break;
      case MI_KP:      dtostrf(settings.kp, 0, 1, val); break;
      case MI_KI:      dtostrf(settings.ki, 0, 2, val); break;
      case MI_KD:      dtostrf(settings.kd, 0, 0, val); break;
      case MI_PKP:     dtostrf(settings.pkp, 0, 1, val); break;
      case MI_PKI:     dtostrf(settings.pki, 0, 2, val); break;
      case MI_PKD:     dtostrf(settings.pkd, 0, 0, val); break;
      case MI_TUNE:    strcpy(val, ">"); break;
      case MI_HIST:    strcpy(val, ">"); break;
      case MI_CURVE:   strcpy(val, ">"); break;
      case MI_SAVE:    strcpy(val, ">"); break;
    }
    bool sel = (item == menuSel);
    if (sel) { u8g2.drawBox(0, y - 9, 128, 11); u8g2.setDrawColor(0); }
    // Libelle dynamique : la cible du shot suit le mode configure
    const char* lbl = labels[item];
    if (item == MI_SHOTTARGET) {
      if (settings.shotMode == SHOT_TIMER) lbl = "Duree shot";
      else if (settings.shotMode == SHOT_FLOW) lbl = "Volume shot";
      else if (settings.shotMode == SHOT_WEIGHT) lbl = "Poids shot";
      else lbl = "Cible shot";
    }
    u8g2.setCursor(4, y);
    u8g2.print(lbl);
    if (editItem == item) { u8g2.print(" ["); }
    printRight(val, y);
    u8g2.setDrawColor(1);
  }
  u8g2.sendBuffer();
}

void drawNameEdit() {
  Profile &p = settings.profiles[settings.active];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 10);
  u8g2.print("NOM DU PROFIL");
  u8g2.setFont(u8g2_font_10x20_tf);
  u8g2.setCursor(14, 35);
  u8g2.print(p.name);
  // Curseur sous le caractere courant
  u8g2.drawBox(14 + nameCursor * 10, 38, 9, 3);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 52);
  u8g2.print("tourner = lettre");
  u8g2.setCursor(0, 62);
  u8g2.print("clic = caract. suivant");
  u8g2.sendBuffer();
}

void drawTune() {
  Profile &p = settings.profiles[settings.active];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 10);
  u8g2.print("AUTOTUNE PID");
  u8g2.setCursor(0, 22);
  u8g2.print("Consigne ");
  u8g2.print(tune.sp, 1);
  u8g2.print("C  (");
  u8g2.print(p.name);
  u8g2.print(")");
  u8g2.setFont(u8g2_font_10x20_tf);
  u8g2.setCursor(0, 42);
  u8g2.print(tBoiler, 1);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(48, 42);
  u8g2.print("C");
  u8g2.setCursor(0, 54);
  u8g2.print("Cycle ");
  u8g2.print(tune.cycles);
  u8g2.print("/3   ");
  unsigned long s = (millis() - tune.startMs) / 1000;
  u8g2.print(s / 60);
  u8g2.print(":");
  if (s % 60 < 10) u8g2.print("0");
  u8g2.print(s % 60);
  u8g2.setCursor(0, 63);
  u8g2.print("clic = annuler");
  u8g2.sendBuffer();
}

void drawTuneResult() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  if (tune.cancelled) {
    u8g2.setCursor(0, 14);
    u8g2.print("AUTOTUNE ANNULE");
    u8g2.setCursor(0, 30);
    u8g2.print("Anciens reglages gardes.");
  } else {
    u8g2.setCursor(0, 14);
    u8g2.print("AUTOTUNE OK — sauve");
    u8g2.setCursor(0, 28);
    u8g2.print("Kp ");
    u8g2.print(tune.resKp, 1);
    u8g2.print("  Ki ");
    u8g2.print(tune.resKi, 2);
    u8g2.setCursor(0, 40);
    u8g2.print("Kd ");
    u8g2.print(tune.resKd, 0);
    u8g2.setCursor(0, 52);
    u8g2.print("PID global — tous profils");
  }
  u8g2.setCursor(0, 63);
  u8g2.print("clic = retour");
  u8g2.sendBuffer();
}

// ===================== Ecran de shot =====================
// Pendant le shot (et la stabilisation) : pression, pompe, debit,
// eau totale, tasse + cible, temps.
void drawShot() {
  Profile &p = settings.profiles[settings.active];
  char buf[24];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  // Ligne 1 : pression + pompe
  u8g2.setCursor(0, 10);
  if (pOk) { u8g2.print("P "); u8g2.print(pBar, 1); u8g2.print(" bar"); }
  else u8g2.print("P -- bar");
  snprintf(buf, sizeof(buf), "Pompe %u%%", dimLevel);
  printRight(buf, 10);
  // Ligne 2 : debit + phase
  u8g2.setCursor(0, 22);
  if (flowSeen) { u8g2.print("Debit "); u8g2.print(flowRateMlS, 1); u8g2.print(" mL/s"); }
  else u8g2.print("Debit -- mL/s");
  const char* ph = "?";
  if (shotWarn[0] != '\0') ph = shotWarn;  // avertissement prioritaire
  else if (settling) ph = "STAB.";
  else if (shotPhase == PH_PREINF) ph = "PREINF";
  else if (shotPhase == PH_RAMP) ph = "RAMPE";
  else if (shotPhase == PH_BREW) ph = "BREW";
  else if (shotPhase == PH_DECLINE) ph = "DECLIN";
  printRight(ph, 22);
  // Ligne 3 : eau totale
  u8g2.setCursor(0, 34);
  if (flowSeen) { u8g2.print("Eau "); u8g2.print(liveMl, 1); u8g2.print(" mL"); }
  else u8g2.print("Eau -- mL");
  // Ligne 4 : tasse + cible
  u8g2.setCursor(0, 46);
  if (settings.shotMode == SHOT_WEIGHT || scalePresent) {
    u8g2.print("Tasse "); u8g2.print(liveGrams, 1); u8g2.print(" g");
  } else {
    u8g2.print("Tasse -- g");
  }
  if (settings.shotMode == SHOT_WEIGHT) {
    snprintf(buf, sizeof(buf), "Cible %u g", p.shotGrams);
    printRight(buf, 46);
  } else if (settings.shotMode == SHOT_FLOW) {
    snprintf(buf, sizeof(buf), "Cible %u mL", p.shotMl);
    printRight(buf, 46);
  } else if (settings.shotMode == SHOT_TIMER) {
    snprintf(buf, sizeof(buf), "Cible %u s", p.shotSecs);
    printRight(buf, 46);
  }
  // Ligne 5 : temps
  u8g2.setCursor(0, 58);
  float ts = (brewing || settling) ? (millis() - shotStartMs) / 1000.0f : (float)lastShotSecs;
  u8g2.print("Temps "); u8g2.print(ts, 1); u8g2.print(" s");
  u8g2.sendBuffer();
}

// ===================== Resume apres shot =====================
void drawSummary() {
  Profile &p = settings.profiles[settings.active];
  char buf[24], f1[8], f2[8];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  // Ligne 1 : duree + poids final (mode grammes)
  u8g2.setCursor(0, 10);
  u8g2.print("SHOT "); u8g2.print(shot.durMs / 1000); u8g2.print("s");
  if (settings.shotMode == SHOT_WEIGHT) {
    dtostrf(finalWeight, 0, 1, f1);
    snprintf(buf, sizeof(buf), "Final %s g", f1);
    printRight(buf, 10);
  }
  // Ligne 2 : cible / overshoot, ou Pmax / Pmoy
  u8g2.setCursor(0, 22);
  if (settings.shotMode == SHOT_WEIGHT) {
    dtostrf(overshootMeasured, 0, 1, f1);
    snprintf(buf, sizeof(buf), "Cible %ug Over %s%s g", p.shotGrams, overshootMeasured >= 0 ? "+" : "", f1);
    u8g2.print(buf);
  } else {
    float avgP = shot.count ? shot.sumP / shot.count : 0;
    dtostrf(shot.maxP, 0, 1, f1); dtostrf(avgP, 0, 1, f2);
    snprintf(buf, sizeof(buf), "Pmax %s  Pmoy %s", f1, f2);
    u8g2.print(buf);
  }
  // Ligne 3 : tau appris, ou temperatures min/max
  u8g2.setCursor(0, 34);
  if (settings.shotMode == SHOT_WEIGHT) {
    if (tauLearned) {
      dtostrf(p.overshootTau, 0, 2, f1);
      dtostrf(p.overshootTau * flowRateGsAtCutoff, 0, 1, f2);
      snprintf(buf, sizeof(buf), "Tau %s s  -%s g", f1, f2);
    } else {
      strcpy(buf, "Tau non appris");
    }
    u8g2.print(buf);
  } else {
    dtostrf(shot.minT, 0, 1, f1); dtostrf(shot.maxT, 0, 1, f2);
    snprintf(buf, sizeof(buf), "T %s-%sC", f1, f2);
    u8g2.print(buf);
  }
  // Ligne 4 : raison d'arret (securite) ou avertissement, si applicable
  if (stopReason[0] != '\0') {
    u8g2.setCursor(0, 46);
    u8g2.print("Arret: "); u8g2.print(stopReason);
  } else if (shotWarn[0] != '\0') {
    u8g2.setCursor(0, 46);
    u8g2.print("Alerte: "); u8g2.print(shotWarn);
  }
  // Ligne 5 : retour
  u8g2.setCursor(0, 58);
  u8g2.print("clic = retour");
  u8g2.sendBuffer();
}

// ===================== Historique des shots (sur la machine) =====================
// Enregistre le shot termine dans l'anneau EEPROM (10 derniers).
void recordShot() {
  if (!shot.valid) return;
  ShotRecord &r = settings.history[settings.histIdx];
  r.durS = shot.durMs / 1000;
  r.pMax10 = (uint8_t)(shot.maxP * 10);
  r.pAvg10 = (uint8_t)((shot.count ? shot.sumP / shot.count : 0) * 10);
  r.waterMl = (uint8_t)liveMl;
  r.finalG10 = (uint16_t)(finalWeight * 10);
  r.over10 = (int8_t)(overshootMeasured * 10);
  r.tau100 = (uint8_t)(settings.profiles[settings.active].overshootTau * 100);
  settings.histIdx = (settings.histIdx + 1) % HIST_N;
  if (settings.histCount < HIST_N) settings.histCount++;
}

// Retourne le i-eme shot le plus recent (0 = le dernier)
ShotRecord& histGet(uint8_t i) {
  uint8_t idx = (settings.histIdx + HIST_N - 1 - i) % HIST_N;
  return settings.history[idx];
}

void drawHistory() {
  char buf[24], f1[8], f2[8];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  if (settings.histCount == 0) {
    u8g2.setCursor(0, 10);
    u8g2.print("HISTORIQUE");
    u8g2.setCursor(0, 26);
    u8g2.print("Aucun shot enregistre");
    u8g2.setCursor(0, 58);
    u8g2.print("clic = retour");
    u8g2.sendBuffer();
    return;
  }
  ShotRecord &r = histGet(histSel);
  u8g2.setCursor(0, 10);
  snprintf(buf, sizeof(buf), "HISTORIQUE %u/%u", histSel + 1, settings.histCount);
  u8g2.print(buf);
  // Ligne 2 : duree + Pmax/Pmoy
  u8g2.setCursor(0, 22);
  dtostrf((float)r.pMax10 / 10.0f, 0, 1, f1);
  dtostrf((float)r.pAvg10 / 10.0f, 0, 1, f2);
  snprintf(buf, sizeof(buf), "%us Pmax %s Pmoy %s", r.durS, f1, f2);
  u8g2.print(buf);
  // Ligne 3 : eau + tasse
  u8g2.setCursor(0, 34);
  if (r.finalG10 > 0) {
    dtostrf((float)r.finalG10 / 10.0f, 0, 1, f1);
    snprintf(buf, sizeof(buf), "Eau %u mL Tasse %s g", r.waterMl, f1);
  } else {
    snprintf(buf, sizeof(buf), "Eau %u mL", r.waterMl);
  }
  u8g2.print(buf);
  // Ligne 4 : overshoot + tau (mode grammes)
  if (r.finalG10 > 0) {
    u8g2.setCursor(0, 46);
    dtostrf((float)r.over10 / 10.0f, 0, 1, f1);
    dtostrf((float)r.tau100 / 100.0f, 0, 2, f2);
    snprintf(buf, sizeof(buf), "Over %s%s g Tau %s s", r.over10 >= 0 ? "+" : "", f1, f2);
    u8g2.print(buf);
  }
  // Ligne 5 : navigation (police 5x7 : 25 caracteres tiennent)
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.setCursor(0, 58);
  u8g2.print("tourner=shot clic=retour");
  u8g2.sendBuffer();
}

// ===================== Courbe de pression (dernier shot) =====================
void drawCurve() {
  Profile &p = settings.profiles[settings.active];
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 10);
  u8g2.print("COURBE PRESSION");
  if (curveLen == 0) {
    u8g2.setCursor(0, 26);
    u8g2.print("Aucun shot enregistre");
    u8g2.setCursor(0, 58);
    u8g2.print("clic = retour");
    u8g2.sendBuffer();
    return;
  }
  // Cadre : x 0-127, y 13-53 (0-12 bar). Ligne pointillee = cible du profil.
  uint8_t yT = 52 - (uint8_t)(p.pressureTarget / 12.0f * 38.0f);
  for (uint8_t x = 0; x < 128; x += 6) u8g2.drawHLine(x, yT, 3);
  for (uint8_t i = 1; i < curveLen; i++) {
    uint8_t x0 = (uint8_t)((i - 1) * 127 / (CURVE_N - 1));
    uint8_t x1 = (uint8_t)(i * 127 / (CURVE_N - 1));
    uint8_t y0 = 52 - (uint8_t)(curveBuf[i - 1] / 12.0f * 38.0f);
    uint8_t y1 = 52 - (uint8_t)(curveBuf[i] / 12.0f * 38.0f);
    u8g2.drawLine(x0, y0, x1, y1);
  }
  u8g2.drawFrame(0, 13, 128, 41);
  u8g2.setCursor(0, 63);
  u8g2.print("clic = retour");
  u8g2.sendBuffer();
}

void drawFault() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_10x20_tf);
  u8g2.setCursor(28, 28);
  u8g2.print("FAULT");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(0, 44);
  if (fault == F_THERM_BOILER) u8g2.print("Sonde chaudiere HS");
  if (fault == F_OVERTEMP)    u8g2.print("Surchauffe >165C");
  if (fault == F_OVERPRESSURE) u8g2.print("Surpression >12.5 bar");
  u8g2.setCursor(0, 56);
  u8g2.print("Chauffage coupe. Reset:");
  u8g2.setCursor(0, 63);
  u8g2.print("eteindre / rallumer.");
  u8g2.sendBuffer();
}

// ===================== Setup / Loop =====================
void setup() {
  pinMode(PIN_HEATER_SSR, OUTPUT);
  pinMode(PIN_SOLENOID_SSR, OUTPUT);
  digitalWrite(PIN_HEATER_SSR, LOW);
  digitalWrite(PIN_SOLENOID_SSR, LOW);
  pinMode(PIN_STEAM_SW, INPUT_PULLUP);
  pinMode(PIN_BREW_SW, INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  pinMode(PIN_FLOW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), flowISR, RISING);
  pinMode(PIN_HX711_DOUT, INPUT);
  pinMode(PIN_HX711_SCK, OUTPUT);
  digitalWrite(PIN_HX711_SCK, LOW);
  // Dimmer pompe : gate en sortie, zero-cross en interruption (D3)
  pinMode(PIN_DIM_GATE, OUTPUT);
  digitalWrite(PIN_DIM_GATE, LOW);
  pinMode(PIN_DIM_ZC, INPUT);
  TCCR1A = 0;
  TCCR1B = (1 << WGM12) | (1 << CS11);  // CTC, prescaler 8 -> 0,5 us/tick
  TIMSK1 = 0;
  attachInterrupt(digitalPinToInterrupt(PIN_DIM_ZC), zcISR, RISING);

  // Profils sauvegardes (EEPROM) ou defauts
  Settings e;
  EEPROM.get(0, e);
  if (e.magic == EEPROM_MAGIC) settings = e;
  else { loadDefaults(); EEPROM.put(0, settings); }
  // PID global : charge les constantes sauvegardees dans le PID live
  pid.kp = settings.kp; pid.ki = settings.ki; pid.kd = settings.kd;
  pidPress.kp = settings.pkp; pidPress.ki = settings.pki; pidPress.kd = settings.pkd;
  applyProfile();

  u8g2.begin();
  u8g2.setContrast(200);
  Serial.begin(115200);
  Serial.println("ms,profil,tBoilerC,pBar,pTarget,dimLevel,flowMlS,waterMl,cupG,phase,shotS");
  Serial.println("Gaggia RAMPS PID v2 — demarrage");
  pid.reset(millis());
  winStartMs = millis();
}

void loop() {
  unsigned long now = millis();

  // ----- Mode vapeur : bouton STEAM d'origine (niveau) -----
  steamMode = (digitalRead(PIN_STEAM_SW) == LOW);

  // ----- Bouton BREW -----
  pollBrewSwitch();

  // ----- Lecture capteurs (toutes les 250 ms) -----
  if (now - lastTempMs >= LOOP_TEMP_MS) {
    lastTempMs = now;
    bool bOk = true;
    tBoiler = readThermistorC(PIN_THERM_BOILER, bOk);
    tGroup = readThermistorC(PIN_THERM_GROUP, gOk);
    pBar = readPressureBar(pOk);

    if (!bOk) fault = F_THERM_BOILER;
    else if (tBoiler > TEMP_FAULT_MAX) fault = F_OVERTEMP;
    else if (pBar > 12.5f) fault = F_OVERPRESSURE;

    if (fault != F_NONE) {
      digitalWrite(PIN_HEATER_SSR, LOW);
      if (brewing) stopShot();
      if (tune.active) cancelAutotune();
    } else if (tune.active) {
      autotuneStep(tBoiler, now);
    } else {
      Profile &p = settings.profiles[settings.active];
      float sp = steamMode ? settings.steamTemp : p.brewTemp;
      pidOut = pid.compute(sp, tBoiler, now);
    }

    // ----- Debitmetre (diagnostic permanent) + balance + stats shot -----
    if (brewing) {
      unsigned long pNow;
      noInterrupts(); pNow = flowPulses; interrupts();
      if (pNow > 0) flowSeen = true;
      flowHist[flowHistIdx] = pNow;
      flowHistIdx = (flowHistIdx + 1) % FLOW_HIST_N;
      uint8_t fNew = (flowHistIdx + FLOW_HIST_N - 1) % FLOW_HIST_N;
      uint8_t fOld = flowHistIdx;  // 1 s plus tot
      flowRateMlS = (float)(flowHist[fNew] - flowHist[fOld]) / FLOW_PULSES_PER_ML;
      liveMl = (float)pNow / FLOW_PULSES_PER_ML;
      if (settings.shotMode == SHOT_WEIGHT) {
        bool wOk = true;
        liveGrams = readWeightGrams(wOk);
        if (!wOk) scaleBadTicks++; else scaleBadTicks = 0;
        weightHist[weightHistIdx] = liveGrams;
        weightHistIdx = (weightHistIdx + 1) % WEIGHT_HIST_N;
        uint8_t wNew = (weightHistIdx + WEIGHT_HIST_N - 1) % WEIGHT_HIST_N;
        uint8_t wOld = weightHistIdx;  // 2 s plus tot
        flowRateGs = (weightHist[wNew] - weightHist[wOld]) / 2.0f;
      }
      // Stats du shot (resume) + courbe pression
      if (pOk) {
        curvePush(pBar);
        if (pBar > shot.maxP) shot.maxP = pBar;
        shot.sumP += pBar; shot.count++;
      }
      if (tBoiler < shot.minT) shot.minT = tBoiler;
      if (tBoiler > shot.maxT) shot.maxT = tBoiler;
      // Diagnostics debit/pression (AVERTISSEMENT seulement, jamais d'arret) :
      // - pression haute + debit faible = puck restrictif (mouture fine : legitime)
      // - debit eleve + pression qui ne monte pas = puck permissif / fuite
      // Seuils de depart, a tuner avec les logs reels.
      if (flowSeen && pOk && (shotPhase == PH_BREW || shotPhase == PH_DECLINE)) {
        if (pBar >= 8.0f && flowRateMlS < 0.5f) warnRestrTicks++; else warnRestrTicks = 0;
        if (flowRateMlS > 3.0f && pBar < 4.0f) warnPermTicks++; else warnPermTicks = 0;
        if (warnRestrTicks >= 20) strcpy(shotWarn, "RESTRICTIF");
        if (warnPermTicks >= 20) strcpy(shotWarn, "PERMISSIF");
      }
    }

    // ----- Controle pression (dimmer) pendant le shot -----
    if (brewing) {
      Profile &p = settings.profiles[settings.active];
      unsigned long phElapsed = now - phaseStartMs;
      // Transitions de phase : preinf -> rampe -> brew -> declin
      if (shotPhase == PH_PREINF && phElapsed >= (unsigned long)p.preinfSecs * 1000UL) {
        shotPhase = (p.rampSecs > 0) ? PH_RAMP : PH_BREW;
        phaseStartMs = now;
      } else if (shotPhase == PH_RAMP && phElapsed >= (unsigned long)p.rampSecs * 1000UL) {
        shotPhase = PH_BREW;
        phaseStartMs = now;
      } else if (shotPhase == PH_BREW && p.declineSecs > 0 && phElapsed >= (unsigned long)p.declineSecs * 1000UL) {
        shotPhase = PH_DECLINE;
        phaseStartMs = now;
      }
      // Consigne selon la phase (interpolation pendant la rampe)
      float pTarget;
      if (shotPhase == PH_PREINF) pTarget = p.preinfPressure;
      else if (shotPhase == PH_RAMP) {
        float f = (p.rampSecs > 0) ? (float)phElapsed / ((float)p.rampSecs * 1000.0f) : 1.0f;
        if (f > 1.0f) f = 1.0f;
        pTarget = p.preinfPressure + (p.pressureTarget - p.preinfPressure) * f;
      }
      else if (shotPhase == PH_DECLINE) pTarget = p.declinePressure;
      else pTarget = p.pressureTarget;
      pTargetLive = pTarget;
      if (!pOk) {
        dimmerSet(100);  // capteur pression HS : pleine pompe (comme avant)
      } else {
        float lvl = pidPress.compute(pTarget, pBar, now);
        if (lvl > 0 && lvl < DIM_MIN_LEVEL) lvl = DIM_MIN_LEVEL;  // la pompe cale en bas
        dimmerSet((uint8_t)lvl);
      }
    }

    // ----- Arret auto du shot -----
    if (brewing) {
      Profile &p = settings.profiles[settings.active];
      unsigned long elapsed = now - shotStartMs;
      bool stop = false;
      if (settings.shotMode == SHOT_TIMER) stop = elapsed >= (unsigned long)p.shotSecs * 1000UL;
      else if (settings.shotMode == SHOT_FLOW) stop = (liveMl >= p.shotMl) || (elapsed >= (unsigned long)SHOT_MAX_SECS * 1000UL);
      else if (settings.shotMode == SHOT_WEIGHT) {
        // Anticipation : couper AVANT la cible (l'eau continue de couler).
        // cutoff = cible - tau * g/s (tau appris par profil, EMA).
        float anticip = p.overshootTau * flowRateGs;
        if (anticip < 0) anticip = 0;
        if (anticip > 5.0f) anticip = 5.0f;  // jamais plus de 5 g d'avance
        float cutoff = (float)p.shotGrams - anticip;
        stop = (liveGrams >= cutoff) || (elapsed >= (unsigned long)SHOT_MAX_SECS * 1000UL);
      }
      // Securite : temps max global 90 s (tous modes, manuel inclus)
      if (elapsed >= 90000UL) { strcpy(stopReason, "90s MAX"); stop = true; }
      // Securite : pas de debit (debitmetre present mais plus rien ne coule)
      if (flowSeen && shotPhase != PH_PREINF) {
        if (flowRateMlS <= 0.001f) noFlowTicks++; else noFlowTicks = 0;
        if (noFlowTicks >= 20) { strcpy(stopReason, "PAS DE DEBIT"); stop = true; }
      }
      // Securite : perte du signal balance (mode grammes)
      if (settings.shotMode == SHOT_WEIGHT && scaleBadTicks >= 8) {
        strcpy(stopReason, "BALANCE HS"); stop = true;
      }
      if (stop) {
        weightAtCutoff = liveGrams;
        flowRateGsAtCutoff = flowRateGs;
        stopShot();
      }
    }

    // ----- Stabilisation balance apres le shot (mode grammes) -----
    if (settling) {
      bool wOk = true;
      float w = readWeightGrams(wOk);
      if (wOk) liveGrams = w;
      else scaleBadTicks++;
      if (!wOk && scaleBadTicks >= 8) {
        settling = false;  // balance perdue : on garde le dernier poids
        finalWeight = liveGrams;
        overshootMeasured = finalWeight - weightAtCutoff;
        recordShot();  // tau non appris (balance perdue)
        showSummary = true;
        summaryMs = now;
      } else {
        if (fabsf(liveGrams - settlePrev) < 0.05f) settleStableTicks++;
        else settleStableTicks = 0;
        settlePrev = liveGrams;
        bool settledClean = (settleStableTicks >= 4);
        if (settledClean || (now - settleStartMs) >= 8000UL) {
          // Poids final (stabilise ou timeout) : mesure l'overshoot.
          finalWeight = liveGrams;
          overshootMeasured = finalWeight - weightAtCutoff;
          Profile &p = settings.profiles[settings.active];
          // PROTECTION DE L'APPRENTISSAGE : seul un shot termine
          // normalement en mode grammes met a jour tau. Exclus : arret
          // manuel, FAULT, arret de securite, balance instable (timeout),
          // poids final aberrant (overshoot < 0 ou > 4 g).
          bool learn = !manualStop && stopReason[0] == '\0' && fault == F_NONE
                       && settledClean
                       && overshootMeasured >= 0 && overshootMeasured <= 4.0f;
          tauLearned = false;
          if (learn) {
            float rate = flowRateGsAtCutoff;
            if (rate < 0.5f) rate = 0.5f;  // evite la division par ~0
            float tauMes = overshootMeasured / rate;
            if (tauMes < 0) tauMes = 0;
            if (tauMes > 2.0f) tauMes = 2.0f;  // aberrant : borne
            p.overshootTau = (1.0f - TAU_EMA_ALPHA) * p.overshootTau + TAU_EMA_ALPHA * tauMes;
            EEPROM.put(0, settings);  // persiste l'apprentissage
            tauLearned = true;
          }
          recordShot();  // historique (tau deja mis a jour si appris)
          settling = false;
          showSummary = true;
          summaryMs = now;
        }
      }
    }
  }

  // ----- Modulation SSR chauffage (fenetre 1 s, sauf en autotune) -----
  if (fault == F_NONE && !tune.active) {
    if (now - winStartMs >= PID_WINDOW_MS) winStartMs = now;
    bool heatOn = (now - winStartMs) < (unsigned long)(pidOut * PID_WINDOW_MS / 100.0f);
    digitalWrite(PIN_HEATER_SSR, heatOn ? HIGH : LOW);
  }

  // ----- Encodeur -----
  int8_t d = encDelta();
  bool clicked = encClicked();
  if (tune.active) {
    if (clicked) cancelAutotune();
  } else if (tune.showResult) {
    if (clicked || (now - tune.resultMs > 5000)) tune.showResult = false;
  } else if (showSummary) {
    if (clicked || (now - summaryMs > 8000)) showSummary = false;
  } else if (inMenu) {
    menuHandle(d, clicked, now);
    if (inMenu && (now - menuLastActMs > 10000)) inMenuStop();  // timeout sans sauver
  } else {
    // Ecran principal : tourner = changer de profil, clic = menu config
    if (d != 0) switchProfile(settings.active + d);
    if (clicked) {
      inMenu = true; menuSel = 0; editItem = -1; editingName = false; menuTop = 0;
      menuLastActMs = now;
    }
  }

  // ----- Affichage (toutes les 200 ms) -----
  if (now - lastDispMs >= 200) {
    lastDispMs = now;
    if (fault != F_NONE) drawFault();
    else if (tune.active) drawTune();
    else if (tune.showResult) drawTuneResult();
    else if (showSummary) drawSummary();
    else if (inMenu) { if (inHistory) drawHistory(); else if (inCurve) drawCurve(); else if (editingName) drawNameEdit(); else drawMenu(); }
    else if (brewing || settling) drawShot();
    else drawMain();
  }

  // ----- Log serie CSV (pour tuner PID pression + tau sur donnees reelles) -----
  // Pendant le shot : une ligne par tick (250 ms). Sinon : 1/s.
  // Colonnes : ms,profil,tBoilerC,pBar,pTarget,dimLevel,flowMlS,waterMl,cupG,phase,shotS
  // phase : 0=idle, 1=preinf, 2=rampe, 3=brew, 4=declin
  if ((brewing && (now - lastSerMs >= 250)) || (!brewing && (now - lastSerMs >= 1000))) {
    lastSerMs = now;
    Serial.print(now); Serial.print(",");
    Serial.print(settings.active + 1); Serial.print(",");
    Serial.print(tBoiler, 1); Serial.print(",");
    Serial.print(pBar, 2); Serial.print(",");
    Serial.print(brewing ? pTargetLive : 0, 1); Serial.print(",");
    Serial.print(dimLevel); Serial.print(",");
    Serial.print(flowRateMlS, 2); Serial.print(",");
    Serial.print(liveMl, 1); Serial.print(",");
    Serial.print(liveGrams, 1); Serial.print(",");
    Serial.print((int)shotPhase); Serial.print(",");
    Serial.println(brewing ? (millis() - shotStartMs) / 1000.0f : 0);
  }
}
