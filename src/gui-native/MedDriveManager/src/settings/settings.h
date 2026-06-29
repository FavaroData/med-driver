#pragma once
#include <windows.h>

typedef struct {
    BOOL    agentAutoStart;
    BOOL    requireAgentRunning;
    wchar_t gsPath[MAX_PATH];
} AppSettings;

void settings_load(AppSettings *out);
BOOL settings_save(const AppSettings *s);
