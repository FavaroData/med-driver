# Meddrive Printer — Referência de Arquitetura Interna

> Este documento detalha a implementação interna da DLL e do protocolo Print Monitor 2.
> Para visão geral, decisões e fluxo de instalação, veja `impressora_virtual_documentacao.md`.

---

## Interface Print Monitor 2

O Windows Spooler carrega `meddrivemon.dll` e chama `InitializePrintMonitor2`, que retorna
um ponteiro para a struct `MONITOR2` com os callbacks implementados:

```c
typedef struct {
    DWORD  cbSize;
    // portas
    BOOL  (*pfnEnumPorts)(...);
    BOOL  (*pfnOpenPort)(...);
    BOOL  (*pfnOpenPortEx)(...);
    BOOL  (*pfnStartDocPort)(...);
    BOOL  (*pfnWritePort)(...);
    BOOL  (*pfnReadPort)(...);
    BOOL  (*pfnEndDocPort)(...);
    BOOL  (*pfnClosePort)(...);
    // adicionar porta
    BOOL  (*pfnAddPortEx)(...);
    // XCV (comunicação com UI)
    BOOL  (*pfnXcvOpenPort)(...);
    DWORD (*pfnXcvDataPort)(...);
    BOOL  (*pfnXcvClosePort)(...);
    // monitor
    VOID  (*pfnShutdown)(...);
} MONITOR2;
```

### Quando cada callback é chamado

| Callback | Quando | O que a DLL faz |
|---|---|---|
| `InitializePrintMonitor2` | Spooler carrega a DLL | Preenche a struct MONITOR2, retorna ponteiro |
| `EnumPorts` | Windows lista portas disponíveis | Retorna `Meddrive Printer PORT` |
| `OpenPort` | Spooler abre a porta para um job | Aloca `PORT_CONTEXT` |
| `StartDocPort` | Início do documento | Cria arquivo `.ps` temporário em `%TEMP%`, abre handle |
| `WritePort` | Bytes de PS chegando (chamado várias vezes) | Acumula bytes no arquivo `.ps` |
| `EndDocPort` | Documento completo | Fecha o `.ps`, chama Ghostscript, deleta o `.ps` |
| `ClosePort` | Job encerrado | Libera `PORT_CONTEXT` |
| `AddPortEx` | `AddPortExW` chamado no install.ps1 | Retorna TRUE (porta já está no registry) |
| `XcvOpenPort` | UI do spooler conecta | Retorna handle válido |
| `XcvDataPort` | UI consulta propriedades da porta | Responde `MonitorUI` e `PortIsLocal` |
| `XcvClosePort` | UI desconecta | Retorna TRUE |

---

## PORT_CONTEXT

Estrutura alocada por porta durante o ciclo de vida de um job:

```c
typedef struct {
    WCHAR  outputPath[MAX_PATH];      // lido do registry em OpenPort
    WCHAR  ghostscriptPath[MAX_PATH]; // lido do registry em OpenPort
    WCHAR  tempPsFile[MAX_PATH];      // gerado em StartDocPort via GetTempFileName
    HANDLE hTempFile;                 // handle do arquivo .ps aberto em StartDocPort
} PORT_CONTEXT;
```

---

## Fluxo interno detalhado

```
OpenPort("Meddrive Printer PORT")
  → aloca PORT_CONTEXT
  → lê OutputPath e GhostscriptPath do registry

StartDocPort(hPort, pPrinterName, JobId, Level, pDocInfo)
  → GetTempPath() + GetTempFileName() → tempPsFile
  → CreateFile(tempPsFile) → hTempFile

WritePort(hPort, pBuffer, cbBuf, pcbWritten)   [chamado N vezes]
  → WriteFile(hTempFile, pBuffer, cbBuf)

EndDocPort(hPort)
  → CloseHandle(hTempFile)
  → monta cmdLine:
      gswin64c.exe -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
                   -sOutputFile="<outputPath>" "<tempPsFile>"
  → CreateProcess(cmdLine)
  → WaitForSingleObject(hProcess, INFINITE)
  → DeleteFile(tempPsFile)

ClosePort(hPort)
  → HeapFree(PORT_CONTEXT)
```

---

## Registry lido em OpenPort

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors
    \Meddrive Printer MONITOR
        Driver = "meddrivemon.dll"
        \Ports
            \Meddrive Printer PORT
                OutputPath      = "C:\...\saida.pdf"
                GhostscriptPath = "C:\Program Files\gs\...\gswin64c.exe"
```

---

## Compilação

```makefile
CC      = x86_64-w64-mingw32-gcc
TARGET  = meddrivemon.dll
CFLAGS  = -Wall -Wextra -O2 -municode
LDFLAGS = -shared -static-libgcc -lkernel32 -ladvapi32
```

O `.def` exporta apenas `InitializePrintMonitor2` — o spooler descobre os demais via a struct `MONITOR2`.

```
; monitor.def
LIBRARY meddrivemon
EXPORTS
    InitializePrintMonitor2
```
