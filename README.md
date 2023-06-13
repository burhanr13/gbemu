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

### Todo:
- other mappers and RTC
- configuration
- save states
- cycle accuracy
- serial communication
- super game boy palettes

## Compilation
This project uses SDL2 as a dependency and that library is needed to compile and link the executable. Use `make` or `make debug` to compile with debug symbols or use `make release` to compile the whole application with optimization.

## How to use
Run the executable with the ROM file path as the only command line argument. You can use the keyboard or connect a game controller prior to running the emulator.

Keyboard Controls:
- A : Z
- B : X
- Start : Return
- Select : Backspace
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
