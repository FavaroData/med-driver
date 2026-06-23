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

echo "=== Compilando DLL do monitor ==="
$MINGW-gcc -shared \
    -DUNICODE -D_UNICODE -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 -DNTDDI_VERSION=0x0A000000 \
    -I"$MINGW_INC" \
    "$PROJ_ROOT/src/monitor.c" \
    "$PROJ_ROOT/src/monitor.def" \
    -lwinspool -ladvapi32 -lshell32 \
    -o "$PROJ_ROOT/meddrivemon.dll"
echo "  -> $PROJ_ROOT/meddrivemon.dll"

echo "=== Compilando fontes C ==="
$MINGW-gcc $DEFS $INC -c "$SRC/main.c"               -o "$TMP/main.o"
$MINGW-gcc $DEFS $INC -c "$SRC/mainwnd.c"             -o "$TMP/mainwnd.o"
$MINGW-gcc $DEFS $INC -c "$SRC/dlg_add.c"             -o "$TMP/dlg_add.o"
$MINGW-gcc $DEFS $INC -c "$SRC/dlg_progress.c"        -o "$TMP/dlg_progress.o"
$MINGW-gcc $DEFS $INC -c "$SRC/dlg_profile.c"         -o "$TMP/dlg_profile.o"
$MINGW-gcc $DEFS $INC -c "$SRC/store.c"               -o "$TMP/store.o"
$MINGW-gcc $DEFS $INC -c "$SRC/ui/theme.c"            -o "$TMP/ui_theme.o"
$MINGW-gcc $DEFS $INC -c "$SRC/ui/titlebar.c"         -o "$TMP/ui_titlebar.o"
$MINGW-gcc $DEFS $INC -c "$SRC/ui/navbar.c"           -o "$TMP/ui_navbar.o"
$MINGW-gcc $DEFS $INC -c "$SRC/ui/listview.c"         -o "$TMP/ui_listview.o"
$MINGW-gcc $DEFS $INC -c "$SRC/ui/statusbar.c"        -o "$TMP/ui_statusbar.o"
$MINGW-gcc $DEFS $INC -c "$SRC/ui/buttons.c"          -o "$TMP/ui_buttons.o"

echo "=== Compilando recursos ==="
$MINGW-windres --codepage 65001 -I"$RES" "$RES/app.rc" -o "$TMP/app_res.o"

echo "=== Linkando MedDriveManager.exe ==="
mkdir -p "$OUT"
$MINGW-gcc -mwindows \
    "$TMP/main.o" "$TMP/mainwnd.o" "$TMP/dlg_add.o" \
    "$TMP/dlg_progress.o" "$TMP/dlg_profile.o" "$TMP/store.o" \
    "$TMP/ui_theme.o" "$TMP/ui_titlebar.o" "$TMP/ui_navbar.o" \
    "$TMP/ui_listview.o" "$TMP/ui_statusbar.o" "$TMP/ui_buttons.o" \
    "$TMP/app_res.o" \
    -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lwinspool -ldwmapi \
    -o "$OUT/MedDriveManager.exe"

echo "  -> $OUT/MedDriveManager.exe"

echo "=== Compilando instalador NSIS (Win10/11) ==="
cd "$PROJ_ROOT/installer/win10-11"
makensis setup.nsi

echo "=== Compilando instalador NSIS (Win7) ==="
cd "$PROJ_ROOT/installer/win7"
makensis setup.nsi

echo ""
echo "=== Build concluido ==="
echo "  Aplicativo    : $OUT/MedDriveManager.exe"
echo "  Instalador    : $PROJ_ROOT/MeddrivePrinter-Setup.exe"
echo "  Instalador W7 : $PROJ_ROOT/MeddrivePrinter-Win7-Setup.exe"

rm -rf "$TMP"
