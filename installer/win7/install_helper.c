/*
 * install_helper.c — instalador Win7 para Meddrive Printer
 * Compilado com MinGW: x86_64-w64-mingw32-gcc
 * Uso: install_helper.exe "<pasta_do_instalador>"
 */

#define WINVER       0x0601
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <winspool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdarg.h>

static FILE *g_log = NULL;

/* ── Logging ─────────────────────────────────────────────────────────── */

static void w2log(const wchar_t *w)
{
    char buf[2048];
    WideCharToMultiByte(CP_UTF8, 0, w, -1, buf, sizeof(buf), NULL, NULL);
    printf("%s\n", buf);
    fflush(stdout);
    if (g_log) { fprintf(g_log, "%s\n", buf); fflush(g_log); }
}

static void log_msg(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    fflush(stdout);
    if (g_log) { fprintf(g_log, "%s\n", buf); fflush(g_log); }
}

static void log_msgw(const wchar_t *fmt, ...)
{
    wchar_t wbuf[2048];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf(wbuf, 2048, fmt, ap);
    va_end(ap);
    w2log(wbuf);
}

/* ── Servicos ────────────────────────────────────────────────────────── */

static BOOL svc_wait(SC_HANDLE h, DWORD state, DWORD ms)
{
    SERVICE_STATUS ss;
    DWORD elapsed = 0;
    while (elapsed < ms) {
        if (!QueryServiceStatus(h, &ss)) return FALSE;
        if (ss.dwCurrentState == state)  return TRUE;
        Sleep(500);
        elapsed += 500;
    }
    return FALSE;
}

static BOOL svc_stop(const wchar_t *name)
{
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return FALSE;
    SC_HANDLE hSvc = OpenServiceW(hSCM, name, SERVICE_ALL_ACCESS);
    BOOL ok = FALSE;
    if (hSvc) {
        SERVICE_STATUS ss;
        ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
        ok = svc_wait(hSvc, SERVICE_STOPPED, 30000);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
    return ok;
}

static BOOL svc_start(const wchar_t *name)
{
    SC_HANDLE hSCM = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCM) return FALSE;
    SC_HANDLE hSvc = OpenServiceW(hSCM, name, SERVICE_ALL_ACCESS);
    BOOL ok = FALSE;
    if (hSvc) {
        StartServiceW(hSvc, 0, NULL);
        ok = svc_wait(hSvc, SERVICE_RUNNING, 30000);
        CloseServiceHandle(hSvc);
    }
    CloseServiceHandle(hSCM);
    return ok;
}

static BOOL svc_restart(const wchar_t *name)
{
    svc_stop(name);
    return svc_start(name);
}

/* ── Arquivos ────────────────────────────────────────────────────────── */

static BOOL copy_file_w(const wchar_t *src, const wchar_t *dst)
{
    log_msgw(L"  %s -> %s", src, dst);
    if (!CopyFileW(src, dst, FALSE)) {
        log_msg("  ERRO: CopyFile falhou (Win32 %lu)", GetLastError());
        return FALSE;
    }
    return TRUE;
}

static void ensure_dir(const wchar_t *path)
{
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
        CreateDirectoryW(path, NULL);
}

/* Busca recursiva por filename em dir; retorna caminho completo em out */
static BOOL find_recursive(const wchar_t *dir, const wchar_t *filename,
                            wchar_t *out, int outLen)
{
    wchar_t pattern[MAX_PATH];
    _snwprintf(pattern, MAX_PATH, L"%s\\*", dir);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    BOOL found = FALSE;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
            continue;

        wchar_t full[MAX_PATH];
        _snwprintf(full, MAX_PATH, L"%s\\%s", dir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (find_recursive(full, filename, out, outLen)) { found = TRUE; break; }
        } else if (!_wcsicmp(fd.cFileName, filename)) {
            _snwprintf(out, outLen, L"%s", full);
            found = TRUE;
            break;
        }
    } while (FindNextFileW(h, &fd));

    FindClose(h);
    return found;
}

