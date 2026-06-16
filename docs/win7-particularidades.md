# Particularidades do suporte ao Windows 7

## Versão do Ghostscript

Usar obrigatoriamente **Ghostscript 9.56.1 (win64)**. Versões 10.x crasham com `STATUS_ACCESS_VIOLATION` (0xC0000005) no Win7, provavelmente por chamarem APIs introduzidas no Windows 8+.

Download: `gs9561w64.exe` na página de releases do Ghostscript no GitHub (ArtifexSoftware/ghostpdl).

---

## DLLs do sistema necessárias no Win7

O Ghostscript 9.56.1 foi compilado com o Visual C++ Runtime e a Universal CRT, que não estão presentes por padrão no Win7. Sem elas, o processo falha ao iniciar com:

- `0xC0000135` — `STATUS_DLL_NOT_FOUND` (DLL ausente)
- `0xC0000139` — `STATUS_ENTRYPOINT_NOT_FOUND` (DLL presente mas versão incompatível)

### Solução: bundlar as DLLs em `gs/ghostscript/bin/`

Copiar de `C:\Windows\System32\` de uma máquina **Windows 10 ou 11**:

**Visual C++ Runtime:**
- `VCRUNTIME140.dll`
- `VCRUNTIME140_1.dll`
- `MSVCP140.dll`

**Universal CRT (`ucrtbase` + forwarders):**
- `ucrtbase.dll`
- `api-ms-win-crt-convert-l1-1-0.dll`
- `api-ms-win-crt-environment-l1-1-0.dll`
- `api-ms-win-crt-filesystem-l1-1-0.dll`
- `api-ms-win-crt-heap-l1-1-0.dll`
- `api-ms-win-crt-locale-l1-1-0.dll`
- `api-ms-win-crt-math-l1-1-0.dll`
- `api-ms-win-crt-runtime-l1-1-0.dll`
- `api-ms-win-crt-stdio-l1-1-0.dll`
- `api-ms-win-crt-string-l1-1-0.dll`
- `api-ms-win-crt-time-l1-1-0.dll`
- `api-ms-win-crt-utility-l1-1-0.dll`

Como o `setup.nsi` inclui `File /r "..\..\gs\ghostscript\bin\*"`, essas DLLs são empacotadas automaticamente no instalador e instaladas em `C:\ProgramData\Meddrive Printer\Ghostscript\bin\`. O Windows as encontra ali antes de procurar em `System32`, garantindo compatibilidade independente do estado do sistema.

---

## Registro do driver de impressão

No Win7, escrever diretamente no registry para registrar o driver PSCRIPT5 (`HKLM:\...\Print\Environments\Windows x64\Drivers\Version-3\...`) faz o spooler **enumerar** o driver mas não consegue **carregá-lo** ao chamar `AddPrinter`, resultando em Win32 erro 6 (`ERROR_INVALID_HANDLE`).

A solução é usar `AddPrinterDriverEx` via P/Invoke com `DRIVER_INFO_2` e flags `APD_COPY_ALL_FILES | APD_COPY_FROM_DIRECTORY = 20`.

Os arquivos do PSCRIPT5 no Win7 ficam no DriverStore com caminho dinâmico:
```
C:\Windows\System32\DriverStore\FileRepository\ntprint.inf_amd64_neutral_<hash>\Amd64\
```

O script detecta esse caminho em runtime:
```powershell
$ps5 = Get-ChildItem "$env:SystemRoot\System32\DriverStore\FileRepository" `
    -Recurse -Filter "PSCRIPT5.DLL" -ErrorAction SilentlyContinue | Select-Object -First 1
$driverDir = $ps5.DirectoryName
```
