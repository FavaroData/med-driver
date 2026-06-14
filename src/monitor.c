#include "monitor.h"
#include <stdio.h>
#include <stdarg.h>

static HKEY        g_hkRoot   = NULL;
static MONITOR2    g_monitor2 = {0};

// Auxiliares

// escreve uma linha formatada no log de diagnóstico
// abre, escreve e fecha o arquivo a cada chamada para garantir que nada se perca em caso de crash
// temporária — será removida quando o problema de carregamento do monitor for resolvido
static void LogDebug(const char *fmt, ...) {
    HANDLE hLog = CreateFileW(L"C:\\Windows\\Temp\\pdfmonitor_init.log",
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
static BOOL ReadConfig(PORT_CONTEXT *ctx) {

    // parametros dentro do registry
    HKEY  hKey;
    DWORD size;

    // log: registra qual chave do registry está sendo aberta
    LogDebug("ReadConfig: abrindo Ports\\Med-driver Port\n");

    // lê as configurações do registry, se nao encontrar retorna false
    LONG rc = RegOpenKeyExW(g_hkRoot, L"Ports\\" PORT_NAME, 0, KEY_READ, &hKey);
    LogDebug("ReadConfig: RegOpenKey resultado=%ld (%s)\n",
        rc, rc == ERROR_SUCCESS ? "SUCCESS" : "FALHOU");
    if (rc != ERROR_SUCCESS) return FALSE;

    // lê o outputpath e armazena em ctx->outputPath
    // futuramente incluir validação do caminho (verificar se a pasta existe, se tem permissão de escrita, etc)
    size = sizeof(ctx->outputPath);
    RegQueryValueExW(hKey, L"OutputPath", NULL, NULL, (LPBYTE)ctx->outputPath, &size);
    LogDebug("ReadConfig: OutputPath lido (len=%lu)\n", size);

    // lê o ghostscriptpath e armazena em ctx->ghostscriptPath
    // incluir a mesma coisa que o outputpath
    size = sizeof(ctx->ghostscriptPath);
    RegQueryValueExW(hKey, L"GhostscriptPath", NULL, NULL, (LPBYTE)ctx->ghostscriptPath, &size);
    LogDebug("ReadConfig: GhostscriptPath lido (len=%lu)\n", size);

    // fecha a chave do registry, retorna TRUE se ambos os caminhos foram lidos com sucesso, FALSE caso contrário
    RegCloseKey(hKey);
    BOOL ok = ctx->outputPath[0] != L'\0' && ctx->ghostscriptPath[0] != L'\0';
    LogDebug("ReadConfig: resultado final=%s\n", ok ? "OK" : "FALHOU (paths vazios)");
    return ok;
}

// Converte o arquivo PostScript gerado pelo spooler em PDF usando o Ghostscript
// executa o Ghostscript em um processo separado
// retorna TRUE se o processo do Ghostscript terminou com sucesso
// se não FALSE e mostra o erro no output debug
// fecha o processo do Ghostscript
static BOOL ConvertPsToPdf(PORT_CONTEXT *ctx) {
    // parametros para criar o processo do Ghostscript
    WCHAR            cmdLine[2048];
    STARTUPINFOW     si = {0};
    PROCESS_INFORMATION pi = {0};
    DWORD            exitCode = 1;

    si.cb = sizeof(si);

    // executa o Ghostscript com os parâmetros armazenados em ctx no terminal
    _snwprintf(cmdLine, 2048,
        L"\"%s\" -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=\"%s\" \"%s\"",
        ctx->ghostscriptPath,
        ctx->outputPath,
        ctx->tempPsFile);

    // cria o processo do Ghostscript
    // retorna TRUE ou FALSE se o processo foi criado com sucesso ou não
    // em caso de falha, mostra o erro no output debug do Windows (pode ser visualizado com ferramentas como DebugView)
    if (!CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, NULL, &si, &pi)) {
        DWORD err = GetLastError();
        WCHAR msg[256];
        _snwprintf(msg, 256, L"[pdfmonitor] CreateProcess falhou. Erro: %lu\nCmd: %s\n", err, cmdLine);
        OutputDebugStringW(msg);
        return FALSE;
    }

    // Espera o Ghostscript terminar sem limite de tempo
    // (Verificação necessária para garantir que o PDF seja gerado antes de prosseguir)
    WaitForSingleObject(pi.hProcess, INFINITE);
    // vefifica se o processo terminou com sucesso (exit code 0)
    GetExitCodeProcess(pi.hProcess, &exitCode);
    // fecha os handles do processo e thread do Ghostscript
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
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
    // log: confirma se o Spooler chama EnumPorts após InitializePrintMonitor2
    // se aparecer, o problema do AddMonitor (3007) está após EnumPorts
    // se não aparecer, o Spooler rejeitou o MONITOR2 antes de chamar qualquer função
    LogDebug("EnumPorts: Level=%lu cbBuf=%lu\n", Level, cbBuf);

    // constante com a descrição da porta, usada apenas no nível 2
    static const WCHAR PORT_DESC[] = L"Impressora Virtual PDF";
    DWORD needed;

    // se o nível pedido for diferente de 1 ou 2, retorna erro
    if (Level != 1 && Level != 2) {
        SetLastError(ERROR_INVALID_LEVEL);
        return FALSE;
    }

    // calculo de quantos bytes necessários para retornar a lista de portas se for nivel 1 ou nivel 2
    if (Level == 1)
        needed = sizeof(PORT_INFO_1W) + (DWORD)((wcslen(PORT_NAME) + 1) * sizeof(WCHAR));
    else
        needed = sizeof(PORT_INFO_2W)
               + (DWORD)((wcslen(PORT_NAME)    + 1) * sizeof(WCHAR))
               + (DWORD)((wcslen(MONITOR_NAME) + 1) * sizeof(WCHAR))
               + (DWORD)((wcslen(PORT_DESC)    + 1) * sizeof(WCHAR));
    *pcbNeeded  = needed;
    *pcReturned = 0;

    // se o buffer fornecido for muito pequeno, retorna FALSE e quantos bytes seriam necessários
    if (cbBuf < needed) {
        LogDebug("EnumPorts: buffer insuficiente (needed=%lu cbBuf=%lu)\n", needed, cbBuf);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    // preenche o buffer com as informações da porta, dependendo do nível
    // apenas nome
    if (Level == 1) {
        PORT_INFO_1W *pInfo = (PORT_INFO_1W *)pPorts;
        LPWSTR        strBuf = (LPWSTR)(pPorts + sizeof(PORT_INFO_1W));
        wcscpy(strBuf, PORT_NAME);
        pInfo->pName = strBuf;
        LogDebug("EnumPorts L1 campos: pPorts=%p cbBuf=%lu sizeof(PORT_INFO_1W)=%lu\n"
                 "  pName: offset=%lu addr=%p val='%ls'\n",
            (void*)pPorts, cbBuf, (unsigned long)sizeof(PORT_INFO_1W),
            (unsigned long)((LPBYTE)pInfo->pName - pPorts),
            (void*)pInfo->pName, pInfo->pName);
    // nome + descrição + monitor
    } else {
        PORT_INFO_2W *pInfo  = (PORT_INFO_2W *)pPorts;
        LPWSTR        strBuf = (LPWSTR)(pPorts + sizeof(PORT_INFO_2W));

        wcscpy(strBuf, PORT_NAME);
        pInfo->pPortName = strBuf;
        strBuf += wcslen(PORT_NAME) + 1;

        wcscpy(strBuf, MONITOR_NAME);
        pInfo->pMonitorName = strBuf;
        strBuf += wcslen(MONITOR_NAME) + 1;

        wcscpy(strBuf, PORT_DESC);
        pInfo->pDescription = strBuf;

        pInfo->fPortType = PORT_TYPE_WRITE;
        pInfo->Reserved  = 0;
        LogDebug("EnumPorts L2 campos: pPorts=%p cbBuf=%lu sizeof(PORT_INFO_2W)=%lu\n"
                 "  pPortName:    offset=%lu addr=%p val='%ls'\n"
                 "  pMonitorName: offset=%lu addr=%p val='%ls'\n"
                 "  pDescription: offset=%lu addr=%p val='%ls'\n"
                 "  fPortType=%lu Reserved=%lu\n",
            (void*)pPorts, cbBuf, (unsigned long)sizeof(PORT_INFO_2W),
            (unsigned long)((LPBYTE)pInfo->pPortName    - pPorts), (void*)pInfo->pPortName,    pInfo->pPortName,
            (unsigned long)((LPBYTE)pInfo->pMonitorName - pPorts), (void*)pInfo->pMonitorName, pInfo->pMonitorName,
            (unsigned long)((LPBYTE)pInfo->pDescription - pPorts), (void*)pInfo->pDescription, pInfo->pDescription,
            (unsigned long)pInfo->fPortType, (unsigned long)pInfo->Reserved);
    }

    // retorna TRUE se estiver tudo certo e que retornamos 1 porta
    LogDebug("EnumPorts: retornando 1 porta (Level=%lu)\n", Level);
    *pcReturned = 1;
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
    if (!ReadConfig(ctx)) {
        HeapFree(GetProcessHeap(), 0, ctx);
        SetLastError(ERROR_OPEN_FAILED);
        return FALSE;
    }

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
    // cast do handle para acessar as informações do job de impressão
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;
    WCHAR         tempDir[MAX_PATH];

    // cria o arquivo temporário para armazenar o conteúdo PostScript gerado pelo spooler
    GetTempPathW(MAX_PATH, tempDir);
    GetTempFileNameW(tempDir, L"pdfmon", 0, ctx->tempPsFile);

    // referencia para o monitor do spooler, que vai escrever o conteúdo PostScript nesse arquivo temporário
    ctx->hTempFile = CreateFileW(
        ctx->tempPsFile, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // retorna TRUE se o arquivo temporário foi criado com sucesso
    return ctx->hTempFile != INVALID_HANDLE_VALUE;
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
    // escreve os bytes PostScript recebidos do spooler
    // caso haja falha na escrita, retorna FALSE e mostra o erro no output debug do Windows
    if (!WriteFile(ctx->hTempFile, pBuffer, cbBuf, pcbWritten, NULL)) {
        DWORD err = GetLastError();
        WCHAR msg[128];
        _snwprintf(msg, 128, L"[pdfmonitor] WriteFile falhou. Erro: %lu\n", err);
        OutputDebugStringW(msg);
        return FALSE;
    }
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
// chama a função de conversão do arquivo ConvertPsToPdf na qual usa o Ghostscript
// deleta o arquivo temporário
static BOOL WINAPI Monitor_EndDocPort(HANDLE hPort) {
    // cast do handle para acessar as informações do job de impressão
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;
    BOOL          ok;

    // fechha o arquivo temporário onde o conteúdo PostScript foi armazenado
    // a fim de liberar o handle para o processo do Ghostscript ler o arquivo e convertê-lo para PDF
    CloseHandle(ctx->hTempFile);
    ctx->hTempFile = INVALID_HANDLE_VALUE;

    // chama a função de conversão de PS para PDF
    ok = ConvertPsToPdf(ctx);
    // deleta o arquivo temporário
    // mesmo que a conversão falhe, o arquivo é deletado, para não deixar lixo no sistema
    DeleteFileW(ctx->tempPsFile);
    // retorna o resultado da conversão do arquivo
    return ok;
}

// função chamada pelo spooler para fechar a porta de impressão
// libera a memória alocada
// caso o arquivo temporário ainda esteja aberto, fecha o handle para evitar vazamento de handle
static BOOL WINAPI Monitor_ClosePort(HANDLE hPort) {
    PORT_CONTEXT *ctx = (PORT_CONTEXT *)hPort;

    // se o arquivo temporário ainda estiver aberto, fecha o handle para evitar vazamento de handle
    if (ctx->hTempFile != INVALID_HANDLE_VALUE)
        CloseHandle(ctx->hTempFile);

    // libera a memória
    HeapFree(GetProcessHeap(), 0, ctx);
    return TRUE;
}

// Stubs XCV — interface de configuração bidirecional entre monitor e ferramentas do Windows
// O Spooler no Windows 10/11 pode exigir que esses ponteiros sejam não-NULL para validar o monitor
// Os stubs apenas sinalizam que o monitor não suporta configuração via XCV
static BOOL WINAPI Monitor_XcvOpenPort(
    HANDLE hMonitor, LPCWSTR pObject, DWORD dwGrantedAccess, PHANDLE phXcv)
{
    // porta não suporta sessão de configuração XCV
    (void)hMonitor; (void)pObject; (void)dwGrantedAccess; (void)phXcv;
    return FALSE;
}

static DWORD WINAPI Monitor_XcvDataPort(
    HANDLE hXcv, LPCWSTR pszDataName,
    PBYTE pInputData, DWORD cbInputData,
    PBYTE pOutputData, DWORD cbOutputData, PDWORD pcbOutputNeeded)
{
    // nenhum comando XCV é suportado
    (void)hXcv; (void)pszDataName; (void)pInputData; (void)cbInputData;
    (void)pOutputData; (void)cbOutputData; (void)pcbOutputNeeded;
    return ERROR_NOT_SUPPORTED;
}

static BOOL WINAPI Monitor_XcvClosePort(HANDLE hXcv)
{
    // nada a fechar
    (void)hXcv;
    return FALSE;
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
