#include "monitor.h"
#include <stdio.h>
#include <stdarg.h>
#include <wtsapi32.h>

static HKEY        g_hkRoot   = NULL;
static MONITOR2    g_monitor2 = {0};

// Auxiliares

// escreve uma linha formatada no log de diagnóstico
// abre, escreve e fecha o arquivo a cada chamada para garantir que nada se perca em caso de crash
// temporária — será removida quando o problema de carregamento do monitor for resolvido
static void LogDebug(const char *fmt, ...) {
    HANDLE hLog = CreateFileW(L"C:\\Windows\\Temp\\meddrive_printer.log",
        FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hLog == INVALID_HANDLE_VALUE) return;
    char  buf[512];
    DWORD w;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    WriteFile(hLog, buf, (DWORD)(len > 0 ? len : 0), &w, NULL);
    CloseHandle(hLog);
}

// Abre a chave do registry
// e lê as configurações necessárias para o monitor (caminho de saída e caminho do Ghostscript)
// guarda em memória alocada em ctx
// se ambos os caminhos forem lidos com sucesso retorna TRUE,
// se nao encontrar retorna FALSE e cancela o job do spooler
static BOOL ReadConfig(PORT_CONTEXT *ctx, LPCWSTR portName) {

    // parametros dentro do registry
    HKEY  hKey;
    DWORD size;

    // constrói o caminho da chave a partir do nome da porta recebida do spooler
    WCHAR keyPath[512];
    swprintf(keyPath, 512, L"Ports\\%ls", portName);
    LogDebug("ReadConfig: abrindo %ls\n", keyPath);

    // lê as configurações do registry, se nao encontrar retorna false
    LONG rc = RegOpenKeyExW(g_hkRoot, keyPath, 0, KEY_READ, &hKey);
    LogDebug("ReadConfig: RegOpenKey resultado=%ld (%s)\n",
        rc, rc == ERROR_SUCCESS ? "SUCCESS" : "FALHOU");
    if (rc != ERROR_SUCCESS) return FALSE;

    // lê a pasta de destino
    size = sizeof(ctx->outputPath);
    RegQueryValueExW(hKey, L"OutputPath", NULL, NULL, (LPBYTE)ctx->outputPath, &size);
    LogDebug("ReadConfig: OutputPath lido (len=%lu)\n", size);

    // lê o nome base do arquivo; usa "saida" como fallback para impressoras antigas
    size = sizeof(ctx->outputBaseName);
    RegQueryValueExW(hKey, L"OutputBaseName", NULL, NULL, (LPBYTE)ctx->outputBaseName, &size);
    if (ctx->outputBaseName[0] == L'\0') {
        wcscpy_s(ctx->outputBaseName, 256, L"saida");
        LogDebug("ReadConfig: OutputBaseName ausente, usando fallback 'saida'\n");
    } else {
        LogDebug("ReadConfig: OutputBaseName lido: %ls\n", ctx->outputBaseName);
    }

    // lê o ghostscriptpath
    size = sizeof(ctx->ghostscriptPath);
    RegQueryValueExW(hKey, L"GhostscriptPath", NULL, NULL, (LPBYTE)ctx->ghostscriptPath, &size);
    LogDebug("ReadConfig: GhostscriptPath lido (len=%lu)\n", size);

    // lê flags opcionais (falha silenciosa — valor padrão 0 já está no ctx zerado)
    size = sizeof(DWORD);
    RegQueryValueExW(hKey, L"OpenAfterGenerate", NULL, NULL, (LPBYTE)&ctx->openAfterGenerate, &size);
    size = sizeof(DWORD);
    RegQueryValueExW(hKey, L"OverwriteFile",     NULL, NULL, (LPBYTE)&ctx->overwriteFile,     &size);
    size = sizeof(DWORD);
    RegQueryValueExW(hKey, L"ChoosePath",        NULL, NULL, (LPBYTE)&ctx->choosePath,        &size);
    LogDebug("ReadConfig: OpenAfterGenerate=%lu OverwriteFile=%lu ChoosePath=%lu\n",
        ctx->openAfterGenerate, ctx->overwriteFile, ctx->choosePath);

    // fecha a chave do registry, retorna TRUE se ambos os caminhos foram lidos com sucesso, FALSE caso contrário
    RegCloseKey(hKey);
    BOOL ok = ctx->outputPath[0] != L'\0' && ctx->ghostscriptPath[0] != L'\0';
    LogDebug("ReadConfig: resultado final=%s\n", ok ? "OK" : "FALHOU (paths vazios)");
    return ok;
}

