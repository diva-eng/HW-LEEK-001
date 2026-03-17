/**
 * Leek PCB Keychain Firmware
 * Diva Engineering — https://github.com/diva-eng
 *
 * Hardware: ATtiny402-SS (SOT-23-8)
 * Toolchain: Arduino IDE + megaTinyCore
 *
 * Pin mapping (from schematic):
 *   PA1 — UPDI (programming only, do not use)
 *   PA2 — SELECT button (active LOW, internal pullup)
 *   PA6 — GREEN_LED group (PWM capable via TCA0)
 *   PA7 — WHITE_LED group (PWM capable via TCA0)
 *
 * Board settings (Arduino IDE):
 *   Board:       ATtiny402
 *   Clock:       Internal 5MHz (or 1MHz for lower power)
 *   Programmer:  SerialUPDI / SNAP
 *
 * ─────────────────────────────────────────────────────────────────────────────
 * ADDING A NEW PATTERN
 * ─────────────────────────────────────────────────────────────────────────────
 * 1. Write a void function following the pattern runner signature:
 *      void myPattern(bool firstRun);
 *    Use firstRun to initialise any static state on first call.
 *    Call stepComplete() when one animation cycle step is done.
 *    Return without blocking — keep the loop() responsive.
 *
 * 2. Add an entry to the PATTERNS array at the bottom of this file:
 *      { myPattern, "My Pattern" }
 *
 * That's it. NUM_PATTERNS is calculated automatically.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <EEPROM.h>

// ═══════════════════════════════════════════════════════════════════════════════
// PIN DEFINITIONS
// ═══════════════════════════════════════════════════════════════════════════════

#define PIN_GREEN   PIN_PA6   // Green LED group
#define PIN_WHITE   PIN_PA7   // White LED group


















#define PIN_BUTTON  PIN_PA2   // Pattern select   (schematic: SELECT    / pin 5)

// ═══════════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

#define EEPROM_PATTERN_ADDR   0       // EEPROM address to persist pattern index
#define DEBOUNCE_MS           50      // Button debounce window in milliseconds
#define PWM_MAX               255     // Maximum PWM value (do not change)

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN RUNNER TYPE
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * PatternFn — function pointer type for all pattern functions.
 * @param firstRun  true on the very first call after a pattern switch,
 *                  use this to reset any static state inside the function.
 */
typedef void (*PatternFn)(bool firstRun);

struct Pattern {
  PatternFn fn;
  const char* name;
};

// ═══════════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Set LED brightness via PWM (0–255).
 * Both pins are PWM-capable on ATtiny402 via TCA0.
 */
inline void setGreen(uint8_t brightness) { analogWrite(PIN_GREEN, brightness); }
inline void setWhite(uint8_t brightness) { analogWrite(PIN_WHITE, brightness); }
inline void setAll(uint8_t brightness)   { setGreen(brightness); setWhite(brightness); }
inline void allOff()                     { setAll(0); }

/**
 * Non-blocking delay helper using millis().
 * Returns true when the requested duration has elapsed.
 * Usage: static uint32_t t = 0; if (wait(t, 500)) { t = millis(); doThing(); }
 */
inline bool wait(uint32_t since, uint32_t duration) {
  return (millis() - since) >= duration;
}

/**
 * Smooth sine-wave PWM value for breathing effects.
 * @param phase  0–255 representing full cycle position
 * @param lo     minimum brightness
 * @param hi     maximum brightness
 */
