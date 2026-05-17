/**
 * Leek PCB Keychain Firmware
 * Diva Engineering — https://github.com/diva-eng
 *
 * Hardware: ATtiny402-SS (SOT-23-8) — MANUFACTURED PCB
 * Toolchain: Arduino IDE + megaTinyCore
 * Board manager: http://drazzy.com/package_drazzy.com_index.json
 * Board: ATtiny402 | Clock: Internal 5MHz | Programmer: SerialUPDI / SNAP
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * PIN MAPPING (final PCB, from schematic)
 * ─────────────────────────────────────────────────────────────────────────────
 *
 *   PA0 / RESET  —  UPDI (programming only, do not use in firmware)
 *   PA1          —  GREEN_GROUP_1  — leek TIP        (2× green, topmost)
 *   PA2          —  SELECT button  — active LOW, internal pullup
 *   PA3          —  WHITE_GROUP_1  — below green group 2
 *   PA6          —  GREEN_GROUP_2  — 3× green, below tip
 *   PA7          —  WHITE_GROUP_2  — bottommost single LED
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * PHYSICAL LED ORDER top → bottom
 * ─────────────────────────────────────────────────────────────────────────────
 *
 *   [1] GREEN_GROUP_1  PA1  — tip         (2× green)
 *   [2] GREEN_GROUP_2  PA6  — upper stalk (3× green)
 *   [3] WHITE_GROUP_1  PA3  — lower stalk (1× white)
 *   [4] WHITE_GROUP_2  PA7  — base        (1× white)
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * ADDING A NEW PATTERN
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * 1. Choose a category: WAVE, BREATH, or BLINK (or add a new category label).
 *
 * 2. Write your pattern function following this signature:
 *      void myPattern(bool firstRun);
 *    - firstRun is true on the first call after switching to this pattern.
 *      Use it to reset any static state inside your function.
 *    - NEVER use delay() inside a pattern. Use the wait() helper instead.
 *    - Use the LED helpers: setTip(), setUpperStalk(), setLowerStalk(),
 *      setBase(), setAllGreen(), setAllWhite(), setAll(), allOff().
 *    - Use sineWave(phase, lo, hi) for smooth fading.
 *
 * 3. Register it in the PATTERNS[] table at the bottom of this file:
 *      { CAT_WAVE,  myPattern,  "My Pattern" }
 *
 * NUM_PATTERNS is calculated automatically. Nothing else needs changing.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <EEPROM.h>

// ═══════════════════════════════════════════════════════════════════════════════
// HARDWARE PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════════════════════════

#define PIN_TIP           PIN_PA1   // GREEN_GROUP_1 — leek tip       (2× green)
#define PIN_UPPER_STALK   PIN_PA6   // GREEN_GROUP_2 — upper stalk    (3× green)
#define PIN_LOWER_STALK   PIN_PA3   // WHITE_GROUP_1 — lower stalk    (1× white)
#define PIN_BASE          PIN_PA7   // WHITE_GROUP_2 — base           (1× white)
#define PIN_BUTTON        PIN_PA2   // SELECT button — active LOW

// ═══════════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

#define EEPROM_PATTERN_ADDR   0       // EEPROM byte address for pattern persistence
#define DEBOUNCE_MS           50      // Button debounce window (ms)
#define PWM_MAX               255     // Full brightness PWM value
#define PWM_DIM               30      // Dim glow level used by some patterns

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN CATEGORIES
// Used for grouping only — no functional effect, helps readability.
// ═══════════════════════════════════════════════════════════════════════════════

#define CAT_WAVE    0
#define CAT_BREATH  1
#define CAT_BLINK   2

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN TYPE
// ═══════════════════════════════════════════════════════════════════════════════

typedef void (*PatternFn)(bool firstRun);

struct Pattern {
  uint8_t     category;   // CAT_WAVE / CAT_BREATH / CAT_BLINK
  PatternFn   fn;
  const char* name;
};

// ═══════════════════════════════════════════════════════════════════════════════
// LED HELPERS
// Named after physical position on the leek for readability in pattern code.
// ═══════════════════════════════════════════════════════════════════════════════

// Individual groups — top to bottom
inline void setTip        (uint8_t v) { analogWrite(PIN_TIP,          v); }
inline void setUpperStalk (uint8_t v) { analogWrite(PIN_UPPER_STALK,  v); }
inline void setLowerStalk (uint8_t v) { analogWrite(PIN_LOWER_STALK,  v); }
inline void setBase       (uint8_t v) { analogWrite(PIN_BASE,         v); }

// Convenience group setters
inline void setAllGreen(uint8_t v)    { setTip(v);  setUpperStalk(v); }
inline void setAllWhite(uint8_t v)    { setLowerStalk(v); setBase(v); }
inline void setAll(uint8_t v)         { setAllGreen(v); setAllWhite(v); }
inline void allOff()                  { setAll(0); }

// Set all four groups individually in one call (top → bottom order)
inline void setGroups(uint8_t tip, uint8_t upper, uint8_t lower, uint8_t base) {
  setTip(tip);
  setUpperStalk(upper);
  setLowerStalk(lower);
  setBase(base);
}

// ═══════════════════════════════════════════════════════════════════════════════
// TIMING HELPER
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Non-blocking wait. Returns true when 'duration' ms have passed since 'since'.
 *
 * Usage:
 *   static uint32_t t = 0;
 *   if (wait(t, 200)) { t = millis(); doThing(); }
 */
