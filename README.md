# Celio-Firmware

## TL;DR

This firmware is required to build a **GB-Link Device**.

- The latest firmware revision can be found under **Releases**.
- Currently, only the **Raspberry Pi RP2040** is supported.

Web flash/update
https://launcher.gblink.io

Manual update
1. Download `gblink.uf2` from the latest **[Release](../../releases)**.
2. Hold the **BOOTSEL** button on the RP2040.
3. Connect the device to your computer.
4. Drag and drop the `.uf2` file onto the mounted drive.

To flash the RP2040:

1. Hold the **BOOTSEL** button.
2. Connect the device to your computer.
3. Drag and drop the firmware `.uf2` file onto the mounted device.

| Mode | Command | Description |
|:---|:---|:---|
| **GBA Trade Emu** | `0x00` | Emulate a GBA link partner for Pokémon Gen 3 trades |
| **GBA Link** | `0x01` | Bridge two GBA systems over the internet |
| **GB Link** | `0x02` | SPI passthrough for Game Boy |
| **GB Printer** | `0x03` | Game Boy Printer emulation (bit-bang SPI slave) |
| **GBA Advance Wars** | `0x04` | Advance Wars, Sill WIP |

---

## Game Boy Advance Connectivity

Existing open source PCB designs

Pico PCB: https://github.com/agtbaskara/game-boy-pico-link-board

Pico Zero PCB: https://github.com/weimanc/game-boy-zero-link-board

### Wiring Guide

- smashstacking's GBA to USB Adapter  
  https://www.youtube.com/watch?v=KtHu693wE9o  

You have several hardware options:

- Order a PCB from JLCPCB (or another PCB manufacturer):  
  https://github.com/agtbaskara/game-boy-pico-link-board

- Buy a prebuilt board from Etsy (not affiliated):  
  https://www.etsy.com/de/listing/1517956485/gb-link-usb-zu-gameboy-link-adapter-fur

---

## Link Cable Requirements

This firmware has only been tested with **original Nintendo GBA Link Cables**.  
Reproduction cables should work as well.

⚠ Important: GBA link cable connectors are **not identical**.

- Slim connector = **Master**
- Wide connector = **Slave**

The Celio-Device **must** be connected to the **master connector**.

---

# Overview

This repository contains the firmware source code for the Celio-Device.

The firmware allows the device to connect to a Game Boy Advance as either:

- **Master** (Link Cable pinout default)
- **Slave** (by pulling SO up)

This makes it possible to implement the Pokémon Generation 3 Link Trading Protocol    
and perform trades or battles using real hardware.

For online connectivity, one system assumes the role of the master while the partner system acts as the slave.    
During synchronization, both sides keep the in-game trade session in an idle state until    
packets from the remote partner arrive. The firmware forwards and injects link data in real time,    
effectively allowing two physically separate Game Boy Advance systems to behave as if they were    
connected by a direct link cable.

In this setup, a Celio-Device act as a “dummy” Game Boy Advance. It injects packets into the ongoing trade while   
simultaneously capturing packets from its connected console and forwarding them to the remote partner.    
This packet-level bridging enables stable online trades and battles on original hardware.

---

# Design

The project is primarily written in **C++**.

While it avoids many advanced language features, it heavily relies on:

- **RAII patterns** for safe state management
- Scoped objects to ensure proper cleanup of link session components

This makes it easier to manage complex state transitions during link sessions
without leaving components in undefined states.

---

## Zephyr RTOS

This project uses **Zephyr RTOS**:

https://www.zephyrproject.org/

Originally, the target MCU was an STM32F07.  
Thanks to Zephyr, migrating to the RP2040 required only minimal to moderate effort.

Zephyr provides most low-level abstractions.  
The only MCU-specific implementation resides in: ```/src/layers/linkLayer_.c```   
This file implements the low-level bit-layer handling of the link protocol.

To port this project to another MCU:

| Range | Module | Commands |
|:---|:---|:---|
| `0x00–0x0F` | Control | `0x00` SetMode, `0x01` Cancel |
| `0x10–0x1F` | GBA Link | SetModeMaster, SetModeSlave, StartHandshake, ConnectLink |
| `0x20–0x2F` | GBA Emu | *(internal section commands)* |
| `0x30–0x3F` | GB Link | `0x30` SetTimingConfig, `0x31` EnterPrinter, `0x32` ExitPrinter |
| `0x40–0x4F` | Hardware | `0x40` Voltage3V3, `0x41` Voltage5V, `0x42` SetLEDColor, `0x43` RebootBootloader, `0x44` SetWebUsbLanding, `0x45` GetLedConfig, `0x46` SetModeLedColor, `0x47` ResetLedColors |
| `0x44` | SetWebUsbLanding | (`data[1]`: 1 = on, 0 = off) persists whether the adapter advertises the **launcher.gblink.io** WebUSB landing page (the browser "open site" prompt on connect). It's stored in flash and applies on the next reconnect. The current state is reported as a 5th byte in the `0x0F` GetFirmwareInfo response. |

| `0x45` | GetLedConfig | returns the persisted per-mode LED colours: `[0x45, count, r,g,b …]` (slots: 0 idle, 1 GBA/Celio, 2 GB/GBC, 3 printer, 4 Advance Wars). |
| `0x46` | SetModeLedColor | (`[0x46, slot, r, g, b]`) persists a mode's colour (applied on next entry to that mode |
|`0x42` | SetLEDColor | sets the LED live for previewing). |
| `0x47` | ResetLedColors | restores all slots to the built-in defaults. Colours are full 0–255 RGB — on a WS2812 the magnitude is also the brightness. |

---

# Known Issues

In rare cases, the firmware may not respond to a mode-switch command.

If this happens, reconnect the device.

- When refreshing the web page, the USB device may need to be reset (unplug or press reset button).
- On Linux, run `./scripts/setup-permissions.sh` once to install udev rules so the browser can access the adapter without sudo (covers both WebUSB and WebSerial).
- In rare cases, the firmware may not respond to a mode-switch command — reboot the device.
- If you are forking this repo: To avoid damaging the board do not pull both GP11 and GP12 low at the same time
