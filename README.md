# 🌿 Leek PCB Keychain

A small-form-factor PCB keychain shaped like Hatsune Miku's iconic leek, designed as concert swag for Magical Mirai. Features green LEDs at the tip, white LEDs at the base, and multiple user-selectable lighting patterns driven by an ATtiny402.

---

## Features

- Leek-shaped PCB outline with keychain mounting hole
- 4x green LEDs (leek tip) + 2x white LEDs (base)
- 4 lighting patterns selectable via tactile button
- SPDT slide switch for hard power on/off
- CR1220 coin cell powered (~16 hours active runtime)
- Fully assembled by JLCPCB (SMT + THT)

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ATtiny402 (SOT-23-8) | LCSC C440337 |
| Green LEDs × 4 | 0402 SMD green | LCSC C2297 — verify Vf ≤ 2.3V |
| White LEDs × 2 | 0402 SMD white | LCSC C2290 — verify Vf ≤ 2.8V |
| Power switch | SPDT slide SMD | LCSC C431540 |
| Pattern button | Tactile 3×4mm SMD | LCSC C455280 |
| Battery holder | CR1220 THT | LCSC C70377 |
| Bypass cap | 100nF 0402 | LCSC C307331 |

See the full design document for complete BOM, resistor values, and wiring details.

## Firmware

Written in Arduino IDE using [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore).

**Board manager URL:**
```
http://drazzy.com/package_drazzy.com_index.json
```

**Board settings:**
- Board: `ATtiny402`
- Clock: Internal 32kHz
- Programmer: SerialUPDI or Microchip SNAP

## Programming

The ATtiny402 is programmed via UPDI using 3 pads on the PCB edge (GND, VCC, UPDI). The cheapest programmer is a CH340 USB-serial adapter with a 1kΩ resistor:

```
USB-Serial TX ── 1kΩ ── UPDI pad
USB-Serial RX ──────── UPDI pad
GND ────────────────── GND pad
```

## LED Patterns

| # | Name | Description |
|---|------|-------------|
| 1 | Slow Pulse | Both LEDs breathe in and out together |
| 2 | Alternate | Green and white flash in turns |
| 3 | Chase | Green → white → both → off, repeat |
| 4 | Heartbeat | Double-flash then long pause |

Press the pattern button to cycle. The selected pattern is saved to EEPROM and restored on next power-on.

## License

MIT — see [LICENSE](LICENSE)

---

*A [Diva Engineering](https://github.com/diva-eng) project.*
