# Decisões de Refatoração Visual – Meddrive Printer Manager

## Q1 — Title bar
**Decisão:** Title bar customizada (WS_POPUP sem WS_CAPTION).
- Altura: 72px
- Esquerda: ícone da aplicação + "Meddrive Printer Manager" (Segoe UI Semibold 18px)
- Direita: botões Minimizar / Maximizar / Fechar com hover moderno
- Drag implementado via WM_NCHITTEST retornando HTCAPTION na área da barra
- Sem WS_CAPTION, sem WS_THICKFRAME

## Q2 — Resize
**Decisão:** Tamanho fixo. Sem redimensionamento.
- Sem WS_THICKFRAME, sem hit-test de borda
- Botão Maximizar presente na title bar mas desabilitado (ou removido — ver Q3)
- Janela sempre centralizada na tela ao abrir (DS_CENTER ou cálculo manual)

## Q3 — Tamanho fixo
**Decisão:** 960×620px (pixels físicos).
- Cabe em 1280×720 com margem para taskbar (~60px) e bordas laterais (~160px)
- Proporcional para a tabela com 4 colunas e os botões de ação abaixo

## Q4 — Botão Maximizar
**Decisão:** Removido. Title bar terá apenas Minimizar e Fechar.

## Q5 — Aba Configurações
**Decisão:** Criada na navegação como placeholder `"Configurações — em breve."` (tab index 2, `g_activeTab == 2`). Conteúdo a ser implementado em entrega futura. Candidatos: caminho do Ghostscript configurável, diagnóstico do sistema, pasta padrão para novos perfis, confirmação ao excluir, painel sobre/versão.

## Q6 / Q10 — Ícones
**Decisão:** Arquivos ICO 32-bit com alpha, embutidos como recursos Win32 `ICON`.
Renderizados via `DrawIconEx` — GDI puro, sem biblioteca externa, compatível com Windows 7+.

### Por que ICO em vez de BMP + AlphaBlend
BMP 32-bit requer alpha pré-multiplicado (`RGB * alpha/255`) que o ImageMagick não gera por padrão.
Seria necessário pré-multiplicar em build-time ou em runtime.
ICO 32-bit com alpha é tratado nativamente pelo Win32 — `DrawIconEx` lida com o canal alpha automaticamente, sem nenhum passo extra.

### Pipeline de build
```
# build.sh — conversão PNG → ICO (ImageMagick, já instalado)
magick res/icons/printer_20.png res/icons/printer_20.ico
```
Adicionado ao `build.sh` para cada ícone antes da chamada ao `windres`.

### Embutir em app.rc
```rc
IDI_PRINTER_20    ICON    "icons/printer_20.ico"
IDI_SETTINGS_20   ICON    "icons/settings_20.ico"
...
```

### Carregar em runtime
```c
HICON hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_PRINTER_20),
                                 IMAGE_ICON, 20, 20, LR_DEFAULTCOLOR);
```

### Desenhar com alpha
```c
DrawIconEx(hdc, x, y, hIcon, 20, 20, 0, NULL, DI_NORMAL);
```
`DI_NORMAL` usa o canal alpha do ICO 32-bit diretamente. Funciona no Windows 7+.

### Ícones disponíveis (em `res/icons/`)
| Arquivo PNG de origem | Uso |
|---|---|
| printer_20 | Aba navegação "Impressoras", empty state |
| printer_16 | Cabeçalho coluna Impressora |
| printer_48 | Empty state (renderizado em 48×48) |
| settings_20 | Aba navegação "Configurações" |
| folder_20 | Botão browse |
| folder_16 | Cabeçalho coluna Pasta de Destino |
| document_20 | — |
| document_16 | Cabeçalho coluna Nome do Arquivo |
| plug_20 | — |
| plug_16 | Cabeçalho coluna Porta |
| add_20 | Botão Adicionar |
| delete_20 | Botão Remover |
| sync_20 | Botão Atualizar |
| info_16 | Barra de status |

## Q11 — ListView
**Decisão:** Owner-draw (`LVS_OWNERDRAWFIXED`) + subclassing do Header Control.
- Pintura customizada via `WM_DRAWITEM`
- Header subclassado com `SetWindowSubclass` para fundo `#1E293B` e texto `#94A3B8`
- Hover via `WM_MOUSEMOVE` + `WM_MOUSELEAVE` no ListView subclassado
- Seleção, scroll, teclado e clique nas colunas permanecem nativos

