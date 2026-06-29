#pragma once
#include <windows.h>

#define WC_MAINWND              L"MedDriveMainWnd"
#define WM_APP_PROFILES_CHANGED (WM_APP + 1)

BOOL mainwnd_register(HINSTANCE hInst);
HWND mainwnd_create(HINSTANCE hInst);
