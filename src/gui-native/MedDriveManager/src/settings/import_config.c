#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winspool.h>
#include <commdlg.h>
#include <stdio.h>
#include "import_config.h"
#include "settings.h"
#include "../store.h"

// mingw nao declara AddPortExW no winspool.h, mas a funcao existe na libwinspool.a
BOOL WINAPI AddPortExW(LPWSTR, DWORD, LPBYTE, LPWSTR);

static const WCHAR PORTS_KEY[]   =
    L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\"
    L"Meddrive Printer MONITOR\\Ports";
static const WCHAR PORT_PREFIX[] = L"Meddrive Printer PORT ";
static const WCHAR MONITOR[]     = L"Meddrive Printer MONITOR";
static const WCHAR DRIVER[]      = L"Meddrive Printer DRIVER";
static const WCHAR INI_PATH[]    = L"%ProgramData%\\Meddrive Printer\\settings.ini";

// scanner simples com wcsstr funciona porque nos geramos o JSON (schema fixo).
// Se o schema crescer ou aceitar JSON externo, use uma lib de parsing.
static BOOL json_str(const wchar_t *json, const wchar_t *key,
                     wchar_t *out, int outLen) {
    wchar_t search[256];
    _snwprintf(search, 256, L"\"%s\":", key);
    const wchar_t *p = wcsstr(json, search);
    if (!p) return FALSE;
    p += wcslen(search);
    while (*p == L' ') p++;
    if (*p != L'"') return FALSE;
    p++;
    int i = 0;
    while (*p && *p != L'"' && i < outLen - 1) {
        if (*p == L'\\') {
            p++;
            // unescape de \\ e \" porque paths do Windows tem barras invertidas no JSON
            if      (*p == L'"')  { out[i++] = L'"';  p++; }
            else if (*p == L'\\') { out[i++] = L'\\'; p++; }
            else                  { out[i++] = *p++;       }
        } else {
            out[i++] = *p++;
        }
    }
    out[i] = 0;
    return TRUE;
}

static int json_int(const wchar_t *json, const wchar_t *key, int def) {
    wchar_t search[256];
    _snwprintf(search, 256, L"\"%s\":", key);
    const wchar_t *p = wcsstr(json, search);
    if (!p) return def;
    p += wcslen(search);
    while (*p == L' ') p++;
    return (*p >= L'0' && *p <= L'9') ? _wtoi(p) : def;
}

// acha o primeiro } depois de { — falha se um valor de string contiver };
// paths do Windows nao tem esse caractere, entao e seguro pro nosso schema.
static const wchar_t *next_obj(const wchar_t *pos, const wchar_t **end) {
    const wchar_t *p = wcschr(pos, L'{');
    if (!p) return NULL;
    const wchar_t *e = wcschr(p + 1, L'}');
    if (!e) return NULL;
    *end = e + 1;
    return p;
}

static BOOL port_exists(const wchar_t *portName) {
    HKEY hPorts;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, PORTS_KEY, 0,
                      KEY_READ, &hPorts) != ERROR_SUCCESS)
        return FALSE;
    HKEY h;
    BOOL found = (RegOpenKeyExW(hPorts, portName, 0, KEY_READ, &h) == ERROR_SUCCESS);
    if (found) RegCloseKey(h);
    RegCloseKey(hPorts);
    return found;
}

static BOOL printer_exists(const wchar_t *name) {
    DWORD needed = 0, returned = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &returned);
    if (!needed) return FALSE;
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, needed);
    if (!buf) return FALSE;
    BOOL found = FALSE;
    if (EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, buf, needed, &needed, &returned)) {
        PRINTER_INFO_2W *info = (PRINTER_INFO_2W *)buf;
        for (DWORD i = 0; i < returned && !found; i++)
            if (info[i].pPrinterName && _wcsicmp(info[i].pPrinterName, name) == 0)
                found = TRUE;
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return found;
}

static void apply_settings_json(const wchar_t *json) {
    const wchar_t *sec = wcsstr(json, L"\"settings\":");
    if (!sec) return;
    const wchar_t *end;
    const wchar_t *obj = next_obj(sec, &end);
    if (!obj) return;

    int len = (int)(end - obj);
    wchar_t *buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                         (size_t)(len + 1) * sizeof(wchar_t));
    if (!buf) return;
    wcsncpy_s(buf, len + 1, obj, len);
    buf[len] = 0;

    AppSettings s;
    // parte dos valores atuais como base para nao apagar campos que o backup nao trouxer
    settings_load(&s);
    s.agentAutoStart      = (BOOL)json_int(buf, L"agentAutoStart",      s.agentAutoStart);
    s.requireAgentRunning = (BOOL)json_int(buf, L"requireAgentRunning",  s.requireAgentRunning);
    json_str(buf, L"ghostscriptPath", s.gsPath, MAX_PATH);
    settings_save(&s);

    int days = json_int(buf, L"logAutoCleanDays", -1);
    if (days >= 0) {
        wchar_t ini[MAX_PATH], ds[8];
        ExpandEnvironmentStringsW(INI_PATH, ini, MAX_PATH);
        _snwprintf(ds, 8, L"%d", days);
        WritePrivateProfileStringW(L"Logs", L"AutoCleanDays", ds, ini);
    }
    HeapFree(GetProcessHeap(), 0, buf);
}

