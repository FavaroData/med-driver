#!/bin/bash
set -e

PROJ_ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$PROJ_ROOT/src/gui-native/MedDriveManager/src"
RES="$PROJ_ROOT/src/gui-native/MedDriveManager/res"
OUT="$PROJ_ROOT/installer/winxp"
MINGW="i686-w64-mingw32"
MINGW_LIB="/usr/$MINGW/sys-root/mingw/lib"
MINGW_INC="/usr/$MINGW/sys-root/mingw/include"
DEFS="-DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DUNICODE -D_UNICODE -DMEDDRIVE_XP"
XPSEC="-include $PROJ_ROOT/src/xp_secure.h"
INC="-I$RES -I$SRC -I$MINGW_INC"
TMP=$(mktemp -d)

echo "=== [XP] meddrivemon_xp.dll (i686) ==="
$MINGW-gcc -shared \
    -DUNICODE -D_UNICODE -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 \
    $XPSEC \
    -I"$MINGW_INC" \
    "$PROJ_ROOT/src/monitor.c" \
    "$PROJ_ROOT/src/monitor.def" \
    -L"$MINGW_LIB" -lwinspool -ladvapi32 -lwtsapi32 \
    -static-libgcc \
    -o "$OUT/meddrivemon_xp.dll"
echo "  -> $OUT/meddrivemon_xp.dll"

echo "=== [XP] MeddrivePrinterAgent_xp.exe (i686) ==="
$MINGW-gcc -mwindows \
    -DUNICODE -D_UNICODE -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 \
    $XPSEC \
    -L"$MINGW_LIB" \
    "$PROJ_ROOT/src/agent/MeddrivePrinterAgent.c" \
    -ladvapi32 -lshell32 -lcomdlg32 -lole32 \
    -o "$OUT/MeddrivePrinterAgent_xp.exe"
echo "  -> $OUT/MeddrivePrinterAgent_xp.exe"

echo "=== [XP] MedDriveManager.exe (i686) ==="
for f in main mainwnd store \
          dialogs/dlg_add dialogs/dlg_progress dialogs/dlg_profile dialogs/dlg_password \
          ui/theme ui/titlebar ui/navbar ui/listview ui/statusbar ui/buttons \
          settings/settings settings/settings_tab settings/import_config \
          profiles/profiles_tab printers/printers_tab printers/native/remove-printer; do
    $MINGW-gcc $DEFS $XPSEC $INC -c "$SRC/$f.c" -o "$TMP/$(basename $f).o"
done
$MINGW-windres --codepage 65001 -I"$RES" "$RES/app.rc" -o "$TMP/app_res.o"
$MINGW-gcc -mwindows \
    "$TMP"/*.o \
    -L"$MINGW_LIB" \
    -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lwinspool -ladvapi32 \
    -o "$OUT/MedDriveManager.exe"
echo "  -> $OUT/MedDriveManager.exe"

echo "=== [XP] Instalador NSIS ==="
cd "$PROJ_ROOT/installer/winxp"
makensis setup.nsi

echo ""
echo "=== Build XP concluido ==="
echo "  meddrivemon XP     : $OUT/meddrivemon_xp.dll"
echo "  Agente XP          : $OUT/MeddrivePrinterAgent_xp.exe"
echo "  MedDriveManager XP : $OUT/MedDriveManager.exe"
echo "  Instalador XP      : $PROJ_ROOT/MeddrivePrinter-WinXP-Setup.exe"

rm -rf "$TMP"
