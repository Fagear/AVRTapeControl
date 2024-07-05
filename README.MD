# AVRTapeControl

## What is this?

**AVRTapeControl** is firmware for AVR MCUs that facilitates control over simple single-motor tape mechanisms.
Desired mode is set through buttons (one per mode), MCU operates a solenoid that switches states of the mechanism. Optonal on/off control is available for a motor that drives capstan, takeup and cam gear (for power saving).

## Supported mechanisms
*(photos of mechanisms are provided in [/mech_photo](mech_photo) folder)*
- **"CRP42602Y"** mechanism (that's what it's called on AliExpress, LG uses similar transports), also known as **M02753900D**.
- **Tanashin TN-21ZLG/CSG clone** mechanism (used in new TEAC and TASCAM decks), also known as **M60207052**.

## Features

- Minimalistic and cheap circuitry for basic mechanism operation
- Options for expanding functionality with more components
- Full logic control and auto-stop (for supported transports)
- Support for reverse playback (including auto-reverse options)
- Support for tape recording
- Mechanism sensor monitoring and fault detection
- Low power consumption and power save features

## Usage

Sorry, no electrical schematics at the moment...

MCU can be clocked from internal 8 MHz RC-oscillator but **external 8 MHz Xtal** is recommended for precise timing. Internally clock is divided by 8, so everything runs at **1 MHz**.

<details>
<summary>AVR fuses for ATmega328 with 8 MHz Xtal</summary>

- **CKDIV8** = 0
- **SUT1** = 0
- **CKSEL3** = 0
- **SPIEN** = 0
- **BODLEVEL1** = 0
- **BODLEVEL0** = 0
- all other at "1"

In hex form:
- low byte: **0x57**
- high byte: **0xDF**
- extended byte: **0xFC**
</details>

<details>
<summary>AVR fuses for ATmega328 with internal RC generator</summary>

- **CKDIV8** = 0
- **SUT1** = 0
- **SUT0** = 0
- **CKSEL3** = 0
- **CKSEL2** = 0
- **CKSEL0** = 0
- **SPIEN** = 0
- **BODLEVEL1** = 0
- **BODLEVEL0** = 0
- all other at "1"

In hex form:
- low byte: **0x42**
- high byte: **0xDF**
- extended byte: **0xFC**
</details>

<details>
<summary>AVR fuses for ATmega48/ATmega88/ATmega168 with 8 MHz Xtal</summary>

- **CKDIV8** = 0
- **SUT1** = 0
- **CKSEL3** = 0
- **SPIEN** = 0
- **BODLEVEL1** = 0
- **BODLEVEL0** = 0
- all other at "1"

In hex form:
- low byte: **0x57**
- high byte: **0xDC**
- extended byte: **0xFF**
</details>

<details>
<summary>AVR fuses for ATmega48/ATmega88/ATmega168 with internal RC generator</summary>

- **CKDIV8** = 0
- **SUT1** = 0
- **SUT0** = 0
- **CKSEL3** = 0
- **CKSEL2** = 0
- **CKSEL0** = 0
- **SPIEN** = 0
- **BODLEVEL1** = 0
- **BODLEVEL0** = 0
- all other at "1"

In hex form:
- low byte: **0x42**
- high byte: **0xDC**
- extended byte: **0xFF**
</details>

Due to low clock MCU can run on voltages from **1.8 V** to **5.0 V**.
If **74HC595** is used for IO expansion, minimum supply voltage is **2.0 V**.

Most tape transports require **12 V** to function and fail at voltage **< 8 V** and usually MCU will get power from that +12 V supply through an LDO, so there is no reason to make MCU operational below **4 V** and thus fuses are set to minimum operating voltage of **4.3 V**.

<details>
<summary>Pinout for ATmega 48/88/168/328 MCU in DIP packages</summary>

Power supply:
- **pin 7** *(VCC)*: +5 V supply
- **pin 20** *(AVCC)*: +5 V supply (BOD)
- **pin 8** *(GND)*: 0 V (common)
- **pin 22** *(GND)*: 0 V (common)

Clock input:
- **pin 9** *(PB6)*: 8.0 MHz Xtal
- **pin 10** *(PB7)*: 8.0 MHz Xtal

User input:
- **pin 23** *(PC0)*: (input) ***fast forward*** command *("0" active, pullup enabled)*
- **pin 24** *(PC1)*: (input) ***play*** command *("0" active, pullup enabled)*
- **pin 25** *(PC2)*: (input) ***record*** command *("0" active, pullup enabled)*
- **pin 26** *(PC3)*: (input) ***stop*** command *("0" active, pullup enabled)*
- **pin 27** *(PC4)*: (input) ***reverse play*** command *("0" active, pullup enabled)*
- **pin 28** *(PC5)*: (input) ***rewind*** command *("0" active, pullup enabled)*