inline bool wait(uint32_t since, uint32_t duration) {
  return (millis() - since) >= duration;
}

// ═══════════════════════════════════════════════════════════════════════════════
// WAVEFORM HELPER
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Fast triangle-wave approximation of a sine curve (avoids sin() on AVR).
 * phase : 0–255 = one full cycle
 * lo    : minimum output brightness  (default 0)
 * hi    : maximum output brightness  (default 255)
 */
uint8_t sineWave(uint8_t phase, uint8_t lo = 0, uint8_t hi = PWM_MAX) {
  uint8_t tri = (phase < 128) ? (phase * 2) : (255 - (phase - 128) * 2);
  return lo + (uint8_t)(((uint16_t)(hi - lo) * tri) / 255);
}

// ═══════════════════════════════════════════════════════════════════════════════
// ── WAVE PATTERNS ─────────────────────────────────────────────────────────────
// Groups light up in sequence top → bottom, like energy running down the leek.
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Wave — Slow Cascade Down
 * Each group fades in one at a time, tip → base, then all fade out together.
 * Gentle, meditative. Good for low-key moments.
 * Step hold: 350ms per group, 700ms all-on, 400ms off, 300ms pause.
 */
void waveSlowCascade(bool firstRun) {
  static uint32_t t    = 0;
  static uint8_t  step = 0;

  //                         tip    upper  lower  base   hold(ms)
  static const uint16_t dur[] = { 350, 350, 350, 350, 700, 400, 300 };

  if (firstRun) { step = 0; t = millis(); allOff(); }

  if (wait(t, dur[step])) {
    t = millis();
    step = (step + 1) % 7;
    switch (step) {
      case 0: allOff();                                                    break;
      case 1: setGroups(PWM_MAX, 0,       0,       0);                    break;
      case 2: setGroups(PWM_MAX, PWM_MAX, 0,       0);                    break;
      case 3: setGroups(PWM_MAX, PWM_MAX, PWM_MAX, 0);                    break;
      case 4: setAll(PWM_MAX);                                             break;
      case 5: allOff();                                                    break;
      case 6: /* pause before repeat */                                    break;
    }
  }
}

/**
 * Wave — Fast Pulse Up
 * A quick flash travels bottom → tip rapidly, like a spark running up the leek.
 * Each group on for 120ms with a brief off gap between runs.
 */
void wavePulseUp(bool firstRun) {
  static uint32_t t    = 0;
  static uint8_t  step = 0;

  // Steps: base → lower → upper → tip → off gap
  static const uint16_t dur[] = { 120, 120, 120, 120, 250 };

  if (firstRun) { step = 0; t = millis(); allOff(); }

  if (wait(t, dur[step])) {
    t = millis();
    step = (step + 1) % 5;
    allOff();
    switch (step) {
      case 0: setBase(PWM_MAX);        break;
      case 1: setLowerStalk(PWM_MAX);  break;
      case 2: setUpperStalk(PWM_MAX);  break;
      case 3: setTip(PWM_MAX);         break;
      case 4: /* off gap */            break;
    }
  }
}

/**
 * Wave — Ripple
 * Groups light in overlapping sequence so the transition looks fluid.
 * Uses PWM phases offset by 64 steps (quarter cycle) between groups.
 * All groups active simultaneously but at different phases.
 */
