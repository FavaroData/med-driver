/*
 * install_helper_xp.c — instalador WinXP x86 para Meddrive Printer
 * Compilar: i686-w64-mingw32-gcc install_helper_xp.c -o install_helper_xp.exe -lwinspool -lAdvapi32
 * Uso: install_helper_xp.exe "<pasta_do_instalador>"
 *
 * Diferencas em relacao ao Win7:
 *  - WINVER 0x0501; driver env "Windows NT x86"; spool em w32x86\3\
 *  - PSCRIPT5.DLL no caminho fixo (DriverStore nao existe no XP)
 *  - %ALLUSERSPROFILE%\Application Data no lugar de %ProgramData%
 *  - Agente registrado via HKLM Run key (Schedule.Service COM e Vista+)
 *  - MedDriveManager.exe e opcional nesta versao (log warning se ausente)
 */

#define WINVER       0x0501
#define _WIN32_WINNT 0x0501

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

/* ── Registry ────────────────────────────────────────────────────────── */

static BOOL reg_set_str(HKEY root, const wchar_t *path, const wchar_t *name,
                         const wchar_t *val)
{
    HKEY hk;
    if (RegCreateKeyExW(root, path, 0, NULL, 0,
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

    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    log_msg("[INICIO] Meddrive Printer -- instalador WinXP x86");

    if (argc < 2) {
        log_msg("[ERRO] Uso: install_helper_xp.exe <pasta_instalador>");
        if (g_log) fclose(g_log);
        LocalFree(argv);
        return 1;
    }

    wchar_t instDir[MAX_PATH];
    _snwprintf(instDir, MAX_PATH, L"%s", argv[1]);
    LocalFree(argv);

    /* caminhos do sistema */
    wchar_t sysRoot[MAX_PATH], allUsers[MAX_PATH];
    GetEnvironmentVariableW(L"SystemRoot",      sysRoot,  MAX_PATH);
    GetEnvironmentVariableW(L"ALLUSERSPROFILE", allUsers, MAX_PATH);

    /* %ALLUSERSPROFILE%\Application Data == ProgramData no XP */
    wchar_t progData[MAX_PATH];
    _snwprintf(progData, MAX_PATH, L"%s\\Application Data", allUsers);

    wchar_t dllSrc[MAX_PATH], dllDst[MAX_PATH];
    _snwprintf(dllSrc, MAX_PATH, L"%s\\..\\meddrivemon.dll", instDir);
    _snwprintf(dllDst, MAX_PATH, L"%s\\System32\\meddrivemon.dll", sysRoot);

    /* XP x86: drivers ficam em w32x86\3\ (nao x64\3\) */
    wchar_t ppdSrc[MAX_PATH], ppdDst[MAX_PATH];
    _snwprintf(ppdSrc, MAX_PATH, L"%s\\MEDDRIVE.PPD", instDir);
    _snwprintf(ppdDst, MAX_PATH,
               L"%s\\System32\\spool\\drivers\\w32x86\\3\\MEDDRIVE.PPD", sysRoot);

    wchar_t appDir[MAX_PATH];
    _snwprintf(appDir, MAX_PATH, L"%s\\Meddrive Printer", progData);

    const wchar_t *monitorName = L"Meddrive Printer MONITOR";
    const wchar_t *driverName  = L"Meddrive Printer DRIVER";
    /* XP x86: environment e "Windows NT x86", nao "Windows x64" */
    const wchar_t *driverEnv   = L"Windows NT x86";

    wchar_t monitorReg[MAX_PATH];
    _snwprintf(monitorReg, MAX_PATH,
               L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\%s", monitorName);

    wchar_t driverKey[MAX_PATH];
    _snwprintf(driverKey, MAX_PATH,
               L"SYSTEM\\CurrentControlSet\\Control\\Print\\Environments"
               L"\\Windows NT x86\\Drivers\\Version-3\\%s", driverName);

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
    if (!reg_set_str(HKEY_LOCAL_MACHINE, monitorReg, L"Driver", L"meddrivemon.dll")) {
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

    /* ── 5. Localiza PSCRIPT5.DLL ────────────────────────────────── */
    /* No XP nao existe DriverStore e num XP limpo os arquivos do PScript5
       nao estao no disco — vem empacotados no instalador (pasta pscript5). */
    log_msg("[5] Localizando PSCRIPT5.DLL empacotado...");
    wchar_t drvSrcDir[MAX_PATH];
    _snwprintf(drvSrcDir, MAX_PATH, L"%s\\pscript5", instDir);
    wchar_t pscript5[MAX_PATH];
    _snwprintf(pscript5, MAX_PATH, L"%s\\PSCRIPT5.DLL", drvSrcDir);

    if (GetFileAttributesW(pscript5) == INVALID_FILE_ATTRIBUTES) {
        log_msgw(L"  ERRO: PSCRIPT5.DLL nao encontrado em %s", pscript5);
        if (g_log) fclose(g_log); return 1;
    }
    log_msgw(L"  Origem PScript5: %s", drvSrcDir);

    /* ── 6. Instala driver PSCRIPT5 ──────────────────────────────── */
    log_msg("[6] Instalando driver PSCRIPT5 via AddPrinterDriverExW...");

    wchar_t drvPath[MAX_PATH], dataFile[MAX_PATH], cfgFile[MAX_PATH];
    _snwprintf(drvPath,  MAX_PATH, L"%s\\PSCRIPT5.DLL", drvSrcDir);
    _snwprintf(dataFile, MAX_PATH, L"%s\\PSCRIPT.NTF",  drvSrcDir);
    _snwprintf(cfgFile,  MAX_PATH, L"%s\\PS5UI.DLL",    drvSrcDir);

    DRIVER_INFO_2W di2;
    memset(&di2, 0, sizeof(di2));
    di2.cVersion     = 3;
    di2.pName        = (LPWSTR)driverName;
    di2.pEnvironment = (LPWSTR)driverEnv;
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
    EnumPrinterDriversW(NULL, (LPWSTR)driverEnv, 1, NULL, 0, &needed, &returned);
    BYTE *buf = (BYTE *)malloc(needed);
    BOOL driverFound = FALSE;
    if (buf && EnumPrinterDriversW(NULL, (LPWSTR)driverEnv, 1, buf, needed,
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

    /* ── 9. Copia arquivos para Application Data ─────────────────── */
    log_msgw(L"[9] Copiando arquivos para %s...", appDir);
    ensure_dir(progData);
    ensure_dir(appDir);

    wchar_t confDst[MAX_PATH];
    _snwprintf(confDst, MAX_PATH, L"%s\\conf", appDir);
    ensure_dir(confDst);

    const wchar_t *confFiles[] = {
        L"add-printer.ps1",    L"create-profile.ps1", L"edit-profile.ps1",
        L"edit-printer.ps1",   L"remove-profile.ps1", NULL
    };
    for (int i = 0; confFiles[i]; i++) {
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        _snwprintf(src, MAX_PATH, L"%s\\conf\\%s", instDir, confFiles[i]);
        _snwprintf(dst, MAX_PATH, L"%s\\%s",       confDst, confFiles[i]);
        if (!copy_file_w(src, dst)) { if (g_log) fclose(g_log); return 1; }
    }

    /* arquivos obrigatorios para o funcionamento da impressora */
    const wchar_t *reqFiles[] = { L"MEDDRIVE.PPD", L"MeddrivePrinterAgent.exe", NULL };
    for (int i = 0; reqFiles[i]; i++) {
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        _snwprintf(src, MAX_PATH, L"%s\\%s", instDir, reqFiles[i]);
        _snwprintf(dst, MAX_PATH, L"%s\\%s", appDir,  reqFiles[i]);
        if (!copy_file_w(src, dst)) { if (g_log) fclose(g_log); return 1; }
    }

    /* MedDriveManager.exe: opcional ate a versao XP x86 ser compilada */
    {
        wchar_t src[MAX_PATH], dst[MAX_PATH];
        _snwprintf(src, MAX_PATH, L"%s\\MedDriveManager.exe", instDir);
        _snwprintf(dst, MAX_PATH, L"%s\\MedDriveManager.exe", appDir);
        if (!copy_file_w(src, dst))
            log_msg("  AVISO: MedDriveManager.exe ausente -- instale a versao XP x86 manualmente");
    }

    /* ── 10. Registra agente no HKLM Run key ────────────────────── */
    /* Schedule.Service COM e Vista+ -- no XP o Run key garante inicio no login */
    log_msg("[10] Registrando agente no Run key...");
    wchar_t agentPath[MAX_PATH];
    _snwprintf(agentPath, MAX_PATH, L"%s\\MeddrivePrinterAgent.exe", appDir);

    if (!reg_set_str(HKEY_LOCAL_MACHINE,
                     L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                     L"MeddrivePrinterAgent", agentPath)) {
        log_msg("  AVISO: falha ao registrar Run key (Win32 %lu)", GetLastError());
    } else {
        log_msg("  OK - agente registrado em HKLM\\...\\Run");
    }

    /* inicia imediatamente sem exigir logout/login */
    wchar_t agentCmd[MAX_PATH + 2];
    _snwprintf(agentCmd, MAX_PATH + 2, L"\"%s\"", agentPath);
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    if (CreateProcessW(NULL, agentCmd, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        log_msg("  OK - agente iniciado para sessao atual");
    } else {
        log_msg("  AVISO: falha ao iniciar agente imediatamente (Win32 %lu)", GetLastError());
    }

    log_msg("");
    log_msgw(L"[OK] Instalacao concluida!");
    log_msgw(L"  Monitor    : %s", monitorName);
    log_msgw(L"  Driver     : %s", driverName);
    log_msgw(L"  Ambiente   : %s", driverEnv);
    log_msgw(L"  Aplicativo : %s\\MedDriveManager.exe", appDir);

    if (g_log) fclose(g_log);
    return 0;
}
