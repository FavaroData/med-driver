#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "settings.h"

#define INI_DIR      MEDDRIVE_DATA_DIR
#define INI_FILE     MEDDRIVE_DATA_DIR L"\\settings.ini"
#define INI_SECT     L"Geral"
#define INI_SECT_GS  L"Ghostscript"
#define GS_DEFAULT   MEDDRIVE_DATA_DIR L"\\Ghostscript\\bin\\" MEDDRIVE_GS_EXE
#define PORTS_KEY    L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\Meddrive Printer MONITOR\\Ports"

static void get_ini_path(wchar_t *out, int len) {
    ExpandEnvironmentStringsW(INI_FILE, out, len);
}

#ifdef MEDDRIVE_XP
#define RUN_KEY_PATH L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_KEY_VAL  L"MeddrivePrinterAgent"

/* No XP o agente sobe pela Run key (o install_helper_xp.c a cria).
   Ligar = gravar o valor; desligar = remover. */
static BOOL xp_set_autostart(BOOL enable) {
    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, RUN_KEY_PATH, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &hk, NULL) != ERROR_SUCCESS)
        return FALSE;
    LONG r;
    if (enable) {
        wchar_t agent[MAX_PATH];
        ExpandEnvironmentStringsW(
            MEDDRIVE_DATA_DIR L"\\MeddrivePrinterAgent.exe", agent, MAX_PATH);
        r = RegSetValueExW(hk, RUN_KEY_VAL, 0, REG_SZ, (BYTE *)agent,
                           (DWORD)((wcslen(agent) + 1) * sizeof(wchar_t)));
    } else {
        r = RegDeleteValueW(hk, RUN_KEY_VAL);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;  /* ja desligado */
    }
    RegCloseKey(hk);
    return (r == ERROR_SUCCESS);
}
#else
#define TASK_NAME  L"MeddrivePrinterAgent"
#define TASK_XML   L"%SystemRoot%\\System32\\Tasks\\MeddrivePrinterAgent"

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
#endif

void settings_load(AppSettings *out) {
    out->agentAutoStart      = FALSE;
    out->requireAgentRunning = FALSE;

    wchar_t ini[MAX_PATH];
    get_ini_path(ini, MAX_PATH);
    out->requireAgentRunning =
        (GetPrivateProfileIntW(INI_SECT, L"RequireAgentRunning", 0, ini) != 0);
    out->bloquearAplicacao =
        (GetPrivateProfileIntW(L"Segurança", L"bloquearAplicacao", 0, ini) != 0);

    ExpandEnvironmentStringsW(GS_DEFAULT, out->gsPath, MAX_PATH);
    GetPrivateProfileStringW(INI_SECT_GS, L"ExecutablePath",
                             out->gsPath, out->gsPath, MAX_PATH, ini);

#ifdef MEDDRIVE_XP
    /* XP: presenca do valor na Run key = auto-start ligado */
    HKEY hk;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, RUN_KEY_PATH, 0, KEY_QUERY_VALUE, &hk) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hk, RUN_KEY_VAL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            out->agentAutoStart = TRUE;
        RegCloseKey(hk);
    }
#else
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
#endif
}

BOOL settings_save(const AppSettings *s) {
#ifdef MEDDRIVE_XP
    BOOL ok = xp_set_autostart(s->agentAutoStart);
#else
    BOOL ok = run_schtasks(s->agentAutoStart ? L"/ENABLE" : L"/DISABLE");
#endif

    wchar_t dir[MAX_PATH], ini[MAX_PATH];
    ExpandEnvironmentStringsW(INI_DIR, dir, MAX_PATH);
    CreateDirectoryW(dir, NULL);
    get_ini_path(ini, MAX_PATH);
    WritePrivateProfileStringW(INI_SECT, L"RequireAgentRunning",
                               s->requireAgentRunning ? L"1" : L"0", ini);
    WritePrivateProfileStringW(INI_SECT_GS, L"ExecutablePath", s->gsPath, ini);
    WritePrivateProfileStringW(L"Segurança", L"bloquearAplicacao",
                               s->bloquearAplicacao ? L"1" : L"0", ini);

    /* atualiza GhostscriptPath no registry de todos os perfis existentes */
    HKEY hPorts;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, PORTS_KEY, 0,
                      KEY_READ | KEY_ENUMERATE_SUB_KEYS, &hPorts) == ERROR_SUCCESS) {
        DWORD i = 0;
        wchar_t portName[256];
        DWORD portNameLen = 256;
        while (RegEnumKeyExW(hPorts, i++, portName, &portNameLen,
                             NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            HKEY hPort;
            if (RegOpenKeyExW(hPorts, portName, 0, KEY_SET_VALUE, &hPort) == ERROR_SUCCESS) {
                RegSetValueExW(hPort, L"GhostscriptPath", 0, REG_SZ,
                               (BYTE *)s->gsPath,
                               (DWORD)((wcslen(s->gsPath) + 1) * sizeof(wchar_t)));
                RegCloseKey(hPort);
            }
            portNameLen = 256;
        }
        RegCloseKey(hPorts);
    }

    return ok;
}
