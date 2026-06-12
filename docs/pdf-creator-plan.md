# PDF Creator — Monitor de Porta Customizado

## Visão geral

Criar um monitor de porta para Windows que intercepta jobs de impressão PostScript,
converte para PDF usando Ghostscript e salva automaticamente em um path configurado.

---

## O que é um monitor de porta

O Windows Print Spooler precisa de um componente que defina o que fazer com os bytes
de um job de impressão. Em impressoras físicas esse componente envia os bytes para a
USB ou rede. No nosso caso, ele recebe o PostScript e converte para PDF.

---

## Arquitetura

```
Usuário clica em Imprimir
        ↓
Windows Print Spooler
        ↓
Microsoft PS Class Driver
transforma o documento em PostScript
        ↓
meumonitor.dll (seu monitor de porta)
recebe os bytes via WritePort()
        ↓
Ghostscript
converte PostScript → PDF
        ↓
PDF salvo em C:\PDFs\saida.pdf
```

---

## O que você cria e o que reutiliza

| Camada | Ação |
|---|---|
| Driver PostScript | reutiliza — Microsoft PS Class Driver |
| Print Processor | reutiliza — padrão do Windows |
| Monitor de porta | **cria — meumonitor.dll** |
| Registro no Windows | **cria — install.ps1** |

---

## Estrutura do projeto

```
pdf-monitor/
├── src/
│   ├── monitor.c        ← implementação do monitor de porta
│   ├── monitor.h        ← definições e structs
│   └── monitor.def      ← exports da DLL
├── installer/
│   └── install.ps1      ← registra a DLL e a impressora no Windows
└── Makefile             ← build com MinGW ou MSVC
```

---

## Funções que a DLL exporta

O Spooler chama essas funções na sua DLL:

| Função | Quando é chamada |
|---|---|
| `InitializePortMonitor` | quando o Spooler carrega a DLL |
| `EnumPorts` | quando o Windows lista as portas disponíveis |
| `OpenPort` | quando um job está chegando |
| `StartDocPort` | início do documento |
| `WritePort` | recebe os bytes do PostScript (chamada várias vezes) |
| `EndDocPort` | documento completo — aqui chama o Ghostscript |
| `ClosePort` | encerra a porta |

---

## Fluxo interno da DLL

```
OpenPort()
  → aloca buffer para receber os bytes

WritePort()
  → acumula os bytes do PostScript no buffer

EndDocPort()
  → salva o buffer em um .ps temporário
  → lê o path de saída do registry
  → chama: gswin64c.exe -dBATCH -dNOPAUSE -sDEVICE=pdfwrite
                        -sOutputFile={path} arquivo.ps
  → deleta o .ps temporário

ClosePort()
  → libera o buffer
```

---

## Configuração — Registry

O path de saída fica salvo no registry:

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\PDF Creator Monitor\Ports\PDF Creator Port
    OutputPath  = "C:\PDFs\saida.pdf"
    GhostscriptPath = "C:\Program Files\gs\gs10.02.1\bin\gswin64c.exe"
```

Para múltiplas impressoras, cada porta tem sua própria chave com seu próprio `OutputPath`.

---

## Registro do monitor no Windows

```
HKLM\SYSTEM\CurrentControlSet\Control\Print\Monitors\PDF Creator Monitor
    Driver = "pdfmonitor.dll"
```

---

## Instalação (`install.ps1`)

O script PowerShell faz tudo em sequência:

```
1. Copia pdfmonitor.dll para C:\Windows\System32\spool\
2. Registra o monitor no registry
3. Instala o driver: Add-PrinterDriver -Name "Microsoft PS Class Driver"
4. Adiciona a porta com o path configurado
5. Registra a impressora: Add-Printer -Name "PDF Creator"
                                      -DriverName "Microsoft PS Class Driver"
                                      -PortName "PDF Creator Port"
```

---

## Stack

| Peça | Tecnologia |
|---|---|
| Monitor de porta | C (Win32 API) |
| Conversão PS→PDF | Ghostscript (`gswin64c.exe`) via `CreateProcess` |
| Registro no Windows | PowerShell (`install.ps1`) |
| Compilador | MinGW (gcc) ou MSVC (Visual Studio) |
| Referência | Microsoft Docs — Print Monitor |

---

## Requisitos

- Windows 10 ou 11
- [Ghostscript para Windows](https://www.ghostscript.com/releases/gsdnld.html)
- MinGW ou Visual Studio (para compilar a DLL)
- PowerShell 5+ (já incluso no Windows 10/11)
- Executar o instalador como Administrador

---

## Ordem de implementação

- [ ] 1. Instalar MinGW ou Visual Studio
- [ ] 2. Instalar Ghostscript e testar no terminal
- [ ] 3. `monitor.h` — structs e definições
- [ ] 4. `monitor.c` — implementar as 7 funções exportadas
- [ ] 5. `monitor.def` — declarar os exports da DLL
- [ ] 6. Compilar e gerar `pdfmonitor.dll`
- [ ] 7. `install.ps1` — registrar no Windows
- [ ] 8. Teste: imprimir de qualquer app e verificar o PDF gerado