// ═══════════════════════════════════════════════════════════════════════════

// Structs trocadas pelo pipe entre o Spooler (sessao 0) e o agente (sessao 1).
// O monitor envia os ingredientes brutos; o agente resolve o nome do arquivo
// com suas proprias credenciais (necessario para pastas de rede).
typedef struct {
    WCHAR psTempPath[MAX_PATH];    // arquivo PS temporario gerado pelo Spooler
    WCHAR outputPath[MAX_PATH];    // pasta de destino (ex: C:\PDFs ou \\servidor\PDFs)
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

// Envia o job ao agente na sessao do usuario e aguarda o resultado de forma sincrona.
// pCancelled é preenchido com TRUE se o usuario cancelou o dialogo de salvar.
static BOOL CallAgentForConversion(PORT_CONTEXT *ctx, BOOL *pCancelled) {
    *pCancelled = FALSE;
    // WTSGetActiveConsoleSessionId funciona a partir da sessao 0 (Spooler).
    // ImpersonatePrinterClient nao funciona aqui porque winspool.drv nao esta carregado em spoolsv.exe
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    LogDebug("CallAgent: sessionId=%lu\n", sessionId);

    WCHAR pipeName[64];
    _snwprintf(pipeName, 64, L"\\\\.\\pipe\\MeddrivePrinter_%lu", sessionId);
    LogDebug("CallAgent: conectando em %ls\n", pipeName);

    HANDLE hPipe = CreateFileW(pipeName,
        GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hPipe == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        // ERROR_PIPE_BUSY: o agente esta rodando mas ocupado com outro job.
        // espera ate 5s e tenta de novo antes de desistir
        if (err == ERROR_PIPE_BUSY) {
            WaitNamedPipeW(pipeName, 5000);
            hPipe = CreateFileW(pipeName,
                GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        }
    }
    if (hPipe == INVALID_HANDLE_VALUE) {
        // agente nao esta rodando: cancela o job e avisa o usuario com uma caixa de dialogo
        LogDebug("CallAgent: pipe indisponivel err=%lu, agente nao esta rodando\n",
            GetLastError());
        static const WCHAR title[] = L"Meddrive Printer";
        static const WCHAR msg[]   =
            L"O MeddrivePrinterAgent nao esta em execucao.\n\n"
            L"O job de impressao foi cancelado.\n\n"
            L"Verifique se a tarefa 'MeddrivePrinterAgent' esta ativa no Agendador de Tarefas "
            L"e faca login novamente para inicia-la.";
        DWORD response = 0;
        // WTSSendMessageW exibe a caixa de dialogo na sessao do usuario (1) mesmo sendo chamado pelo Spooler (0).
        // os parametros de tamanho sao em bytes, nao em caracteres, por isso o * sizeof(WCHAR)
        WTSSendMessageW(WTS_CURRENT_SERVER_HANDLE, sessionId,
            (LPWSTR)title, (DWORD)(wcslen(title) * sizeof(WCHAR)),
            (LPWSTR)msg,   (DWORD)(wcslen(msg)   * sizeof(WCHAR)),
            MB_OK | MB_ICONERROR, 0, &response, FALSE);
        return FALSE;
    }

    // envia os ingredientes brutos; o agente resolve o nome do arquivo do lado dele,
    // onde tem credenciais do usuario para acessar pastas de rede
    PrintJobMsg msg = {0};
    wcscpy_s(msg.psTempPath,    MAX_PATH, ctx->tempPsFile);
    wcscpy_s(msg.outputPath,    MAX_PATH, ctx->outputPath);
    wcscpy_s(msg.outputBaseName, 256,    ctx->outputBaseName);
    wcscpy_s(msg.docName,        512,    ctx->docName);
    wcscpy_s(msg.gsPath,        MAX_PATH, ctx->ghostscriptPath);
    msg.openAfterGenerate = ctx->openAfterGenerate;
    msg.choosePath        = ctx->choosePath;
    msg.overwriteFile     = ctx->overwriteFile;
    DWORD written = 0;
    WriteFile(hPipe, &msg, sizeof(msg), &written, NULL);
    LogDebug("CallAgent: msg enviada (%lu bytes)\n", written);

    PrintJobResponse resp = {0};
    DWORD bytesRead = 0;
    ReadFile(hPipe, &resp, sizeof(resp), &bytesRead, NULL);
    CloseHandle(hPipe);

    if (resp.userCancelled) {
        *pCancelled = TRUE;
        LogDebug("CallAgent: usuario cancelou o dialogo\n");
        return FALSE;
    }

    // atualiza resolvedPath com o caminho real usado pelo agente
    // (pode diferir quando choosePath esta ativo)
    if (resp.outputPath[0] != L'\0')
        wcscpy_s(ctx->resolvedPath, MAX_PATH, resp.outputPath);

    LogDebug("CallAgent: exitCode=%lu path=%ls\n", resp.exitCode, ctx->resolvedPath);
    return resp.exitCode == 0;
}


// Funções do monitor que serão chamadas pelo spooler

// Enumera as portas suportadas pelo monitor (apenas uma porta virtual)
// se for 1 retorna apenas o nome da porta
// se for 2 retorna nome da porta + nome do monitor + descrição
// Dois níveis para que seja compatível com diferentes versões do Windows
// Alguns pedem apenas o nome, outros pedem nome + descrição + monitor para mostrar na lista de impressoras
static BOOL WINAPI Monitor_EnumPorts(
    HANDLE hMonitor,      // handle do monitor
    LPWSTR pName,         // nome do servidor (NULL = local)
    DWORD Level,          // nível de "detalhe" pedido (1 ou 2)
    LPBYTE pPorts,        // buffer onde escrevemos a lista de portas
    DWORD cbBuf,          // tamanho do buffer em bytes
    LPDWORD pcbNeeded,    // buffer para armazenar quantos bytes precisamos
    LPDWORD pcReturned)   // buffer para armazenar quantas portas retornamos
{
    LogDebug("[inicio] EnumPorts Level=%lu cbBuf=%lu\n", Level, cbBuf);

    static const WCHAR PORT_DESC[] = L"Impressora Virtual PDF";

    if (Level != 1 && Level != 2) {
        SetLastError(ERROR_INVALID_LEVEL);
        return FALSE;
    }

    // abre a chave Ports\ para enumerar todas as portas registradas
    HKEY hPortsKey;
    LONG rc = RegOpenKeyExW(g_hkRoot, L"Ports", 0, KEY_READ, &hPortsKey);
    if (rc != ERROR_SUCCESS) {
        LogDebug("[meio] EnumPorts ERRO chave Ports rc=%ld\n", rc);
        *pcbNeeded  = 0;
        *pcReturned = 0;
        return TRUE;
    }

    // primeira passagem: conta portas e calcula bytes necessários
    DWORD portCount = 0;
    DWORD needed    = 0;
    DWORD idx       = 0;
    WCHAR portName[256];
    DWORD portNameLen;

    for (;;) {
        portNameLen = 256;
        rc = RegEnumKeyExW(hPortsKey, idx, portName, &portNameLen, NULL, NULL, NULL, NULL);
        if (rc == ERROR_NO_MORE_ITEMS) break;
        if (rc != ERROR_SUCCESS) break;

        DWORD nameBytes = (portNameLen + 1) * sizeof(WCHAR);
        if (Level == 1)
            needed += sizeof(PORT_INFO_1W) + nameBytes;
        else
            needed += sizeof(PORT_INFO_2W) + nameBytes
                    + (DWORD)((wcslen(MONITOR_NAME) + 1) * sizeof(WCHAR))
                    + (DWORD)((wcslen(PORT_DESC)    + 1) * sizeof(WCHAR));
        portCount++;
        idx++;
    }

    *pcbNeeded  = needed;
    *pcReturned = 0;

    LogDebug("[meio] EnumPorts %lu porta(s) | needed=%lu bytes\n", portCount, needed);

    if (cbBuf < needed) {
        RegCloseKey(hPortsKey);
        LogDebug("[meio] EnumPorts ERRO buffer insuficiente needed=%lu cbBuf=%lu\n", needed, cbBuf);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    // segunda passagem: preenche o buffer
    // structs ficam no início; strings ficam logo após o array de structs
    DWORD   structSize = (Level == 1) ? sizeof(PORT_INFO_1W) : sizeof(PORT_INFO_2W);
    LPBYTE  strArea    = pPorts + portCount * structSize;
    DWORD   filled     = 0;

    for (idx = 0; filled < portCount; idx++) {
        portNameLen = 256;
        rc = RegEnumKeyExW(hPortsKey, idx, portName, &portNameLen, NULL, NULL, NULL, NULL);
        if (rc != ERROR_SUCCESS) break;

        if (Level == 1) {
            PORT_INFO_1W *pInfo = (PORT_INFO_1W *)(pPorts + filled * structSize);
            pInfo->pName = (LPWSTR)strArea;
            wcscpy((LPWSTR)strArea, portName);
            strArea += (portNameLen + 1) * sizeof(WCHAR);
        } else {
            PORT_INFO_2W *pInfo = (PORT_INFO_2W *)(pPorts + filled * structSize);

            pInfo->pPortName = (LPWSTR)strArea;
            wcscpy((LPWSTR)strArea, portName);
            strArea += (portNameLen + 1) * sizeof(WCHAR);

            pInfo->pMonitorName = (LPWSTR)strArea;
            wcscpy((LPWSTR)strArea, MONITOR_NAME);
            strArea += (wcslen(MONITOR_NAME) + 1) * sizeof(WCHAR);

            pInfo->pDescription = (LPWSTR)strArea;
            wcscpy((LPWSTR)strArea, PORT_DESC);
            strArea += (wcslen(PORT_DESC) + 1) * sizeof(WCHAR);

            pInfo->fPortType = PORT_TYPE_WRITE;
            pInfo->Reserved  = 0;
        }
        filled++;
    }

    RegCloseKey(hPortsKey);
    LogDebug("[fim] EnumPorts OK %lu porta(s) Level=%lu\n", portCount, Level);
    *pcReturned = portCount;
    return TRUE;
}

// prepara a memória para armazenar as informações do job de impressão
// (caminho do arquivo PostScript gerado pelo spooler e o caminho do Ghostscript)
// todas as funções vão receber esse handle resultado dessa função
// que aponta para a variável do estado do job do spooler
static BOOL WINAPI Monitor_OpenPort(HANDLE hMonitor, LPWSTR pName, PHANDLE pHandle) {
    // log: confirma que o Spooler chama OpenPort após EnumPorts
    LogDebug("OpenPort: hMonitor=%p pName=%ls pHandle=%p\n",
        (void*)hMonitor, pName ? pName : L"<NULL>", (void*)pHandle);
    // inicia a variável para armazenar as informações do job de impressão e as configurações lidas do registry
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PORT_CONTEXT));

    // se não conseguir alocar memória para armazenar as informações do job
    // retorna FALSE e erro de memória insuficiente
    if (!ctx) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    // inicializa o handle do arquivo temporário como inválido,
    // serve para o close port não tentar fechar um handle caso falhe antes de criar o arquivo temporário
    ctx->hTempFile = INVALID_HANDLE_VALUE;

    // lê as configurações do registry
    // tenta preencher o ctx com o caminho de saída e caminho do Ghostscript
    // se não conseguir ler as configurações necessárias
    // libera a memória alocada
    // retorna FALSE e erro de falha ao abrir
    if (!ReadConfig(ctx, pName)) {
        HeapFree(GetProcessHeap(), 0, ctx);
        SetLastError(ERROR_OPEN_FAILED);
        return FALSE;
    }

    wcsncpy_s(ctx->portName, 256, pName, _TRUNCATE);

    // retorna o handle para as outras funções do monitor
    // que vai apontar para a variável com as informações do job de impressão
    *pHandle = (HANDLE)ctx;
    return TRUE;
}

// recebe a memória alocada no open port com as informações do job de impressão
// cria um arquivo temporário para armazenar o conteúdo PostScript enviado pelo spooler
static BOOL WINAPI Monitor_StartDocPort(
    HANDLE hPort, LPWSTR pPrinterName, DWORD JobId, DWORD Level, LPBYTE pDocInfo)
{
    LogDebug("StartDocPort: hPort=%p pPrinterName=%ls JobId=%lu Level=%lu\n",
        (void*)hPort,
        pPrinterName ? pPrinterName : L"<NULL>",
        JobId, Level);
 
    // cast do handle para acessar as informações do job de impressão
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;
    WCHAR         tempDir[MAX_PATH];

    // re-lê configurações voláteis do registry a cada job (o OpenPort é chamado uma vez e o ctx fica cacheado)
    {
        WCHAR keyPath[512];
        swprintf(keyPath, 512, L"Ports\\%ls", ctx->portName);
        HKEY hKey;
        if (RegOpenKeyExW(g_hkRoot, keyPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD sz = sizeof(DWORD);
            RegQueryValueExW(hKey, L"OpenAfterGenerate", NULL, NULL, (LPBYTE)&ctx->openAfterGenerate, &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hKey, L"OverwriteFile",     NULL, NULL, (LPBYTE)&ctx->overwriteFile,     &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hKey, L"ChoosePath",        NULL, NULL, (LPBYTE)&ctx->choosePath,        &sz);
            RegCloseKey(hKey);
        }
    }

    // cria o arquivo temporário para armazenar o conteúdo enviado pelo spooler
    GetTempPathW(MAX_PATH, tempDir);
    GetTempFileNameW(tempDir, L"pdfmon", 0, ctx->tempPsFile);
    LogDebug("StartDocPort: arquivo temporario = %ls\n", ctx->tempPsFile);
 
    // abre o arquivo temporário para escrita
    ctx->hTempFile = CreateFileW(
        ctx->tempPsFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
 
    BOOL ok = ctx->hTempFile != INVALID_HANDLE_VALUE;
    LogDebug("StartDocPort: CreateFile %s (handle=%p err=%lu)\n",
        ok ? "OK" : "FALHOU",
        (void*)ctx->hTempFile,
        ok ? 0 : GetLastError());

    // captura o nome do documento do job de impressão
    ctx->docName[0] = L'\0';
    if (pDocInfo && Level >= 1) {
        DOC_INFO_1W *di = (DOC_INFO_1W *)pDocInfo;
        if (di->pDocName)
            wcsncpy_s(ctx->docName, 512, di->pDocName, _TRUNCATE);
        LogDebug("StartDocPort: docName = %ls\n", ctx->docName);
    }

    ctx->jobId             = JobId;
    ctx->totalBytesWritten = 0;
    wcsncpy_s(ctx->printerName, 256, pPrinterName ? pPrinterName : L"", _TRUNCATE);

    SYSTEMTIME st;
    GetLocalTime(&st);
    LogDebug("[JOB] #%lu | \"%ls\" | %ls | destino: %ls | %04u-%02u-%02u %02u:%02u:%02u\n",
        JobId, ctx->docName, ctx->printerName, ctx->outputPath,
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    if (ok)
        LogDebug("[inicio] WritePort aguardando dados do Spooler\n");

    return ok;
}


// função para escrever os dados dentro do arquivo temporário criado no StartDocPort
static BOOL WINAPI Monitor_WritePort(
    HANDLE hPort,        // handle que aponta para o PORT_CONTEXT do job
    LPBYTE pBuffer,      // ponteiro para os bytes PostScript que chegaram
    DWORD cbBuf,         // quantos bytes chegaram nessa chamada
    LPDWORD pcbWritten)   // devolve quantos bytes foram gravados

{
    // cast do handle para acessar as informações do job de impressão
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;
    if (!WriteFile(ctx->hTempFile, pBuffer, cbBuf, pcbWritten, NULL)) {
        LogDebug("[meio] WritePort ERRO WriteFile err=%lu\n", GetLastError());
        return FALSE;
    }
    ctx->totalBytesWritten += *pcbWritten;
    return TRUE;
}

// função de leitura do monitor
// a princípio não é necessária para o funcionamento,
// no entanto é necessária pois o contrato com o Spooler exije essa função
// a fim de garantir a compatibilidade com diferentes versões do Windows (algumas chamam o ReadPort)
// e para o Spooler não interpretar lixo de memória como resposta, retorna sempre 0 bytes lidos e TRUE
static BOOL WINAPI Monitor_ReadPort(
    HANDLE hPort, LPBYTE pBuffer, DWORD cbBuf, LPDWORD pcbRead)
{
    *pcbRead = 0;
    return TRUE;
}

// função chamada pelo spooler quando o job de impressão é finalizado
// fecha o arquivo temporário onde o conteúdo PostScript foi armazenado
typedef struct {
    WCHAR printerName[256];
    DWORD jobId;
} DeleteJobCtx;

static DWORD WINAPI delete_job_thread(LPVOID param) {
    DeleteJobCtx *j = (DeleteJobCtx *)param;
    HANDLE hPrinter = NULL;
    if (OpenPrinterW(j->printerName, &hPrinter, NULL)) {
        SetJobW(hPrinter, j->jobId, 0, NULL, JOB_CONTROL_DELETE);
        ClosePrinter(hPrinter);
        LogDebug("[OK] job #%lu entregue\n", j->jobId);
    }
    HeapFree(GetProcessHeap(), 0, j);
    return 0;
}

static BOOL WINAPI Monitor_EndDocPort(HANDLE hPort) {
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;

    CloseHandle(ctx->hTempFile);
    ctx->hTempFile = INVALID_HANDLE_VALUE;
    LogDebug("[fim] WritePort %lu bytes recebidos\n", ctx->totalBytesWritten);
    LogDebug("EndDocPort: arquivo=%ls\n", ctx->tempPsFile);

    LogDebug("[CONV] chamando agente...\n");
    BOOL cancelled = FALSE;
    BOOL ok = CallAgentForConversion(ctx, &cancelled);
    LogDebug("EndDocPort: agente=%s cancelled=%d\n", ok ? "OK" : "FALHOU", cancelled);
    DeleteFileW(ctx->tempPsFile);

    if (cancelled)
        LogDebug("[CANCELADO] usuario encerrou o dialogo\n");
    else if (ok)
        LogDebug("[PDF] %ls\n", ctx->resolvedPath);
    else
        LogDebug("[FALHOU] agente retornou erro\n");

    // o agente ja verificou o PDF do lado dele (com credenciais do usuario);
    // o monitor so precisa saber se deu certo ou nao
    // remove o job da fila em caso de sucesso ou cancelamento pelo usuario
    if (ok || cancelled) {
        DeleteJobCtx *j = HeapAlloc(GetProcessHeap(), 0, sizeof(DeleteJobCtx));
        if (j) {
            wcsncpy_s(j->printerName, 256, ctx->printerName, _TRUNCATE);
            j->jobId = ctx->jobId;
            HANDLE t = CreateThread(NULL, 0, delete_job_thread, j, 0, NULL);
            if (t) CloseHandle(t);
            else HeapFree(GetProcessHeap(), 0, j);
        }
        if (cancelled) ok = TRUE;
    }

    return ok;
}


// função chamada pelo spooler para fechar a porta de impressão
// libera a memória alocada
// caso o arquivo temporário ainda esteja aberto, fecha o handle para evitar vazamento de handle
static BOOL WINAPI Monitor_ClosePort(HANDLE hPort) {
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;

    if (ctx->hTempFile != INVALID_HANDLE_VALUE)
        CloseHandle(ctx->hTempFile);

    HeapFree(GetProcessHeap(), 0, ctx);
    return TRUE;
}

// Stubs XCV — interface de configuração bidirecional entre monitor e ferramentas do Windows
// O Spooler no Windows 10/11 pode exigir que esses ponteiros sejam não-NULL para validar o monitor
// Os stubs apenas sinalizam que o monitor não suporta configuração via XCV
static BOOL WINAPI Monitor_XcvOpenPort(
    HANDLE hMonitor, LPCWSTR pObject, DWORD dwGrantedAccess, PHANDLE phXcv)
{
    // O spooler exige TRUE aqui para aceitar conexões XcvMonitor.
    // Retornar FALSE fazia o AddPrinterW falhar com 1801 porque o spooler
    // testa o canal XCV antes de validar a porta no AddPrinterW.
    // Devolvemos um handle fictício não-NULL para satisfazer o contrato.
    (void)hMonitor; (void)pObject; (void)dwGrantedAccess;
    if (phXcv) *phXcv = (HANDLE)1;
    return TRUE;
}

static DWORD WINAPI Monitor_XcvDataPort(
    HANDLE hXcv, LPCWSTR pszDataName,
    PBYTE pInputData, DWORD cbInputData,
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded)
{
    // loga o nome do comando XCV para identificar o que o Edge/Chrome estão pedindo
    // o Edge chama XcvDataPort para consultar capacidades antes de renderizar a prévia
    LogDebug("XcvDataPort: pszDataName=%ls cbInput=%lu cbOutput=%lu\n",
        pszDataName ? pszDataName : L"<NULL>",
        cbInputData, cbOutputData);

    (void)hXcv; (void)pInputData;
    (void)pOutputData; (void)pcbOutputNeeded;

    // MonitorUI — o spooler/Edge pede a DLL de UI do monitor
    // retornar ERROR_NOT_SUPPORTED faz o Edge ficar esperando indefinidamente
    // retornar ERROR_INSUFFICIENT_BUFFER com pcbOutputNeeded=0 encerra a query corretamente
    if (pszDataName && wcscmp(pszDataName, L"MonitorUI") == 0) {
        LogDebug("XcvDataPort: MonitorUI query — retornando sem UI\n");
        if (pcbOutputNeeded) *pcbOutputNeeded = 0;
        return ERROR_SUCCESS;
    }

    // PortIsLocal — alguns componentes perguntam se a porta é local
    if (pszDataName && wcscmp(pszDataName, L"PortIsLocal") == 0) {
        LogDebug("XcvDataPort: PortIsLocal query\n");
        if (pOutputData && cbOutputData >= sizeof(DWORD))
            *(DWORD*)pOutputData = 1;
        if (pcbOutputNeeded) *pcbOutputNeeded = sizeof(DWORD);
        return ERROR_SUCCESS;
    }

    // qualquer outro comando XCV não suportado — loga e retorna
    LogDebug("XcvDataPort: comando nao suportado — retornando ERROR_NOT_SUPPORTED\n");
    if (pcbOutputNeeded) *pcbOutputNeeded = 0;
    return ERROR_NOT_SUPPORTED;
}

static BOOL WINAPI Monitor_XcvClosePort(HANDLE hXcv)
{
    (void)hXcv;
    return TRUE;
}

// Stubs de ciclo de vida — campos obrigatórios da MONITOR2 completa (Windows 2000+/XP+/7+)
// Ausência desses campos fazia cbSize=144 < MONITOR2_SIZE_WIN2K=148, causando erro 3007
static VOID WINAPI Monitor_Shutdown(HANDLE hMonitor)
{
    (void)hMonitor;
}

static DWORD WINAPI Monitor_SendRecvBidi(
    HANDLE hPort, DWORD dwAccessBit, LPCWSTR pAction,
    PBIDI_REQUEST_CONTAINER pReqData, PBIDI_RESPONSE_CONTAINER *ppResData)
{
    (void)hPort; (void)dwAccessBit; (void)pAction;
    (void)pReqData; (void)ppResData;
    return ERROR_NOT_SUPPORTED;
}

static DWORD WINAPI Monitor_NotifyUsedPorts(
    HANDLE hMonitor, DWORD cPorts, PCWSTR *ppszPorts)
{
    (void)hMonitor; (void)cPorts; (void)ppszPorts;
    return ERROR_NOT_SUPPORTED;
}

static DWORD WINAPI Monitor_NotifyUnusedPorts(
    HANDLE hMonitor, DWORD cPorts, PCWSTR *ppszPorts)
{
    (void)hMonitor; (void)cPorts; (void)ppszPorts;
    return ERROR_NOT_SUPPORTED;
}

static BOOL WINAPI Monitor_AddPortEx(
    HANDLE  hMonitor,
    LPWSTR  pName,
    DWORD   Level,
    LPBYTE  lpBuffer,
    LPWSTR  lpMonitorName)
{
    LogDebug("AddPortEx: Level=%lu pName=%ls monitorName=%ls\n",
        Level,
        pName       ? pName       : L"<NULL>",
        lpMonitorName ? lpMonitorName : L"<NULL>");
    (void)hMonitor; (void)pName; (void)Level;
    (void)lpBuffer; (void)lpMonitorName;
    return TRUE;
}

// Entrada da DLL chamada pelo Windows para carregar antes de chamar as funções
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL; (void)fdwReason; (void)lpvReserved;
    return TRUE;
}

// Função com o único nome que o Spooler reconhece para inicializar o monitor de impressão
// e utilizar as funções definidas na dll para lidar com os jobs de impressão
LPMONITOR2 WINAPI InitializePrintMonitor2(PMONITORINIT pMonitorInit, PHANDLE phMonitor) {
    // log: confirma que a função foi chamada e registra os parâmetros recebidos do Spooler
    LogDebug("InitializePrintMonitor2: pMonitorInit=%p phMonitor=%p\n",
        (void*)pMonitorInit, (void*)phMonitor);
    if (pMonitorInit)
        LogDebug("  cbSize=%lu hckRegistryRoot=%p\n",
            pMonitorInit->cbSize, (void*)pMonitorInit->hckRegistryRoot);

    if (!pMonitorInit || !phMonitor) return NULL;

    // chave global do registry para dar acesso ao monitor ler as configurações
    g_hkRoot = (HKEY)pMonitorInit->hckRegistryRoot;
    // handle do monitor devolvido ao Spooler
    *phMonitor = (HANDLE)1;

    // define como o Spooler vai chamar as funções do monitor, preenchendo a estrutura MONITOR2
    g_monitor2.cbSize                       = sizeof(MONITOR2);
    g_monitor2.pfnEnumPorts                 = Monitor_EnumPorts;
    g_monitor2.pfnOpenPort                  = Monitor_OpenPort;
    g_monitor2.pfnStartDocPort              = Monitor_StartDocPort;
    g_monitor2.pfnWritePort                 = Monitor_WritePort;
    g_monitor2.pfnReadPort                  = Monitor_ReadPort;
    g_monitor2.pfnEndDocPort                = Monitor_EndDocPort;
    g_monitor2.pfnClosePort                 = Monitor_ClosePort;
    g_monitor2.pfnXcvOpenPort               = Monitor_XcvOpenPort;
    g_monitor2.pfnXcvDataPort               = Monitor_XcvDataPort;
    g_monitor2.pfnXcvClosePort              = Monitor_XcvClosePort;
    g_monitor2.pfnShutdown                  = Monitor_Shutdown;
    g_monitor2.pfnSendRecvBidiDataFromPort  = Monitor_SendRecvBidi;
    g_monitor2.pfnNotifyUsedPorts           = Monitor_NotifyUsedPorts;
    g_monitor2.pfnNotifyUnusedPorts         = Monitor_NotifyUnusedPorts;
    g_monitor2.pfnAddPortEx                 = Monitor_AddPortEx;

    // log: confirma tamanho e ponteiros do MONITOR2 antes de retornar ao Spooler
    // %lu com cast porque msvcrt.dll não suporta %zu (C99)
    LogDebug("sizeof(MONITOR2)=%lu cbSize=%lu ret=%p\n"
             "  EnumPorts=%p OpenPort=%p StartDoc=%p\n"
             "  Write=%p Read=%p EndDoc=%p Close=%p\n"
             "  XcvOpen=%p XcvData=%p XcvClose=%p\n"
             "  Shutdown=%p Bidi=%p NotifyUsed=%p NotifyUnused=%p\n",
        (unsigned long)sizeof(MONITOR2), g_monitor2.cbSize, (void*)&g_monitor2,
        (void*)g_monitor2.pfnEnumPorts, (void*)g_monitor2.pfnOpenPort,
        (void*)g_monitor2.pfnStartDocPort, (void*)g_monitor2.pfnWritePort,
        (void*)g_monitor2.pfnReadPort, (void*)g_monitor2.pfnEndDocPort,
        (void*)g_monitor2.pfnClosePort,
        (void*)g_monitor2.pfnXcvOpenPort, (void*)g_monitor2.pfnXcvDataPort,
        (void*)g_monitor2.pfnXcvClosePort,
        (void*)g_monitor2.pfnShutdown, (void*)g_monitor2.pfnSendRecvBidiDataFromPort,
        (void*)g_monitor2.pfnNotifyUsedPorts, (void*)g_monitor2.pfnNotifyUnusedPorts);

    // devolve o endereço da estrutura com as funções do monitor para o Spooler
    return &g_monitor2;
}
