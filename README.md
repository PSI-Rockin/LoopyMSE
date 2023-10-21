# LoopyMSE
A Casio Loopy emulator. WIP, but able to run some games.

## How to use
LoopyMSE must be launched from the command line with these arguments: [game ROM] [BIOS]

The emulator will automatically load .sav files with the same name as the game ROM. If no .sav file exists, the emulator will create one. Specifying the save file to use in the command line may be added at a future date.

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
kasami - dumping the BIOS, HW testing, and many other valuable non-code contributions  
UBCH server - translations and moral support
