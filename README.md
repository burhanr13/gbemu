# gbemu

An emulator for the Game Boy, a handheld game console from the 1990s which uses a CPU similar to the 8080 and Z80. At the current stage some popular games are playable such as "Tetris", "Dr. Mario",  "Pokemon Red and Blue", and "The Legend of Zelda: Link's Awakening".

## Task List/Features

### Completed:
- CPU
- MBC1,3,5
- Background rendering
- window rendering
- object rendering

### In Progress:
- bug fixes

### Todo:
- battery saves with memory mapped files
- sound
- other mappers and RTC
- timing/accuracy
- game boy color support
- configuration and emulator controls

## Compilation
This project uses SDL2 as a dependency and that library is needed to compile and link the executable. The Makefile can be used to compile the project.

## How to use
Run the executable with the ROM file path as the only command line argument. Controls are currently fixed but they will be configurable in the future. DPad is the arrow keys, 'A' is Z, 'B' is X, 'Start' is return, 'Select' is backspace.