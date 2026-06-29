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
#include <shellapi.h>
#include <commdlg.h>
#include <objbase.h>
#include <stdio.h>
#include <stdarg.h>

#define PIPE_PREFIX L"\\\\.\\pipe\\MeddrivePrinter_"
#define LOG_PATH    L"C:\\Windows\\Temp\\meddrive_printer.log"

typedef struct {
    WCHAR psTempPath[MAX_PATH];    // arquivo PS temporario gerado pelo Spooler
    WCHAR outputPath[MAX_PATH];    // pasta de destino 
    WCHAR outputBaseName[256];     // template do nome (ex: {documento}_{nnn})
    WCHAR docName[512];            // nome do job de impressao (ex: "Relatorio Abril")
    WCHAR gsPath[MAX_PATH];        // caminho do executavel do Ghostscript
    DWORD openAfterGenerate;       // 1 = abrir o PDF no visualizador apos gerar
    DWORD choosePath;              // 1 = mostrar dialogo "Salvar Como" antes de converter
    DWORD overwriteFile;           // 1 = sobrescrever se existir; 0 = incrementar contador
} PrintJobMsg;

typedef struct {
    DWORD exitCode;
    WCHAR errorMsg[512];
    WCHAR outputPath[MAX_PATH];
    DWORD userCancelled;
} PrintJobResponse;

// Troca caracteres proibidos em nomes de arquivo Windows por '_'.
static void sanitize_doc_name(const WCHAR *src, WCHAR *dst, int cch) {
    static const WCHAR invalid[] = L"\\/:*?\"<>|";
    int di = 0;
    for (int i = 0; src[i] && di < cch - 1; i++) {
        WCHAR c = src[i];
        dst[di++] = wcschr(invalid, c) ? L'_' : c;
    }
    dst[di] = 0;
}

// Percorre o template e substitui cada token pelo valor correspondente.
// nStr pode ser um numero formatado (ex: "003") ou L"*" para montar um padrao de busca.
static void apply_token_replacements(
    const WCHAR *tmpl, WCHAR *dst, int cchDst,
    const WCHAR *dateStr, const WCHAR *timeStr,
    const WCHAR *docStr,  const WCHAR *nStr)
{
    int di = 0;
    const WCHAR *p = tmpl;
    struct { const WCHAR *token; const WCHAR *value; } fixed[3] = {
        { L"{data}",      dateStr },
        { L"{hora}",      timeStr },
        { L"{documento}", docStr  },
    };
    while (*p && di < cchDst - 1) {
        if (*p != L'{') { dst[di++] = *p++; continue; }
        BOOL matched = FALSE;
        for (int k = 0; k < 3 && !matched; k++) {
            int tlen = (int)wcslen(fixed[k].token);
            if (wcsncmp(p, fixed[k].token, tlen) == 0) {
                for (int j = 0; fixed[k].value[j] && di < cchDst-1; j++)
                    dst[di++] = fixed[k].value[j];
                p += tlen;
                matched = TRUE;
            }
        }
        if (matched) continue;
        // {n}, {nn}, {nnn}: aceita qualquer largura de contador
        const WCHAR *q = p + 1;
        while (*q == L'n') q++;
        if (q > p + 1 && *q == L'}') {
            for (int j = 0; nStr[j] && di < cchDst-1; j++)
                dst[di++] = nStr[j];
            p = q + 1;
            continue;
        }
        dst[di++] = *p++;
    }
    dst[di] = 0;
}

// Retorna a largura do token {n+} (numero de 'n' entre chaves), ou 0 se nao houver.
static int detect_n_token_width(const WCHAR *tmpl) {
    for (int i = 0; tmpl[i]; i++) {
        if (tmpl[i] == L'{') {
            int j = i + 1, cnt = 0;
            while (tmpl[j] == L'n') { cnt++; j++; }
            if (cnt > 0 && tmpl[j] == L'}') return cnt;
        }
    }
    return 0;
}

