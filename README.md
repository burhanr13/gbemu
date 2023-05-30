# gbemu

An emulator for the Game Boy, a handheld game console from the 1990s. Currently many games are playable with full graphics and audio support, however, it is not cycle-accurate.

## Task List/Features

### Completed:
- CPU
- graphics
- saving
- MBC1,3,5
- audio

### Todo:
- other mappers and RTC
- timing/accuracy
- game boy color support
- configuration and emulator controls
- save states
- fast forward

## Compilation
This project uses SDL2 as a dependency and that library is needed to compile and link the executable. The Makefile can be used to compile the project.

## How to use
Run the executable with the ROM file path as the only command line argument. Controls are currently fixed but they will be configurable in the future. DPad is the arrow keys, 'A' is Z, 'B' is X, 'Start' is return, 'Select' is backspace.