void waveRipple(bool firstRun) {
  static uint32_t t     = 0;
  static uint8_t  phase = 0;

  if (firstRun) { phase = 0; t = millis(); }

  if (wait(t, 14)) {
    t = millis();
    phase++;
    // Each group is offset by 64 (quarter cycle), tip leads
    setTip        (sineWave((uint8_t)(phase),        8, PWM_MAX));
    setUpperStalk (sineWave((uint8_t)(phase + 64),   8, PWM_MAX));
    setLowerStalk (sineWave((uint8_t)(phase + 128),  8, PWM_MAX));
    setBase       (sineWave((uint8_t)(phase + 192),  8, PWM_MAX));
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ── BREATH PATTERNS ───────────────────────────────────────────────────────────
// Smooth PWM fading. Easy on the eyes for sustained watching.
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Breath — All Together
 * Every LED group breathes in and out in sync.
 * Full cycle ~3 seconds. Most calming pattern.
 */
void breathAll(bool firstRun) {
  static uint32_t t     = 0;
  static uint8_t  phase = 0;

  if (firstRun) { phase = 0; t = millis(); }

  if (wait(t, 12)) {
    t = millis();
    phase++;
    setAll(sineWave(phase, 4, PWM_MAX));
  }
}

/**
 * Breath — Green / White Crossfade
 * Green groups and white groups breathe opposite each other.
 * As greens brighten, whites dim, and vice versa.
 */
void breathCrossfade(bool firstRun) {
  static uint32_t t     = 0;
  static uint8_t  phase = 0;

  if (firstRun) { phase = 0; t = millis(); }

  if (wait(t, 10)) {
    t = millis();
    phase++;
    setAllGreen(sineWave(phase,             4, PWM_MAX));
    setAllWhite(sineWave((uint8_t)(phase + 128), 4, PWM_MAX));
  }
}

/**
 * Breath — Tip Focus
 * Tip pulses fully while the rest of the leek holds a soft constant dim glow.
 * Draws the eye to the top of the leek.
 */
void breathTipFocus(bool firstRun) {
  static uint32_t t     = 0;
  static uint8_t  phase = 0;

  if (firstRun) { phase = 0; t = millis(); }

  if (wait(t, 9)) {
    t = millis();
    phase++;
    setTip(sineWave(phase, 10, PWM_MAX));
    setUpperStalk(PWM_DIM);
    setLowerStalk(PWM_DIM / 2);
    setBase(PWM_DIM / 2);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ── BLINK PATTERNS ────────────────────────────────────────────────────────────
// Hard on/off transitions. More energetic, concert-appropriate.
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Blink — All Together
 * All LEDs flash on and off in unison.
 * 500ms on, 500ms off.
 */
void blinkAll(bool firstRun) {
  static uint32_t t      = 0;
  static bool     isOn   = false;

  if (firstRun) { isOn = false; t = millis(); allOff(); }

  if (wait(t, 500)) {
    t = millis();
    isOn = !isOn;
    isOn ? setAll(PWM_MAX) : allOff();
  }
}

/**
 * Blink — Alternate Green / White
 * Green groups and white groups flash alternately.
 * 350ms per group.
 */
void blinkAlternate(bool firstRun) {
  static uint32_t t        = 0;
  static bool     greenOn  = true;

  if (firstRun) { greenOn = true; t = millis(); allOff(); }

  if (wait(t, 350)) {
    t = millis();
    greenOn = !greenOn;
    setAllGreen(greenOn  ? PWM_MAX : 0);
    setAllWhite(!greenOn ? PWM_MAX : 0);
  }
}

/**
 * Blink — Heartbeat
 * Double-flash all LEDs: thump-thump ... long pause.
 * Timings: 80ms on, 100ms off, 80ms on, 900ms pause.
 */
void blinkHeartbeat(bool firstRun) {
  static uint32_t t    = 0;
  static uint8_t  step = 0;

  static const uint16_t dur[] = { 80, 100, 80, 900 };

  if (firstRun) { step = 0; t = millis(); allOff(); }

  if (wait(t, dur[step])) {
    t = millis();
    step = (step + 1) % 4;
    (step == 0 || step == 2) ? setAll(PWM_MAX) : allOff();
  }
}

/**
 * Blink — SOS
 * Morse code SOS on all LEDs (··· — — — ···).
 * Easter egg for concert emergencies.
 */
void blinkSOS(bool firstRun) {
  // Each pair: [on duration, off/gap duration] in ms
  // dot=180ms  dash=540ms  symbol gap=180ms  letter gap=540ms  word gap=1800ms
  static const uint16_t seq[] = {
    180, 180,    // S dot 1
    180, 180,    // S dot 2
    180, 540,    // S dot 3  + letter gap
    540, 180,    // O dash 1
    540, 180,    // O dash 2
    540, 540,    // O dash 3 + letter gap
    180, 180,    // S dot 1
    180, 180,    // S dot 2
    180, 1800,   // S dot 3  + word gap before repeat
  };
  static const uint8_t SEQ_LEN = sizeof(seq) / sizeof(seq[0]);

  static uint32_t t    = 0;
  static uint8_t  step = 0;

  if (firstRun) { step = 0; t = millis(); allOff(); }

  if (wait(t, seq[step])) {
    t = millis();
    step = (step + 1) % SEQ_LEN;
    (step % 2 == 0) ? setAll(PWM_MAX) : allOff();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN REGISTRY
// ─────────────────────────────────────────────────────────────────────────────
// Patterns cycle in the order listed here when the button is pressed.
// Group related patterns together using CAT_* labels for clarity.
// Add new patterns by appending a new { CAT_*, functionName, "Name" } line.
// ═══════════════════════════════════════════════════════════════════════════════

static const Pattern PATTERNS[] = {
  // ── WAVE ──────────────────────────────────────────────────────────────────
  { CAT_WAVE,   waveSlowCascade,  "Wave: Cascade"   },
  { CAT_WAVE,   wavePulseUp,      "Wave: Pulse Up"  },
  { CAT_WAVE,   waveRipple,       "Wave: Ripple"    },

  // ── BREATH ────────────────────────────────────────────────────────────────
  { CAT_BREATH, breathAll,        "Breath: All"     },
  { CAT_BREATH, breathCrossfade,  "Breath: Cross"   },
  { CAT_BREATH, breathTipFocus,   "Breath: Tip"     },

  // ── BLINK ─────────────────────────────────────────────────────────────────
  { CAT_BLINK,  blinkAll,         "Blink: All"      },
  { CAT_BLINK,  blinkAlternate,   "Blink: Alternate"},
  { CAT_BLINK,  blinkHeartbeat,   "Blink: Heartbeat"},
  { CAT_BLINK,  blinkSOS,         "Blink: SOS"      },
};

static const uint8_t NUM_PATTERNS = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

// ═══════════════════════════════════════════════════════════════════════════════
// RUNTIME STATE
// ═══════════════════════════════════════════════════════════════════════════════

static uint8_t currentPattern  = 0;
static bool    patternFirstRun = true;

// ═══════════════════════════════════════════════════════════════════════════════
// BUTTON HANDLER
// ═══════════════════════════════════════════════════════════════════════════════

void handleButton() {
  if (digitalRead(PIN_BUTTON) != LOW) return;

  delay(DEBOUNCE_MS);
  if (digitalRead(PIN_BUTTON) != LOW) return;

  // Advance to next pattern
  currentPattern = (currentPattern + 1) % NUM_PATTERNS;

  // Persist to EEPROM — update() only writes if value changed (saves write cycles)
  EEPROM.update(EEPROM_PATTERN_ADDR, currentPattern);

  // Brief blackout so the user feels the pattern change
  allOff();
  delay(120);

  patternFirstRun = true;

  // Wait for button release before continuing
  while (digitalRead(PIN_BUTTON) == LOW) {}
  delay(DEBOUNCE_MS);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  pinMode(PIN_TIP,          OUTPUT);
  pinMode(PIN_UPPER_STALK,  OUTPUT);
  pinMode(PIN_LOWER_STALK,  OUTPUT);
  pinMode(PIN_BASE,         OUTPUT);
  pinMode(PIN_BUTTON,       INPUT_PULLUP);

  allOff();

  // Restore last used pattern from EEPROM
  uint8_t saved = EEPROM.read(EEPROM_PATTERN_ADDR);
  if (saved < NUM_PATTERNS) {
    currentPattern = saved;
  } else {
    // EEPROM uninitialised (0xFF on fresh chip) — default to first pattern
    currentPattern = 0;
    EEPROM.write(EEPROM_PATTERN_ADDR, 0);
  }

  patternFirstRun = true;
}

void loop() {
  handleButton();
  PATTERNS[currentPattern].fn(patternFirstRun);
  patternFirstRun = false;
}
