#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "settings.h"

#define TASK_NAME  L"MeddrivePrinterAgent"
#define TASK_XML   L"%SystemRoot%\\System32\\Tasks\\MeddrivePrinterAgent"
#define INI_DIR    L"%ProgramData%\\Meddrive Printer"
#define INI_FILE   L"%ProgramData%\\Meddrive Printer\\settings.ini"
#define INI_SECT   L"Geral"

static void get_ini_path(wchar_t *out, int len) {
    ExpandEnvironmentStringsW(INI_FILE, out, len);
}

static BOOL run_schtasks(const wchar_t *flag) {
    wchar_t cmd[256];
    _snwprintf_s(cmd, 256, _TRUNCATE,
                 L"schtasks /Change /TN \"%s\" %s", TASK_NAME, flag);

    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return FALSE;

    WaitForSingleObject(pi.hProcess, 10000);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (code == 0);
}

void settings_load(AppSettings *out) {
    out->agentAutoStart      = FALSE;
    out->requireAgentRunning = FALSE;

    wchar_t ini[MAX_PATH];
    get_ini_path(ini, MAX_PATH);
    out->requireAgentRunning =
        (GetPrivateProfileIntW(INI_SECT, L"RequireAgentRunning", 0, ini) != 0);

    wchar_t path[MAX_PATH];
    ExpandEnvironmentStringsW(TASK_XML, path, MAX_PATH);

    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE)
        return;  /* tarefa não registrada */

    DWORD fileSize = GetFileSize(hf, NULL);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0 || fileSize > 65536) {
        CloseHandle(hf);
        out->agentAutoStart = TRUE;
        return;
    }

    char *buf = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize + 4);
    if (!buf) { CloseHandle(hf); out->agentAutoStart = TRUE; return; }

    DWORD n = 0;
    ReadFile(hf, buf, fileSize, &n, NULL);
    CloseHandle(hf);

    /* tarefa existe: habilitada por padrão, salvo se o XML diz o contrário */
    BOOL disabled = FALSE;
    if (n >= 2 && (unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) {
        /* UTF-16 LE (com BOM) */
        DWORD wlen = (n - 2) / 2;
        wchar_t *wbuf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                              (wlen + 1) * sizeof(wchar_t));
        if (wbuf) {
            memcpy(wbuf, buf + 2, wlen * sizeof(wchar_t));
            wbuf[wlen] = L'\0';
            disabled = (wcsstr(wbuf, L"<Enabled>false</Enabled>") != NULL);
            HeapFree(GetProcessHeap(), 0, wbuf);
        }
    } else {
        buf[n] = '\0';
        disabled = (strstr(buf, "<Enabled>false</Enabled>") != NULL);
    }

    HeapFree(GetProcessHeap(), 0, buf);
    out->agentAutoStart = !disabled;
}

BOOL settings_save(const AppSettings *s) {
    BOOL ok = run_schtasks(s->agentAutoStart ? L"/ENABLE" : L"/DISABLE");

    wchar_t dir[MAX_PATH], ini[MAX_PATH];
    ExpandEnvironmentStringsW(INI_DIR, dir, MAX_PATH);
    CreateDirectoryW(dir, NULL);
    get_ini_path(ini, MAX_PATH);
    WritePrivateProfileStringW(INI_SECT, L"RequireAgentRunning",
                               s->requireAgentRunning ? L"1" : L"0", ini);
    return ok;
}