// Decide o nome final do PDF a partir do template e grava o caminho completo em resolvedPath.
// Roda no agente (sessao do usuario) para ter acesso a pastas de rede.
static void resolve_output_path(
    const WCHAR *outputPath, const WCHAR *outputBaseName,
    const WCHAR *docName, DWORD overwriteFile,
    WCHAR *resolvedPath)
{
    const WCHAR *tmpl = outputBaseName;

    // template sem tokens vira "<nome>-{n}" para nao sobrescrever silenciosamente
    WCHAR adjustedTmpl[MAX_PATH];
    if (!wcschr(tmpl, L'{')) {
        _snwprintf(adjustedTmpl, MAX_PATH, L"%s-{n}", tmpl);
        tmpl = adjustedTmpl;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR dateStr[16], timeStr[16];
    _snwprintf(dateStr, 16, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    _snwprintf(timeStr, 16, L"%02d-%02d-%02d", st.wHour, st.wMinute, st.wSecond);

    WCHAR safeDoc[512] = {0};
    sanitize_doc_name(docName[0] ? docName : L"documento", safeDoc, 512);

    int nWidth = detect_n_token_width(tmpl);

    if (nWidth > 0) {
        WCHAR nStr[16];
        if (overwriteFile) {
            // sobrescrever ativado: sempre usa o numero 1, sem varrer a pasta
            _snwprintf(nStr, 16, nWidth == 1 ? L"%d" : L"%0*d", nWidth == 1 ? 1 : nWidth, 1);
        } else {
            // descobre o maior numero ja usado na pasta e usa o proximo
            WCHAR globName[MAX_PATH];
            // '?' cobre data e hora para encontrar arquivos de sessoes anteriores
            apply_token_replacements(tmpl, globName, MAX_PATH,
                                     L"??????????", L"????????", safeDoc, L"*");
            WCHAR globPattern[MAX_PATH];
            _snwprintf(globPattern, MAX_PATH, L"%s\\%s.pdf", outputPath, globName);

            const WCHAR *star = wcschr(globName, L'*');
            int prefixLen = star ? (int)(star - globName) : 0;
            int suffixLen = star ? (int)wcslen(star + 1) : 0;

            int maxN = 0;
            WIN32_FIND_DATAW fd;
            HANDLE hFind = FindFirstFileW(globPattern, &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    int fnLen = (int)wcslen(fd.cFileName);
                    int nStart = prefixLen;
                    int nEnd   = fnLen - 4 - suffixLen; // 4 = strlen(".pdf")
                    if (nEnd > nStart) {
                        WCHAR nBuf[16] = {0};
                        wcsncpy_s(nBuf, 16, fd.cFileName + nStart, (size_t)(nEnd - nStart));
                        int n = _wtoi(nBuf);
                        if (n > maxN) maxN = n;
                    }
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
            _snwprintf(nStr, 16, nWidth == 1 ? L"%d" : L"%0*d", nWidth == 1 ? maxN+1 : nWidth, maxN + 1);
        }
        WCHAR finalName[MAX_PATH];
        apply_token_replacements(tmpl, finalName, MAX_PATH, dateStr, timeStr, safeDoc, nStr);
        _snwprintf(resolvedPath, MAX_PATH, L"%s\\%s.pdf", outputPath, finalName);
    } else {
        // sem token {n}: resolve direto; se o arquivo ja existe e nao pode sobrescrever,
        // usa o padrao do Windows: "nome (2).pdf", "nome (3).pdf"...
        WCHAR finalName[MAX_PATH];
        apply_token_replacements(tmpl, finalName, MAX_PATH, dateStr, timeStr, safeDoc, L"");
        _snwprintf(resolvedPath, MAX_PATH, L"%s\\%s.pdf", outputPath, finalName);
        if (!overwriteFile && GetFileAttributesW(resolvedPath) != INVALID_FILE_ATTRIBUTES) {
            int copy = 1;
            do {
                _snwprintf(resolvedPath, MAX_PATH, L"%s\\%s (%d).pdf", outputPath, finalName, copy++);
            } while (GetFileAttributesW(resolvedPath) != INVALID_FILE_ATTRIBUTES);
        }
    }
}

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

static BOOL IsWin7OrOlder(void) {
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOEXW *);
    RtlGetVersionFn fn = (RtlGetVersionFn)
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    if (!fn) return FALSE;
    OSVERSIONINFOEXW osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    fn(&osvi);
    return osvi.dwMajorVersion < 10;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hInst; (void)hPrev; (void)lpCmd; (void)nShow;

    BOOL needCoInit = IsWin7OrOlder();
    if (needCoInit)
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    DWORD sessionId = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

    WCHAR pipeName[64];
    _snwprintf(pipeName, 64, PIPE_PREFIX L"%lu", sessionId);
    Log("[agent] inicio: sessao=%lu pipe=%ls\n", sessionId, pipeName);

    /* NULL DACL: concede acesso total a qualquer processo local.
       O pipe é IPC local (sem exposição de rede), necessário para que
       o SYSTEM (Spooler, sessão 0) consiga conectar ao pipe do usuário. */
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), &sd, FALSE };

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
        Log("[agent] [JOB] \"%ls\" -> %ls | template: %ls\n",
            msg.docName, msg.outputPath, msg.outputBaseName);
        Log("[agent] [GS PATH] \"%ls\"\n", msg.gsPath);

        PrintJobResponse resp = {0};

        // resolve o nome do arquivo com as credenciais do usuario (acessa pastas de rede)
        WCHAR resolvedPath[MAX_PATH] = {0};
        resolve_output_path(msg.outputPath, msg.outputBaseName,
                            msg.docName, msg.overwriteFile, resolvedPath);
        Log("[agent] caminho resolvido: %ls\n", resolvedPath);

        if (msg.choosePath) {
            // usa o nome resolvido como sugestao; o usuario pode alterar antes de salvar
            OPENFILENAMEW ofn = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = L"Arquivos PDF (*.pdf)\0*.pdf\0";
            ofn.lpstrFile   = resolvedPath;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrTitle  = L"Salvar PDF";
            ofn.lpstrDefExt = L"pdf";
            ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

            if (!GetSaveFileNameW(&ofn)) {
                Log("[agent] [CANCELADO] usuario encerrou o dialogo\n");
                resp.userCancelled = 1;
                DWORD w = 0;
                WriteFile(hPipe, &resp, sizeof(resp), &w, NULL);
                FlushFileBuffers(hPipe);
                DisconnectNamedPipe(hPipe);
                CloseHandle(hPipe);
                continue;
            }
            Log("[agent] caminho escolhido: %ls\n", resolvedPath);
        }

        WCHAR cmdLine[2048];
        _snwprintf(cmdLine, 2048,
            L"\"%s\" -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=\"%s\" \"%s\"",
            msg.gsPath, resolvedPath, msg.psTempPath);

        STARTUPINFOW si = {0};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {0};
        Log("[agent] [GS] convertendo para PDF...\n");

        if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                            CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            resp.exitCode = GetLastError();
            _snwprintf(resp.errorMsg, 512,
                L"CreateProcess falhou: %lu", resp.exitCode);
            Log("[agent] [FALHOU] CreateProcess falhou: %lu\n", resp.exitCode);
        } else {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &resp.exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            if (resp.exitCode == 0) {
                // GS as vezes retorna 0 mesmo sem gravar — confirma o arquivo
                if (GetFileAttributesW(resolvedPath) == INVALID_FILE_ATTRIBUTES) {
                    resp.exitCode = 1;
                    _snwprintf(resp.errorMsg, 512,
                        L"PDF nao encontrado apos conversao: %ls", resolvedPath);
                    Log("[agent] [FALHOU] PDF nao encontrado apos conversao: %ls\n", resolvedPath);
                } else {
                    Log("[agent] [PDF] %ls\n", resolvedPath);
                    Log("[agent] [OK] job concluido\n");
                    if (msg.openAfterGenerate) {
                        HINSTANCE r = ShellExecuteW(NULL, L"open", resolvedPath, NULL, NULL, SW_SHOWNORMAL);
                        Log("[agent] ShellExecuteW: %Id%s\n", (INT_PTR)r,
                            (INT_PTR)r <= 32 ? " (FALHOU)" : " (OK)");
                    }
                }
            } else {
                _snwprintf(resp.errorMsg, 512,
                    L"Ghostscript falhou: codigo %lu", resp.exitCode);
                Log("[agent] [FALHOU] Ghostscript retornou codigo %lu\n", resp.exitCode);
            }
        }

        wcsncpy_s(resp.outputPath, MAX_PATH, resolvedPath, _TRUNCATE);

        DWORD bytesWritten = 0;
        WriteFile(hPipe, &resp, sizeof(resp), &bytesWritten, NULL);
        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }

    return 0;
}