static int apply_profiles_json(const wchar_t *json, const wchar_t *gsPath) {
    const wchar_t *arr = wcsstr(json, L"\"profiles\":");
    if (!arr) return 0;
    arr = wcschr(arr, L'[');
    if (!arr) return 0;
    const wchar_t *arrEnd = wcschr(arr, L']');
    if (!arrEnd) arrEnd = json + wcslen(json);

    int created = 0;
    const wchar_t *pos = arr;
    while (pos < arrEnd) {
        const wchar_t *end;
        const wchar_t *obj = next_obj(pos, &end);
        if (!obj || obj > arrEnd) break;

        int len = (int)(end - obj);
        wchar_t *buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                             (size_t)(len + 1) * sizeof(wchar_t));
        if (!buf) break;
        wcsncpy_s(buf, len + 1, obj, len);
        buf[len] = 0;

        wchar_t name[PRINTER_NAME_MAX]        = {0};
        wchar_t outputPath[PRINTER_PATH_MAX]   = {0};
        wchar_t baseName[PRINTER_BASENAME_MAX] = {0};
        json_str(buf, L"name",           name,       PRINTER_NAME_MAX);
        json_str(buf, L"outputPath",     outputPath, PRINTER_PATH_MAX);
        json_str(buf, L"outputBaseName", baseName,   PRINTER_BASENAME_MAX);
        DWORD openAfter = (DWORD)json_int(buf, L"openAfterGenerate", 0);
        DWORD overwrite = (DWORD)json_int(buf, L"overwriteFile",     0);
        DWORD choose    = (DWORD)json_int(buf, L"choosePath",        0);
        HeapFree(GetProcessHeap(), 0, buf);

        pos = end;
        if (!name[0]) continue;

        wchar_t portName[PRINTER_PORT_MAX];
        _snwprintf(portName, PRINTER_PORT_MAX, L"%s%s", PORT_PREFIX, name);
        if (port_exists(portName)) continue;

        // escrevemos no registry direto antes do AddPortEx: o monitor le essas chaves
        // quando o Spooler chama sua funcao de inicializacao da porta
        wchar_t keyPath[512];
        _snwprintf(keyPath, 512, L"%s\\%s", PORTS_KEY, portName);
        HKEY hPort;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, keyPath, 0, NULL,
                            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL,
                            &hPort, NULL) != ERROR_SUCCESS)
            continue;

        RegSetValueExW(hPort, L"OutputPath",        0, REG_SZ,
            (BYTE *)outputPath, (DWORD)((wcslen(outputPath) + 1) * sizeof(wchar_t)));
        RegSetValueExW(hPort, L"OutputBaseName",    0, REG_SZ,
            (BYTE *)baseName,   (DWORD)((wcslen(baseName)   + 1) * sizeof(wchar_t)));
        RegSetValueExW(hPort, L"GhostscriptPath",   0, REG_SZ,
            (BYTE *)gsPath,     (DWORD)((wcslen(gsPath)     + 1) * sizeof(wchar_t)));
        RegSetValueExW(hPort, L"OpenAfterGenerate", 0, REG_DWORD,
            (BYTE *)&openAfter, sizeof(DWORD));
        RegSetValueExW(hPort, L"OverwriteFile",     0, REG_DWORD,
            (BYTE *)&overwrite, sizeof(DWORD));
        RegSetValueExW(hPort, L"ChoosePath",        0, REG_DWORD,
            (BYTE *)&choose,    sizeof(DWORD));
        RegCloseKey(hPort);

        // erro ignorado: AddPortEx retorna FALSE se a porta ja esta carregada no Spooler
        PORT_INFO_1W pi1 = {portName};
        AddPortExW(NULL, 1, (LPBYTE)&pi1, (LPWSTR)MONITOR);

        created++;
    }
    return created;
}

