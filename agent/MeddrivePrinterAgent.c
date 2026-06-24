/*
 * MeddrivePrinterAgent.c — agente de sessão de usuário para conversão PS→PDF
 *
 * Roda na sessão do usuário logado (via Task Scheduler), com credenciais
 * de rede do usuário. Recebe jobs da meddrivemon.dll via named pipe e
 * executa o Ghostscript em nome do usuário.
 *
 * Pipe: \\.\pipe\MeddrivePrinter_<SessionId>
 * Log:  C:\Windows\Temp\meddrive_agent.log
 */

#define WINVER       0x0600
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
#include <stdarg.h>

#define PIPE_PREFIX L"\\\\.\\pipe\\MeddrivePrinter_"
#define LOG_PATH    L"C:\\Windows\\Temp\\meddrive_agent.log"

typedef struct {
    WCHAR psTempPath[MAX_PATH];
    WCHAR outputPath[MAX_PATH];
    WCHAR gsPath[MAX_PATH];
} PrintJobMsg;

typedef struct {
    DWORD exitCode;
    WCHAR errorMsg[512];
} PrintJobResponse;

static void Log(const char *fmt, ...) {
    HANDLE hLog = CreateFileW(LOG_PATH,
        FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hLog == INVALID_HANDLE_VALUE) return;
    char buf[512];
    DWORD w;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    WriteFile(hLog, buf, (DWORD)(len > 0 ? len : 0), &w, NULL);
    CloseHandle(hLog);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hInst; (void)hPrev; (void)lpCmd; (void)nShow;

    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

    WCHAR pipeName[64];
    _snwprintf(pipeName, 64, PIPE_PREFIX L"%lu", sessionId);
    Log("[agent] inicio: sessao=%lu pipe=%ls\n", sessionId, pipeName);

    /* Permite que SYSTEM (a DLL no Spooler, sessão 0) conecte ao pipe */
    PSECURITY_DESCRIPTOR pSD = NULL;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(
        L"D:(A;;GA;;;SY)(A;;GA;;;CO)", SDDL_REVISION_1, &pSD, NULL);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), pSD, FALSE };

    for (;;) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            sizeof(PrintJobResponse),
            sizeof(PrintJobMsg),
            5000,
            &sa);

        if (hPipe == INVALID_HANDLE_VALUE) {
            Log("[agent] CreateNamedPipe falhou: %lu\n", GetLastError());
            break;
        }

        Log("[agent] aguardando conexao...\n");
        BOOL connected = ConnectNamedPipe(hPipe, NULL) ||
                         (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) {
            Log("[agent] ConnectNamedPipe falhou: %lu\n", GetLastError());
            CloseHandle(hPipe);
            continue;
        }

        PrintJobMsg msg = {0};
        DWORD bytesRead = 0;
        if (!ReadFile(hPipe, &msg, sizeof(msg), &bytesRead, NULL) ||
            bytesRead != sizeof(msg)) {
            Log("[agent] ReadFile falhou: %lu bytesRead=%lu\n", GetLastError(), bytesRead);
            DisconnectNamedPipe(hPipe);
            CloseHandle(hPipe);
            continue;
        }
        Log("[agent] job: ps=%ls out=%ls\n", msg.psTempPath, msg.outputPath);

        PrintJobResponse resp = {0};
        WCHAR cmdLine[2048];
        _snwprintf(cmdLine, 2048,
            L"\"%s\" -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=\"%s\" \"%s\"",
            msg.gsPath, msg.outputPath, msg.psTempPath);

        STARTUPINFOW si = {0};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        Log("[agent] executando GS...\n");

        if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            resp.exitCode = GetLastError();
            _snwprintf(resp.errorMsg, 512,
                L"CreateProcess falhou: %lu", resp.exitCode);
            Log("[agent] CreateProcess falhou: %lu\n", resp.exitCode);
        } else {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &resp.exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (resp.exitCode != 0)
                _snwprintf(resp.errorMsg, 512,
                    L"Ghostscript falhou: codigo %lu", resp.exitCode);
            Log("[agent] GS exitCode=%lu\n", resp.exitCode);
        }

        DWORD bytesWritten = 0;
        WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    if (pSD) LocalFree(pSD);
    return 0;
}
