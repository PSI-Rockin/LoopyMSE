# LoopyMSE
A Casio Loopy emulator. WIP, but able to run some games.
Includes functional but slightly buggy/incomplete sound emulation.

## How to use
LoopyMSE must be launched from the command line with these arguments: `<game ROM> <BIOS> [sound BIOS]`

The emulator will automatically load .sav files with the same name as the game ROM. If no .sav file exists, the emulator will create one. Specifying the save file to use in the command line may be added at a future date.

The sound BIOS file is optional, and the emulator will run silently if it is not provided. The file may be incorrectly labelled as "Printer BIOS" in older ROM sets.

NOTE: all files must be in big-endian format.

## Controls
Only hardcoded keyboard keys for the time being:

| Loopy | Keyboard |
| ----- | -------- |
| A | Z |
| B | X |
| C | A |
| D | S |
| L1 | Q |
| R1 | W |
| Up | Up |
| Down | Down |
| Left | Left |
| Right | Right |
| Start | Enter/Return |

## Special Thanks
kasami - sound implementation, dumping the BIOS, HW testing, and many other valuable non-code contributions  
UBCH server - translations and moral support
