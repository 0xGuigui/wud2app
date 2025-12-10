# wud2app
convert wudump folders and .wud images into cert, tik, tmd and app files

## What it does
- Extracts `title.cert`, `title.tik`, `title.tmd` and all `.app`/`.h3` files from a Wii U WUD image (single file or `wudump` split parts).
- Output folder name comes from the 10-byte game ID in the WUD header.

## Inputs
- `common.key` : **16-byte raw AES key (binary)**. If you have a 32-char hex string, convert it first, e.g. `echo <hex> | xxd -r -p > common.key`.
- `game.key`   : Same format, 16-byte raw disc key for the game (convert from hex the same way).
- WUD source:
  - Single file: `game.wud`
  - Split parts: `game_part1.wud` … `game_part12.wud` in the same folder (as produced by `wudump`).

## Usage
- Single WUD file: `wud2app common.key game.key game.wud`
- Wudump folder  : `wud2app "/full/path/to/folder"`
  - Folder must contain `common.key`, `game.key`, and `game_part1.wud` … `game_part12.wud`.

## Build

### Linux (gcc/clang)
- `make` (uses the provided `Makefile`), or  
- `gcc -std=c11 -Wall -Wextra -O3 main.c wudparts.c rijndael.c sha1.c -o wud2app`

### Windows
- MinGW/MSYS2 : `gcc -std=c11 -Wall -Wextra -O3 -static main.c wudparts.c rijndael.c sha1.c -o wud2app.exe`  
- MSVC : `cl /std:c11 /O2 /W4 main.c wudparts.c rijndael.c sha1.c /Fe:wud2app.exe`

The codebase now handles platform differences (64-bit file I/O, case-insensitive comparisons, mkdir, packed structs) via `platform.h`.