## Q16 — Drag da janela
**Decisão:** `WM_NCHITTEST` retorna `HTCAPTION` quando o cursor está na área da title bar (y < 72px). O Windows gerencia o drag automaticamente.

## Q14 — Dialogs dlg_add e dlg_progress
**Decisão:** Incluídos na refatoração. Dark theme aplicado via `WM_CTLCOLOREDIT`, `WM_CTLCOLORSTATIC`, `WM_CTLCOLORDLG`, `WM_CTLCOLORBTN`. Botões com owner-draw para o visual do spec.

## Q12 — Organização do código
**Decisão:** Diretório `src/ui/` com módulos exclusivamente visuais. Nenhuma regra de negócio nesses arquivos.

Módulos planejados:
| Arquivo | Responsabilidade |
|---|---|
| `ui/theme.c/.h` | Cores, fontes, ícones — constantes e handles centralizados |
| `ui/titlebar.c/.h` | Pintura e hit-test da barra superior customizada |
| `ui/navbar.c/.h` | Navegação horizontal (Impressoras / Configurações) |
| `ui/listview.c/.h` | Owner-draw do ListView e subclassing do Header |
| `ui/statusbar.c/.h` | Barra de status inferior customizada |
| `ui/buttons.c/.h` | Botões de ação (Adicionar / Remover / Atualizar) |

`mainwnd.c` permanece como orquestrador: cria janela, delega eventos para os módulos `ui/`, chama lógica de negócio existente (`store.c`, `dlg_add.c`, etc.) sem misturar com pintura.

## Q7 — Animações
**Decisão:** Nenhuma. Hover é troca de cor instantânea. Sem timers, sem fade, sem transições.

## Q8 — Sombra DWM
**Decisão:** Sim. `DwmSetWindowAttribute(DWMWA_NCRENDERING_POLICY, DWMNCRP_ENABLED)` aplicado após criação da janela.

## Q9 — Borda da janela
**Decisão:** Sem borda. A sombra DWM é suficiente para separar a janela do fundo.

## Q15 — Barra de status
**Decisão:** Custom — janela filha com `WM_PAINT` próprio.
- Fundo: `CLR_STATUSBAR_BG` (`#0B1220`)
- Borda superior: linha 2px `CLR_ACCENT` (`#3B82F6`)
- Ícone info (16px) + texto "N impressoras cadastradas"
- Atualizada via `InvalidateRect` sempre que a lista muda

## Q13 — Sistema de cores e fontes
**Decisão:** `#define` em `theme.h`.
```c
// Paleta
#define CLR_BG_PRIMARY    RGB(15,  23,  42)   // #0F172A
#define CLR_BG_SECONDARY  RGB(17,  24,  39)   // #111827
#define CLR_CARD          RGB(30,  41,  59)   // #1E293B
#define CLR_BORDER        RGB(51,  65,  85)   // #334155
#define CLR_ACCENT        RGB(59, 130, 246)   // #3B82F6
#define CLR_ACCENT_HOVER  RGB(96, 165, 250)   // #60A5FA
#define CLR_TEXT_PRIMARY  RGB(248,250,252)     // #F8FAFC
#define CLR_TEXT_SECONDARY RGB(148,163,184)   // #94A3B8
#define CLR_TEXT_DISABLED RGB(100,116,139)    // #64748B
#define CLR_ROW_HOVER     RGB(36,  50,  68)   // #243244
#define CLR_ROW_SELECTED  RGB(29,  78, 216)   // #1D4ED8
#define CLR_TEXT_ROW      RGB(226,232,240)    // #E2E8F0
#define CLR_BTN_PRIMARY   RGB(37,  99, 235)   // #2563EB
#define CLR_BTN_SECONDARY RGB(55,  65,  81)   // #374151
#define CLR_STATUSBAR_BG  RGB(11,  18,  32)   // #0B1220

// Fontes criadas uma vez em theme_init() e destruídas em theme_destroy()
// HFONT g_fontTitle;    // Segoe UI Semibold 18px
// HFONT g_fontSubtitle; // Segoe UI 14px
// HFONT g_fontContent;  // Segoe UI 13px
```