/* ── Registry ────────────────────────────────────────────────────────── */

static BOOL reg_set_str(const wchar_t *path, const wchar_t *name,
                         const wchar_t *val)
{
    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0,
                         KEY_SET_VALUE, NULL, &hk, NULL) != ERROR_SUCCESS)
        return FALSE;
    DWORD sz = (DWORD)((wcslen(val) + 1) * sizeof(wchar_t));
    BOOL ok = RegSetValueExW(hk, name, 0, REG_SZ, (const BYTE *)val, sz) == ERROR_SUCCESS;
    RegCloseKey(hk);
    return ok;
}

static BOOL reg_set_dword(const wchar_t *path, const wchar_t *name, DWORD val)
{
    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0,
                         KEY_SET_VALUE, NULL, &hk, NULL) != ERROR_SUCCESS)
        return FALSE;
    BOOL ok = RegSetValueExW(hk, name, 0, REG_DWORD,
                              (const BYTE *)&val, sizeof(DWORD)) == ERROR_SUCCESS;
    RegCloseKey(hk);
    return ok;
}

static BOOL reg_set_multisz(const wchar_t *path, const wchar_t *name,
                              const wchar_t *val, DWORD chars)
{
    HKEY hk;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0,
                         KEY_SET_VALUE, NULL, &hk, NULL) != ERROR_SUCCESS)
        return FALSE;
    BOOL ok = RegSetValueExW(hk, name, 0, REG_MULTI_SZ,
                              (const BYTE *)val, chars * sizeof(wchar_t)) == ERROR_SUCCESS;
    RegCloseKey(hk);
    return ok;
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(void)
{
    g_log = fopen("C:\\Windows\\Temp\\meddrive_install.log", "w");

    /* argc/argv via API para suporte a Unicode */
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    log_msg("[INICIO] Meddrive Printer -- instalador Win7");

    if (argc < 2) {
        log_msg("[ERRO] Uso: install_helper.exe <pasta_instalador>");
        if (g_log) fclose(g_log);
        LocalFree(argv);
        return 1;
    }

    wchar_t instDir[MAX_PATH];
    _snwprintf(instDir, MAX_PATH, L"%s", argv[1]);
    LocalFree(argv);

    /* caminhos do sistema */
    wchar_t sysRoot[MAX_PATH], progData[MAX_PATH];
    GetEnvironmentVariableW(L"SystemRoot", sysRoot, MAX_PATH);
    GetEnvironmentVariableW(L"ProgramData", progData, MAX_PATH);

    wchar_t dllSrc[MAX_PATH], dllDst[MAX_PATH];
    _snwprintf(dllSrc, MAX_PATH, L"%s\\..\\meddrivemon.dll", instDir);
    _snwprintf(dllDst, MAX_PATH, L"%s\\System32\\meddrivemon.dll", sysRoot);

    wchar_t ppdSrc[MAX_PATH], ppdDst[MAX_PATH];
    _snwprintf(ppdSrc, MAX_PATH, L"%s\\MEDDRIVE.PPD", instDir);
    _snwprintf(ppdDst, MAX_PATH, L"%s\\System32\\spool\\drivers\\x64\\3\\MEDDRIVE.PPD", sysRoot);

    wchar_t appDir[MAX_PATH];
    _snwprintf(appDir, MAX_PATH, L"%s\\Meddrive Printer", progData);

    wchar_t driverStoreRoot[MAX_PATH];
    _snwprintf(driverStoreRoot, MAX_PATH,
               L"%s\\System32\\DriverStore\\FileRepository", sysRoot);

    const wchar_t *monitorName = L"Meddrive Printer MONITOR";
    const wchar_t *driverName  = L"Meddrive Printer DRIVER";

    wchar_t monitorReg[MAX_PATH];
    _snwprintf(monitorReg, MAX_PATH,
               L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\%s", monitorName);

    wchar_t driverKey[MAX_PATH];
    _snwprintf(driverKey, MAX_PATH,
               L"SYSTEM\\CurrentControlSet\\Control\\Print\\Environments"
               L"\\Windows x64\\Drivers\\Version-3\\%s", driverName);

    /* ── 1. Para o Spooler ────────────────────────────────────────── */
    log_msg("[1] Parando o Spooler...");
    svc_stop(L"Spooler");
    log_msg("  OK - Spooler parado");

    /* ── 2. Copia DLL ─────────────────────────────────────────────── */
    log_msg("[2] Copiando meddrivemon.dll para System32...");
    if (!copy_file_w(dllSrc, dllDst)) { if (g_log) fclose(g_log); return 1; }
    log_msg("  OK");

    /* ── 3. Registra monitor ──────────────────────────────────────── */
    log_msg("[3] Registrando monitor no registry...");
    if (!reg_set_str(monitorReg, L"Driver", L"meddrivemon.dll")) {
        log_msg("  ERRO: falha ao registrar monitor (Win32 %lu)", GetLastError());
        if (g_log) fclose(g_log); return 1;
    }
    log_msg("  OK");

    /* ── 4. Inicia Spooler ────────────────────────────────────────── */
    log_msg("[4] Iniciando o Spooler...");
    if (!svc_start(L"Spooler")) {
        log_msg("  ERRO: Spooler nao iniciou");
        if (g_log) fclose(g_log); return 1;
    }
    Sleep(5000);
    log_msg("  OK - Spooler em execucao");

    /* ── 5. Localiza PSCRIPT5.DLL no DriverStore ─────────────────── */
    log_msg("[5] Localizando PSCRIPT5.DLL no DriverStore...");
    wchar_t pscript5[MAX_PATH] = {0};
    if (!find_recursive(driverStoreRoot, L"PSCRIPT5.DLL", pscript5, MAX_PATH)) {
        log_msgw(L"  ERRO: PSCRIPT5.DLL nao encontrado em %s", driverStoreRoot);
        if (g_log) fclose(g_log); return 1;
    }
    wchar_t driverDir[MAX_PATH];
    _snwprintf(driverDir, MAX_PATH, L"%s", pscript5);
    wchar_t *lastSlash = wcsrchr(driverDir, L'\\');
    if (lastSlash) *lastSlash = L'\0';
    log_msgw(L"  DriverStore: %s", driverDir);

    /* ── 6. Instala driver PSCRIPT5 ──────────────────────────────── */
    log_msg("[6] Instalando driver PSCRIPT5 via AddPrinterDriverExW...");

    wchar_t drvPath[MAX_PATH], dataFile[MAX_PATH], cfgFile[MAX_PATH];
    _snwprintf(drvPath,  MAX_PATH, L"%s\\PSCRIPT5.DLL", driverDir);
    _snwprintf(dataFile, MAX_PATH, L"%s\\PSCRIPT.NTF",  driverDir);
    _snwprintf(cfgFile,  MAX_PATH, L"%s\\PS5UI.DLL",    driverDir);

    DRIVER_INFO_2W di2;
    memset(&di2, 0, sizeof(di2));
    di2.cVersion     = 3;
    di2.pName        = (LPWSTR)driverName;
    di2.pEnvironment = L"Windows x64";
    di2.pDriverPath  = drvPath;
    di2.pDataFile    = dataFile;
    di2.pConfigFile  = cfgFile;

    /* APD_COPY_ALL_FILES | APD_COPY_FROM_DIRECTORY = 0x04 | 0x10 = 20 */
    if (!AddPrinterDriverExW(NULL, 2, (LPBYTE)&di2, 0x14)) {
        log_msg("  ERRO: AddPrinterDriverExW falhou (Win32 %lu)", GetLastError());
        if (g_log) fclose(g_log); return 1;
    }
    reg_set_dword(driverKey, L"PrinterDriverAttributes", 2);
    log_msgw(L"  OK - driver '%s' instalado", driverName);

    /* ── 7. PPD ───────────────────────────────────────────────────── */
    log_msg("[7] Instalando PPD...");
    if (!copy_file_w(ppdSrc, ppdDst)) { if (g_log) fclose(g_log); return 1; }

    /* MULTI_SZ: "MEDDRIVE.PPD\0\0" */
    wchar_t depFiles[] = L"MEDDRIVE.PPD\0";
    reg_set_multisz(driverKey, L"Dependent Files",
                    depFiles, sizeof(depFiles) / sizeof(wchar_t));
    log_msg("  OK - PPD copiado e registrado em Dependent Files");

    /* ── 8. Reinicia Spooler e valida driver ─────────────────────── */
    log_msg("[8] Reiniciando o Spooler...");
    if (!svc_restart(L"Spooler")) {
        log_msg("  ERRO: Spooler nao reiniciou");
        if (g_log) fclose(g_log); return 1;
    }
    Sleep(3000);

    DWORD needed = 0, returned = 0;
    EnumPrinterDriversW(NULL, L"Windows x64", 1, NULL, 0, &needed, &returned);
    BYTE *buf = (BYTE *)malloc(needed);
    BOOL driverFound = FALSE;
    if (buf && EnumPrinterDriversW(NULL, L"Windows x64", 1, buf, needed,
                                    &needed, &returned)) {
        DRIVER_INFO_1W *di1 = (DRIVER_INFO_1W *)buf;
        for (DWORD i = 0; i < returned; i++) {
            if (!_wcsicmp(di1[i].pName, driverName)) { driverFound = TRUE; break; }
        }
    }
    free(buf);

    if (!driverFound) {
        log_msgw(L"  ERRO: driver '%s' nao reconhecido pelo spooler", driverName);
        if (g_log) fclose(g_log); return 1;
    }
    log_msgw(L"  OK - driver '%s' reconhecido", driverName);

    /* ── 9. Copia arquivos para ProgramData ──────────────────────── */
    log_msgw(L"[9] Copiando arquivos para %s...", appDir);
    ensure_dir(appDir);

    wchar_t confDst[MAX_PATH];
    _snwprintf(confDst, MAX_PATH, L"%s\\conf", appDir);
    ensure_dir(confDst);

    const wchar_t *confFiles[] = {
        L"add-printer.ps1",    L"create-profile.ps1", L"edit-profile.ps1",
        L"edit-printer.ps1",   L"remove-printer.ps1", L"remove-profile.ps1", NULL
    };
    for (int i = 0; confFiles[i]; i++) {
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        _snwprintf(src, MAX_PATH, L"%s\\conf\\%s", instDir,  confFiles[i]);
        _snwprintf(dst, MAX_PATH, L"%s\\%s",       confDst,  confFiles[i]);
        if (!copy_file_w(src, dst)) { if (g_log) fclose(g_log); return 1; }
    }

    const wchar_t *rootFiles[] = { L"MEDDRIVE.PPD", L"MedDriveManager.exe", NULL };
    for (int i = 0; rootFiles[i]; i++) {
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        _snwprintf(src, MAX_PATH, L"%s\\%s", instDir, rootFiles[i]);
        _snwprintf(dst, MAX_PATH, L"%s\\%s", appDir,  rootFiles[i]);
        if (!copy_file_w(src, dst)) { if (g_log) fclose(g_log); return 1; }
    }

    log_msg("");
    log_msgw(L"[OK] Instalacao concluida!");
    log_msgw(L"  Monitor    : %s", monitorName);
    log_msgw(L"  Driver     : %s", driverName);
    log_msgw(L"  Aplicativo : %s\\MedDriveManager.exe", appDir);

    if (g_log) fclose(g_log);
    return 0;
}