Mechanism sensors:
- **pin 4** *(PD2)*: (input) takeup ***tachometer*** *(pullup enabled)*
- **pin 5** *(PD3)*: (input) home/***stop position*** sensor *("1" active, pullup enabled)*
- **pin 6** *(PD4)*: (input) ***tape presence*** sensor *("0" active, pullup enabled)*
- **pin 11** *(PD5)*: (input) forward ***record inhibit*** sensor *("1" active, pullup enabled)*
- **pin 12** *(PD6)*: (input) reverse ***record inhibit*** sensor *("1" active, pullup enabled)* **(for CRP42602Y transport)**
- **pin 12** *(PD6)*: (output) +5 V power to tachometer sensor **(for Tanashin-clone)**

Mechanism controls:
- **pin 14** *(PB0)*: (output) ***solenoid*** drive *("1" for energizing)*
- **pin 15** *(PB1)*: (output) ***capstan motor*** drive *("1" for spinning)*

Other (optional) controls:
- **pin 2** *(PD0)*: (output) playback ***mute*** *("1" for head amplifier to mute sound when not in playback)*
- **pin 13** *(PD7)*: (output) ***record*** enable *("1" for enabling erase generator and switching amplifier to record mode)*

For extended functions:
- **pin 16** *(PB2)*: (output) SPI latch *(to pin 12 of **74HC595**)*
- **pin 17** *(PB3)*: (output) SPI data *(to pin 14 of **74HC595**)*
- **pin 19** *(PB5)*: (output) SPI clock *(to pin 11 of **74HC595**)*
- **pin 3** *(PD1)*: (output) TTL UART TX ***for debug*** @125000 8-N-1 (if enabled by [UART_TERM] define, not recommended for actual use)

</details>

Buttons that are not necessary can be not connected. If some mechanism sensors are absent or needed to be bypassed - corresponding pins should be left not connected or shorted to ground (see active signal notes).

> [!NOTE]
> "*Capstan motor drive*" is optional and can be left disconnected.

> [!NOTE]
> "*Playback mute*" and "*record enable*" outputs are also optional.

> [!NOTE]
> **74HC595** extender is used for mode indication and can be not installed.

<details>
<summary>Pinout for 74HC595 extender</summary>

- **pin 15** *(bit 0)*: **fault**
- **pin 1** *(bit 1)*: **tape presence** (can be used for tape compartment illumination)
- **pin 2** *(bit 2)*: **stop**
- **pin 3** *(bit 3)*: **recording**
- **pin 4** *(bit 4)*: **rewind**
- **pin 5** *(bit 5)*: **playback in reverse** (or **playback direction**)
- **pin 6** *(bit 6)*: **playback in forward** (or **playback**)
- **pin 7** *(bit 7)*: **fast forward**

</details>

> [!IMPORTANT]
> "*Solenoid drive*" and "*capstan motor drive*" should be connected to some transistors that will switch current to solenoid and motor respectively. Usually those are powered from a separate **+12 V** supply.

## How does it work

Firmware senses state of the sensors on the tape transport and looks for user command. All control is performed by two main outputs:

- Motor enable (optional, used only for power-saving)
- Solenoid drive

Single-motor transports usually have one solenoid to perform all mode changes. Firmware provides series of precisely-timed impulses to the solenoid to select desired mode.

### Auto-detecting number of "PLAY" buttons

At the startup firmware performs a short test, searching for a short between "*Play in forward*" and "*Play in reverse*" inputs. If short is detected then control of the playback will be performed using only one ("*Play in forward*") button. If there is no short, control scheme will require two playback buttons.

> [!NOTE]
> This feature applies only for transports with reverse playback capabilities.

When in "*single play button*" control scheme, pressing "*Play*" button when not in playback mode will engage playback in the last performed direction. If "*Play*" button is pressed during playback, tape direction will reverse and playback will start in an opposite direction. So, single button controls both starting the playback and its direction. "*Playback*" indicator will activate for playback in any direction, "*playback direction*" will active when playing in reverse.

When in "*dual play button*" control scheme, pressing "*Play*" will engage playback in forward direction. Pressing "*Play in reverse*" will engage playback in reverse direction. "*Playback in forward*" indicator will activate for playback in forward direction. "*Playback in reverse*" indicator will activate for playback in reverse direction.

### Self-test mode

If "*Stop*" button is held at the powerup, firmware will perform 10 second self-test.

"*Tape presence*" indicator will activate if there is a tape inserted.

"*Recording*" indicator will activate if record inhibit switch on any side senses "recording is allowed" condition.

"*Fault*" indicator will stay active.

Other indicators will switch state **once a second**:

- "*Stop*" is active
- "*Playback in forward*" and "*Playback in reverse*" are active
- "*Fast forward*" and "*Rewind*" are active

This mode allows to test tape mechanism switches, correct MCU clock (by blink speed), function of indicators.

After 10 second elapsed self-test mode is disabled and firmware resumes normal operation.

## Demo

Release firmware for **CSG clone of Tanashin TN-21ZLG**/**M60207052** transport:

[![CSG clone of Tanashin TN-21ZLG with AVRTapeControl](https://img.youtube.com/vi/yGgGG5cwDQc/default.jpg)](https://www.youtube.com/watch?v=yGgGG5cwDQc)

Release firmware for **CRP42602Y**/**M02753900D** transport:

[![CRP42602Y with AVRTapeControl](https://img.youtube.com/vi/U36fvBHO8jw/default.jpg)](https://www.youtube.com/watch?v=U36fvBHO8jw)

Old video of reverse-engineering **CRP42602Y** transport:

[![CRP42602Y reverse-engineering](https://img.youtube.com/vi/5YsUvNuCFKs/default.jpg)](https://www.youtube.com/watch?v=5YsUvNuCFKs)

## Current state

- User-input processing is working fine
- Auto-detection of number of playback buttons
- Self-test mode for verifying MCU clock, indicators and tape transport switches
- Mode indicators are working fine
- State machine for CRP42602Y is working fine (some bugs in edge cases may still be present)
- State machine for Tanashin-clone is working fine
- Power saving procedures are working fine

## Plans
- Add support for more transports
- Add feature toggling through command buttons in STOP mode (no pins for additional switches)
- Enable and use EEPROM driver for saving feature configuration and run counters
- Add more IO on SPI for more switches and indicators
- Add SPI-slave operations to support higher-level control from another MCU

## License
Program copyright 2021-2024.

This program is free software.
Licensed under the Apache License, Version 2.0 (the "License");