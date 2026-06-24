#pragma once
#include <windows.h>

#define WC_MAINWND L"MedDriveMainWnd"

BOOL mainwnd_register(HINSTANCE hInst);
HWND mainwnd_create(HINSTANCE hInst);
