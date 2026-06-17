#!/bin/bash
set -e

PROJ_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$PROJ_ROOT/gui-native/MedDriveManager/src"
RES="$PROJ_ROOT/gui-native/MedDriveManager/res"
OUT="$PROJ_ROOT/installer/win10-11/x64/Debug"
MINGW="x86_64-w64-mingw32"
MINGW_INC="/usr/x86_64-w64-mingw32/sys-root/mingw/include"
DEFS="-DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DUNICODE -D_UNICODE"
INC="-I$RES -I$SRC -I$MINGW_INC"
TMP=$(mktemp -d)

echo "=== Compilando fontes C ==="
$MINGW-gcc $DEFS $INC -c "$SRC/main.c"          -o "$TMP/main.o"
$MINGW-gcc $DEFS $INC -c "$SRC/mainwnd.c"        -o "$TMP/mainwnd.o"
$MINGW-gcc $DEFS $INC -c "$SRC/dlg_add.c"        -o "$TMP/dlg_add.o"
$MINGW-gcc $DEFS $INC -c "$SRC/dlg_progress.c"   -o "$TMP/dlg_progress.o"
$MINGW-gcc $DEFS $INC -c "$SRC/store.c"          -o "$TMP/store.o"

echo "=== Compilando recursos ==="
$MINGW-windres -I"$RES" "$RES/app.rc" -o "$TMP/app_res.o"

echo "=== Linkando MedDriveManager.exe ==="
mkdir -p "$OUT"
$MINGW-gcc -mwindows \
    "$TMP/main.o" "$TMP/mainwnd.o" "$TMP/dlg_add.o" \
    "$TMP/dlg_progress.o" "$TMP/store.o" "$TMP/app_res.o" \
    -lcomctl32 -lcomdlg32 -lshell32 -lole32 \
    -o "$OUT/MedDriveManager.exe"

echo "  -> $OUT/MedDriveManager.exe"

echo "=== Compilando instalador NSIS ==="
cd "$PROJ_ROOT/installer/win10-11"
makensis setup.nsi

echo ""
echo "=== Build concluido ==="
echo "  Aplicativo : $OUT/MedDriveManager.exe"
echo "  Instalador : $PROJ_ROOT/MeddrivePrinter-Setup.exe"

rm -rf "$TMP"
