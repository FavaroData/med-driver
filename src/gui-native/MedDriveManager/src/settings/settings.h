#pragma once
#include <windows.h>

typedef struct {
    BOOL agentAutoStart;
    BOOL requireAgentRunning;
} AppSettings;

void settings_load(AppSettings *out);
BOOL settings_save(const AppSettings *s);
