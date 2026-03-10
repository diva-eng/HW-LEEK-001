# Leek PCB Keychain

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

| Comment | Designator | Footprint | LCSC | Quantity |
|---------|------------|-----------|------|----------|
| ATtiny402-SS | U1 | SOIC-8_3.9x4.9mm_P1.27mm | C2053235 | 1 |
| Battery_Cell | BT1 | BAT-SMD_MY-1220-03 | C964818 | 1 |
| C | C1 | C_0402_1005Metric | C1525 | 1 |
| LED | D6,D7 | LED_0603_1608Metric | C189306 | 2 |
| LED | D4,D5 | LED_0603_1608Metric | C5246833 | 2 |
| LED | D1,D2,D3 | LED_0805_2012Metric | C5355441 | 3 |
| R | R6,R7 | R_0402_1005Metric | C25076 | 2 |
| R | R1,R2,R3,R4,R5 | R_0402_1005Metric | C25104 | 5 |
| ~ | SW3 | SW-TH_K3-1296S-E1 | C128955 | 1 |
| ~ | SW4 | SW-SMD_4P-L5.1-W5.1-P3.70-LS6.5-TL_H1.5 | C318884 | 1 |
| Conn_01x03_Pin | J1 | JST_EH_S3B-EH_1x03_P2.50mm_Horizontal |  | 1 |

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
