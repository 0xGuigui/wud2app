# wud2app
convert wudump folders and .wud images into cert, tik, tmd and app files

## Build

### Linux (gcc/clang)
- `make` (uses the provided `Makefile`), or  
- `gcc -std=c11 -Wall -Wextra -O3 main.c wudparts.c rijndael.c sha1.c -o wud2app`

### Windows
- MinGW/MSYS2 : `gcc -std=c11 -Wall -Wextra -O3 -static main.c wudparts.c rijndael.c sha1.c -o wud2app.exe`  
- MSVC : `cl /std:c11 /O2 /W4 main.c wudparts.c rijndael.c sha1.c /Fe:wud2app.exe`

The codebase now handles platform differences (64-bit file I/O, case-insensitive comparisons, mkdir, packed structs) via `platform.h`.