static int apply_printers_json(const wchar_t *json) {
    const wchar_t *arr = wcsstr(json, L"\"printers\":");
    if (!arr) return 0;
    arr = wcschr(arr, L'[');
    if (!arr) return 0;
    const wchar_t *arrEnd = wcschr(arr, L']');
    if (!arrEnd) arrEnd = json + wcslen(json);

    int created = 0;
    const wchar_t *pos = arr;
    while (pos < arrEnd) {
        const wchar_t *end;
        const wchar_t *obj = next_obj(pos, &end);
        if (!obj || obj > arrEnd) break;

        int len = (int)(end - obj);
        wchar_t *buf = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                             (size_t)(len + 1) * sizeof(wchar_t));
        if (!buf) break;
        wcsncpy_s(buf, len + 1, obj, len);
        buf[len] = 0;

        wchar_t name[PRINTER_NAME_MAX]     = {0};
        wchar_t portName[PRINTER_PORT_MAX] = {0};
        json_str(buf, L"name",     name,     PRINTER_NAME_MAX);
        json_str(buf, L"portName", portName, PRINTER_PORT_MAX);
        HeapFree(GetProcessHeap(), 0, buf);

        pos = end;
        if (!name[0] || !portName[0]) continue;
        if (printer_exists(name)) continue;

        // AddPrinterW recusa se a porta nao estiver no Spooler — apply_profiles_json tem que rodar antes
        PRINTER_INFO_2W pi2  = {0};
        pi2.pPrinterName     = name;
        pi2.pPortName        = portName;
        pi2.pDriverName      = (LPWSTR)DRIVER;
        pi2.pPrintProcessor  = L"winprint";
        pi2.pDatatype        = L"RAW";
        pi2.Attributes       = PRINTER_ATTRIBUTE_LOCAL;

        HANDLE h = AddPrinterW(NULL, 2, (LPBYTE)&pi2);
        if (h) { ClosePrinter(h); created++; }
    }
    return created;
}

// AddPortEx e AddPrinterW falham silenciosamente se o Spooler nao estiver ativo
static void spooler_ensure_running(void) {
    SC_HANDLE hm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
    if (!hm) return;
    SC_HANDLE hs = OpenServiceW(hm, L"Spooler", SERVICE_START | SERVICE_QUERY_STATUS);
    if (hs) {
        SERVICE_STATUS ss;
        if (QueryServiceStatus(hs, &ss) && ss.dwCurrentState != SERVICE_RUNNING)
            StartServiceW(hs, 0, NULL);
        CloseServiceHandle(hs);
    }
    CloseServiceHandle(hm);
    Sleep(1500);
}

void import_config_run(HWND parent) {
    OPENFILENAMEW ofn = {sizeof(ofn)};
    wchar_t path[MAX_PATH] = {0};
    ofn.hwndOwner   = parent;
    ofn.lpstrFilter = L"JSON\0*.json\0Todos os arquivos\0*.*\0\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        MessageBoxW(parent, L"Não foi possível abrir o arquivo.",
                    L"Importar configuração", MB_ICONERROR | MB_OK);
        return;
    }
    DWORD size = GetFileSize(hf, NULL);
    char *utf8 = (char *)HeapAlloc(GetProcessHeap(), 0, size + 1);
    DWORD rd = 0;
    ReadFile(hf, utf8, size, &rd, NULL);
    CloseHandle(hf);
    utf8[rd] = 0;

    int wLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    wchar_t *json = (wchar_t *)HeapAlloc(GetProcessHeap(), 0,
                                          (size_t)wLen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, json, wLen);
    HeapFree(GetProcessHeap(), 0, utf8);

    // barreira contra backups de versoes futuras com schema diferente
    if (json_int(json, L"version", 0) != 1) {
        MessageBoxW(parent,
            L"Arquivo incompatível (versão não suportada).\r\n"
            L"Use um backup gerado por esta versão do Meddrive Printer.",
            L"Importar configuração", MB_ICONERROR | MB_OK);
        HeapFree(GetProcessHeap(), 0, json);
        return;
    }

    apply_settings_json(json);

    // le o gsPath recem-salvo pelo apply_settings_json para repassar aos perfis
    AppSettings s;
    settings_load(&s);

    spooler_ensure_running();
    int profCreated = apply_profiles_json(json, s.gsPath);
    int prnCreated  = apply_printers_json(json);

    HeapFree(GetProcessHeap(), 0, json);

    wchar_t msg[256];
    _snwprintf(msg, 256,
        L"Importação concluída.\r\n\r\n"
        L"Perfis criados: %d\r\n"
        L"Impressoras criadas: %d\r\n\r\n"
        L"Itens já existentes foram mantidos sem alteração.",
        profCreated, prnCreated);
    MessageBoxW(parent, msg, L"Importar configuração", MB_ICONINFORMATION | MB_OK);
}
