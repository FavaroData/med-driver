# Particularidades do suporte ao Windows Vista

## Pré-requisitos obrigatórios

| Requisito | Versão mínima | Como obter |
|---|---|---|
| Windows Vista | **SP2** (build 6002) | Windows Update |
| PowerShell | **2.0** | Windows Update — KB968930 |

O instalador verifica ambos em `.onInit` e aborta com mensagem de erro se não estiverem presentes.

---

## Por que Vista SP2 e não RTM ou SP1

O UCRT (Universal C Runtime) — necessário para o Ghostscript 9.56.1 — é distribuído pela Microsoft via **KB2999226**, que suporta Vista **SP2**, 7 SP1, 8 e 8.1. Vista RTM (build 6000) e SP1 (build 6001) não recebem esse patch e o Ghostscript falharia ao iniciar com `STATUS_DLL_NOT_FOUND (0xC0000135)`.

O instalador detecta o build number via registry:
```
HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\CurrentBuildNumber
```
- Build < 6002 → aborta com mensagem pedindo SP2
- Build ≥ 6002 → prossegue

---

## PowerShell 2.0

Vista é distribuído com PowerShell 1.0 em edições Business/Enterprise/Ultimate (Home Basic e Home Premium não incluem PS por padrão). As edições Home podem não ter o PS instalado.

Os scripts `conf\*.ps1` do MedDriveManager requerem PS 2.0 (uso de `-ErrorAction`, arrays tipados e sintaxe de pipeline não disponíveis no PS 1.0).

O instalador lê:
```
HKLM\SOFTWARE\Microsoft\PowerShell\1\PowerShellEngine\PowerShellVersion
```
- Ausente ou vazio → PS não instalado → aborta
- Dígito principal == "1" (ex: "1.0") → PS 1.x → aborta
- Dígito principal >= "2" → prossegue

**Para instalar o PS 2.0 no Vista:** Windows Update → KB968930.

---

## Ghostscript e DLLs de runtime

O Vista usa a mesma build do Ghostscript 9.56.1 que o Win7 (`gs\ghostscript-win7\`). As DLLs de runtime bundladas (VC++ 140 + UCRT) também são as mesmas — elas funcionam no Vista SP2 desde que o UCRT seja instalado via KB2999226 ou as DLLs sejam copiadas localmente (o que o instalador faz via `InstallDllIfMissing`).

Não é necessária uma build separada do Ghostscript para Vista.

---

## Reutilização de artefatos do Win7

O instalador Vista (`installer/vista/setup.nsi`) não duplica arquivos — referencia diretamente os artefatos do Win7:

| Artefato | Origem |
|---|---|
| `install_helper.exe` | `installer/win7/install_helper.exe` |
| `MEDDRIVE.PPD` | `installer/win7/MEDDRIVE.PPD` |
| `conf\*.ps1` | `installer/win7/conf\*.ps1` |
| `DLL\*.dll` | `installer/win7/DLL\*.dll` |
| `MedDriveManager.exe` | `installer/win10-11/x64/Debug/MedDriveManager.exe` |

`install_helper.exe` foi compilado com `WINVER=0x0601` (Win7), mas as APIs que ele usa (`AddPrinterDriverExW`, `CopyFileW`, `RegCreateKeyExW`) existem desde o Windows XP — compatível com Vista sem recompilação.

---

## Diferenças em relação ao Win7

| Item | Win7 | Vista |
|---|---|---|
| Verificação SP | Não necessária | Exige SP2 (build ≥ 6002) |
| Verificação PS | Não necessária | Exige PS 2.0 (KB968930) |
| Ghostscript | 9.56.1 (ghostscript-win7) | Mesmo bundle |
| DLLs runtime | Bundladas | Mesmas bundladas |
| install_helper.exe | Mesmo binário | Mesmo binário |
| Arquivo gerado | `MeddrivePrinter-Win7-Setup.exe` | `MeddrivePrinter-Vista-Setup.exe` |

---

## Limitação conhecida: DriverStore no Vista

O caminho do DriverStore no Vista pode diferir ligeiramente do Win7:

```
Vista: C:\Windows\System32\DriverStore\FileRepository\ntprint.inf_amd64_neutral_<hash>\Amd64\
Win7:  C:\Windows\System32\DriverStore\FileRepository\ntprint.inf_amd64_neutral_<hash>\Amd64\
```

O `install_helper.exe` detecta o caminho dinamicamente varrendo o DriverStore por `PSCRIPT5.DLL`, portanto não é afetado por variações no hash do diretório.

---

## Compatibilidade da edição

| Edição Vista x64 | PS incluído | Observação |
|---|---|---|
| Home Basic | Não | Instalar PS 2.0 via KB968930 antes de instalar |
| Home Premium | Não | Instalar PS 2.0 via KB968930 antes de instalar |
| Business | PS 1.0 | Atualizar para PS 2.0 via KB968930 |
| Enterprise | PS 1.0 | Atualizar para PS 2.0 via KB968930 |
| Ultimate | PS 1.0 | Atualizar para PS 2.0 via KB968930 |

Edições de 32-bit (x86) não são suportadas — a DLL é 64-bit.
