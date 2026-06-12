# Impressora Virtual PDF
**Documentação Técnica de Decisões de Projeto**
Versão 1.0 · Junho 2025

---

## 1. Visão Geral

Impressora virtual que captura jobs de impressão do Windows e os converte automaticamente em arquivos PDF, salvando-os em uma pasta configurável.

**Objetivos:**
- Instalar como impressora nativa no Windows
- Interceptar jobs de impressão do spooler
- Converter automaticamente para PDF via Ghostscript
- Salvar na pasta configurada pelo usuário
- Não depender de software de terceiros além do Ghostscript

---

## 2. Contexto e Motivação

O projeto surgiu da necessidade de uma solução leve e controlável de impressora virtual PDF, sem depender de ferramentas como PDFCreator, CutePDF ou Redmon — projetos descontinuados ou com licenças restritivas.

### 2.1 Alternativas Avaliadas

| Alternativa | Status | Motivo |
|---|---|---|
| Redmon + Ghostscript | Descartado | Projeto abandonado, problemas de compatibilidade com Windows 10/11 |
| Microsoft Print to PDF | Descartado | Não permite configurar pasta de destino automaticamente |
| PDFCreator / CutePDF | Descartado | Dependência de instaladores de terceiros, menos controle |
| pdfmon | Descartado | Monitor de porta antigo, confiabilidade incerta em versões recentes do Windows |
| **PowerShell + Ghostscript** | **Adotado** | Controle total, sem dependências externas além do Ghostscript, compatível com Windows 10/11 |

---

## 3. Arquitetura da Solução

Fluxo completo de processamento de um job de impressão:

```
Aplicativo imprime
      ↓
Spooler do Windows
      ↓
Porta FILE: → salva .ps em pasta temporária
      ↓
Serviço Windows (PowerShell) monitora a pasta temporária
      ↓
Ghostscript converte .ps → .pdf
      ↓
Pasta de destino configurável
```

### 3.1 Componentes

| Camada | Componente | Responsabilidade |
|---|---|---|
| Driver | Generic / Text Only | Driver nativo do Windows, sem instalação adicional |
| Porta | FILE: | Redireciona output do spooler para arquivo temporário .ps |
| Monitor | Serviço PowerShell | FileSystemWatcher monitora pasta temp e aciona conversão |
| Conversor | Ghostscript (gswin64c) | Converte PostScript → PDF |
| Configuração | config.json | Define pasta destino, nome do arquivo, path do Ghostscript |

---

## 4. Decisões Técnicas

| Decisão | Escolha | Justificativa |
|---|---|---|
| Linguagem de automação | PowerShell | Nativo no Windows, sem instalar runtimes adicionais |
| Monitor de porta | Serviço Windows via PowerShell | Mais confiável que pdfmon no Win 10/11, totalmente controlável |
| Driver de impressora | Generic / Text Only | Driver nativo do Windows, sem dependência externa |
| Formato intermediário | PostScript (.ps) | Formato padrão de impressão, suportado nativamente pelo Ghostscript |
| Conversor PDF | Ghostscript (gswin64c.exe) | Ferramenta open source madura, conversão de alta qualidade |
| Pasta de saída | Configurável via config.json | Permite ao usuário definir destino sem editar scripts |
| Nome do arquivo | Fixo, configurável em config.json | Simplicidade — sem sobrescrita acidental |
| Persistência de config | config.json | Formato simples, editável em qualquer editor de texto |

---

## 5. Estrutura de Arquivos

| Arquivo | Função | Descrição |
|---|---|---|
| `config.json` | Configuração | Pasta destino, nome do arquivo, caminho do Ghostscript |
| `setup.ps1` | Instalação | Cria porta, instala driver, registra impressora, instala serviço |
| `print-to-pdf.ps1` | Serviço de conversão | FileSystemWatcher + chamada ao Ghostscript |

### 5.1 Estrutura do config.json

```json
{
  "OutputFolder":    "C:\\Impressoes",
  "FileName":        "documento.pdf",
  "GhostscriptPath": "C:\\Program Files\\gs\\gs10.00.0\\bin\\gswin64c.exe"
}
```

---

## 6. Decisões Pendentes

- [ ] Confirmar caminho exato do Ghostscript na VM Windows (`where gswin64c`)
- [ ] Definir comportamento quando arquivo de destino já existe (sobrescrever ou incrementar número?)
- [ ] Definir se o serviço inicia automaticamente com o Windows (`Automatic` vs `Manual`)
- [ ] Definir pasta temporária para os arquivos `.ps` intermediários

---

## 7. Referências

- Projeto de referência: https://github.com/TheHeadlessSourceMan/virtualPrinter
- Ghostscript: https://www.ghostscript.com
- Documentação PowerShell: `Add-Printer`, `Add-PrinterPort`, `New-Service`
- Documentação Windows: `FileSystemWatcher` Class (.NET)
