# gbemu

An emulator for the Game Boy and Game Boy Color. Most games should be playable with full color graphics and audio support.

## Task List/Features

### Completed:
- CPU
- graphics
- saving
- MBC1,3,5
- audio
- controller support
- fast forward
- Game Boy Color support
- real time clock for MBC3
- save states with compression by zlib
- cycle accuracy - passes most of the mooneye test suite

### Todo:
- other MBCs
- configuration
- serial communication
- super game boy palettes
- GUI

## Compilation
This project uses SDL2 and zlib as a dependencies. Use `make` or `make debug` to compile with debug symbols or use `make release` to compile the whole application with optimization.

## How to use
Run the executable with the ROM file path as the only command line argument. You can use the keyboard or connect a game controller prior to running the emulator.

Keyboard Controls:
- A : Z
- B : X
- Start : Return
- Select : Right Shift
- Dpad : arrow keys

Gamepad Controls:
- A : A
- B : X
- Start : Start
- Select : Back
- Dpad : Dpad

Hotkeys:
- Reset : R
- Reset and switch between gb/gbc : T
- Pause : P
- Mute : M
- Toggle Fast forward : Tab
- Save State : 9
- Load State : 0