uint8_t sineWave(uint8_t phase, uint8_t lo = 0, uint8_t hi = PWM_MAX) {
  // sin() is expensive on AVR — use a fast integer approximation instead
  // Maps phase 0–255 to a triangle wave, then softens edges
  uint8_t tri = (phase < 128) ? (phase * 2) : (255 - (phase - 128) * 2);
  // Scale to [lo, hi]
  return lo + ((uint16_t)(hi - lo) * tri) / 255;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Pattern 0 — Slow Pulse
 * Both LEDs breathe in and out together slowly.
 * Period: ~3 seconds per full cycle.
 */
void patternSlowPulse(bool firstRun) {
  static uint32_t lastStep = 0;
  static uint8_t  phase    = 0;

  if (firstRun) { phase = 0; lastStep = millis(); }

  // Advance phase every 12ms → full 256-step cycle ≈ 3 seconds
  if (wait(lastStep, 12)) {
    lastStep = millis();
    phase++;
    uint8_t brightness = sineWave(phase, 4, PWM_MAX);
    setAll(brightness);
  }
}

/**
 * Pattern 1 — Alternate Flash
 * Green and white take turns flashing.
 * Each LED on for 400ms, off while other is on.
 */
void patternAlternate(bool firstRun) {
  static uint32_t lastSwitch = 0;
  static bool     greenOn    = true;

  if (firstRun) { greenOn = true; lastSwitch = millis(); allOff(); }

  if (wait(lastSwitch, 400)) {
    lastSwitch = millis();
    greenOn = !greenOn;
    setGreen(greenOn ? PWM_MAX : 0);
    setWhite(greenOn ? 0 : PWM_MAX);
  }
}

/**
 * Pattern 2 — Chase
 * Sequential: green on → white on → both on → both off → repeat.
 * Each step holds for 300ms.
 */
void patternChase(bool firstRun) {
  static uint32_t lastStep = 0;
  static uint8_t  step     = 0;

  if (firstRun) { step = 0; lastStep = millis(); allOff(); }

  if (wait(lastStep, 300)) {
    lastStep = millis();
    step = (step + 1) % 4;
    switch (step) {
      case 0: setGreen(PWM_MAX); setWhite(0);        break; // green only
      case 1: setGreen(0);       setWhite(PWM_MAX);  break; // white only
      case 2: setAll(PWM_MAX);                        break; // both on
      case 3: allOff();                               break; // both off
    }
  }
}

/**
 * Pattern 3 — Heartbeat
 * Double-flash both LEDs, then a long pause.
 * Mimics a heartbeat rhythm: thump-thump ... pause.
 */
void patternHeartbeat(bool firstRun) {
  static uint32_t lastStep = 0;
  static uint8_t  step     = 0;

  // Step durations in milliseconds
  // 0: first flash on, 1: off, 2: second flash on, 3: off (long pause)
  static const uint16_t stepDurations[] = { 80, 100, 80, 900 };

  if (firstRun) { step = 0; lastStep = millis(); allOff(); }

  if (wait(lastStep, stepDurations[step])) {
    lastStep = millis();
    step = (step + 1) % 4;
    // Steps 0 and 2 are the flashes, steps 1 and 3 are off periods
    (step == 0 || step == 2) ? setAll(PWM_MAX) : allOff();
  }
}

/**
 * Pattern 4 — Slow Drift
 * Green and white breathe out of phase with each other,
 * creating a gentle alternating glow without hard transitions.
 * Phase offset: 128 (half cycle = ~1.5s apart).
 */
void patternSlowDrift(bool firstRun) {
  static uint32_t lastStep   = 0;
  static uint8_t  phaseGreen = 0;

  if (firstRun) { phaseGreen = 0; lastStep = millis(); }

  if (wait(lastStep, 12)) {
    lastStep = millis();
    phaseGreen++;
    uint8_t phaseWhite = phaseGreen + 128; // 180° out of phase
    setGreen(sineWave(phaseGreen, 4, PWM_MAX));
    setWhite(sineWave(phaseWhite, 4, PWM_MAX));
  }
}

/**
 * Pattern 5 — SOS
 * Blinks both LEDs in the SOS Morse code pattern (··· — — — ···).
 * A bit of fun for a Miku concert emergency.
 */
void patternSOS(bool firstRun) {
  // Morse timing: dot=200ms, dash=600ms, symbol gap=200ms, letter gap=600ms
  // S = · · ·   O = — — —   S = · · ·
  static const uint16_t sequence[] = {
    200, 200,   // S: dot, gap
    200, 200,   // S: dot, gap
    200, 600,   // S: dot, letter gap
    600, 200,   // O: dash, gap
    600, 200,   // O: dash, gap
    600, 600,   // O: dash, letter gap
    200, 200,   // S: dot, gap
    200, 200,   // S: dot, gap
    200, 2000,  // S: dot, long pause before repeat
  };
  static const uint8_t SEQ_LEN = sizeof(sequence) / sizeof(sequence[0]);

  static uint32_t lastStep = 0;
  static uint8_t  step     = 0;

  if (firstRun) { step = 0; lastStep = millis(); allOff(); }

  if (wait(lastStep, sequence[step])) {
    lastStep = millis();
    step = (step + 1) % SEQ_LEN;
    // Even steps are ON (symbol), odd steps are OFF (gap)
    (step % 2 == 0) ? setAll(PWM_MAX) : allOff();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
// PATTERN REGISTRY
// Add new patterns here. NUM_PATTERNS is calculated automatically.
// ═══════════════════════════════════════════════════════════════════════════════

static const Pattern PATTERNS[] = {
  { patternSlowPulse,  "Slow Pulse"  },
  { patternAlternate,  "Alternate"   },
  { patternChase,      "Chase"       },
  { patternHeartbeat,  "Heartbeat"   },
  { patternSlowDrift,  "Slow Drift"  },
  { patternSOS,        "SOS"         },
};

static const uint8_t NUM_PATTERNS = sizeof(PATTERNS) / sizeof(PATTERNS[0]);

// ═══════════════════════════════════════════════════════════════════════════════
// STATE
// ═══════════════════════════════════════════════════════════════════════════════

static uint8_t  currentPattern = 0;
static bool     patternFirstRun = true;

// ═══════════════════════════════════════════════════════════════════════════════
// BUTTON HANDLING
// ═══════════════════════════════════════════════════════════════════════════════

void handleButton() {
  // Check for button press (active LOW)
  if (digitalRead(PIN_BUTTON) != LOW) return;

  // Debounce
  delay(DEBOUNCE_MS);
  if (digitalRead(PIN_BUTTON) != LOW) return;

  // Advance to next pattern
  currentPattern = (currentPattern + 1) % NUM_PATTERNS;

  // Persist to EEPROM (update only writes if value changed — saves write cycles)
  EEPROM.update(EEPROM_PATTERN_ADDR, currentPattern);

  // Signal pattern change: brief blackout
  allOff();
  delay(150);

  // Mark that the new pattern needs initialisation on first call
  patternFirstRun = true;

  // Wait for button release to avoid double-triggering
  while (digitalRead(PIN_BUTTON) == LOW) { /* wait */ }
  delay(DEBOUNCE_MS);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETUP & LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  pinMode(PIN_GREEN,  OUTPUT);
  pinMode(PIN_WHITE,  OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  allOff();

  // Restore last used pattern from EEPROM
  uint8_t saved = EEPROM.read(EEPROM_PATTERN_ADDR);
  if (saved < NUM_PATTERNS) {
    currentPattern = saved;
  } else {
    // EEPROM uninitialised (0xFF) — default to pattern 0
    currentPattern = 0;
    EEPROM.write(EEPROM_PATTERN_ADDR, 0);
  }

  patternFirstRun = true;
}

void loop() {
  handleButton();

  // Run the current pattern, passing firstRun flag
  PATTERNS[currentPattern].fn(patternFirstRun);
  patternFirstRun = false;
}
