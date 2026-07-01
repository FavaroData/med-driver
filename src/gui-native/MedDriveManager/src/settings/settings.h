#pragma once
#include <windows.h>

/* Base de dados por SO: no XP %ProgramData% nao existe -> usa %ALLUSERSPROFILE%\Application Data.
   No XP x86 o Ghostscript e gswin32c.exe (nao gswin64c.exe). */
#ifdef MEDDRIVE_XP
#define MEDDRIVE_DATA_DIR L"%ALLUSERSPROFILE%\\Application Data\\Meddrive Printer"
#define MEDDRIVE_GS_EXE   L"gswin32c.exe"
#else
#define MEDDRIVE_DATA_DIR L"%ProgramData%\\Meddrive Printer"
#define MEDDRIVE_GS_EXE   L"gswin64c.exe"
#endif

typedef struct {
    BOOL    agentAutoStart;
    BOOL    requireAgentRunning;
    wchar_t gsPath[MAX_PATH];
    BOOL    bloquearAplicacao;
} AppSettings;

void settings_load(AppSettings *out);
BOOL settings_save(const AppSettings *s);
