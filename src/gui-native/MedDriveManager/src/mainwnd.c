#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <winspool.h>
#include "mainwnd.h"
#include "dlg_add.h"
#include "dlg_profile.h"
#include "dlg_progress.h"
#include "store.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/titlebar.h"
#include "ui/navbar.h"
#include "ui/listview.h"
#include "ui/statusbar.h"
#include "ui/buttons.h"
#include "settings/settings_tab.h"

#define MAX_PRINTERS 512

/* ── Layout inline da aba Perfis ────────────────────────────────────── */
#define PROF_SUBLABEL_Y  (TITLEBAR_H + NAVBAR_H + SUBTITLE_H + 4)  /* "Perfil selecionado:" */
#define PROF_COMBO_Y     (PROF_SUBLABEL_Y + 16)                     /* ComboBox              */
#define PROF_BTN_Y       (PROF_COMBO_Y + 28)                        /* botões inline         */
#define PROF_PANEL_Y     (PROF_BTN_Y + BTN_H + 10)                  /* painel 3 colunas      */

/* ── Estado global ──────────────────────────────────────────────────── */
static HWND g_hwndMain;
static int  g_activeTab = 0; /* 0=Perfis  1=Impressoras  2=Configurações */

/* Impressoras */
static HWND g_hwndList;
static HWND g_hwndBtnAdd;
static HWND g_hwndBtnRemove;
static HWND g_hwndBtnEditPrinter;
static HWND g_hwndBtnRefresh;
static PrinterEntry g_printers[MAX_PRINTERS];
static int  g_count = 0;

/* Perfis */
static HWND g_hwndCombo;
static HWND g_hwndBtnNewProfile;
static HWND g_hwndBtnEditProfile;
static HWND g_hwndBtnDupProfile;
static HWND g_hwndBtnDelProfile;
static ProfileEntry g_profiles[MAX_PRINTERS];
static int  g_profileCount = 0;

/* Status bar */
static HWND g_hwndStatus;
static BOOL s_navTrackingMouse = FALSE;

/* ── Helpers de negócio — Impressoras ────────────────────────────────── */
static void list_refresh(void) {
    ListView_DeleteAllItems(g_hwndList);
    for (int i = 0; i < g_count; i++) {
        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.pszText = g_printers[i].name;
        ListView_InsertItem(g_hwndList, &lvi);
        ListView_SetItemText(g_hwndList, i, 1, g_printers[i].profileName);
    }
    statusbar_set_text(g_hwndStatus, g_count);
    InvalidateRect(g_hwndMain, NULL, FALSE);
}

static void sync_with_system(void) {
    static const WCHAR PORT_PREFIX[] = L"Meddrive Printer PORT ";
    int prefixLen = (int)wcslen(PORT_PREFIX);

    DWORD needed = 0, returned = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &returned);

    int newCount = 0;
    PrinterEntry newPrinters[MAX_PRINTERS];
    memset(newPrinters, 0, sizeof(newPrinters));

    if (needed > 0) {
        BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, needed);
        if (buf) {
            if (EnumPrintersW(PRINTER_ENUM_LOCAL, NULL, 2, buf, needed, &needed, &returned)) {
                PRINTER_INFO_2W *info = (PRINTER_INFO_2W *)buf;
                for (DWORD i = 0; i < returned && newCount < MAX_PRINTERS; i++) {
                    if (!info[i].pDriverName) continue;
                    if (_wcsicmp(info[i].pDriverName, L"Meddrive Printer DRIVER") != 0) continue;

                    PrinterEntry *e = &newPrinters[newCount];
                    wcsncpy_s(e->name, PRINTER_NAME_MAX, info[i].pPrinterName, _TRUNCATE);
                    if (info[i].pPortName) {
                        wcsncpy_s(e->portName, PRINTER_PORT_MAX, info[i].pPortName, _TRUNCATE);
                        if (wcsncmp(info[i].pPortName, PORT_PREFIX, (size_t)prefixLen) == 0)
                            wcsncpy_s(e->profileName, PRINTER_NAME_MAX,
                                      info[i].pPortName + prefixLen, _TRUNCATE);
                        else
                            wcsncpy_s(e->profileName, PRINTER_NAME_MAX,
                                      info[i].pPortName, _TRUNCATE);
                    }
                    newCount++;
                }
            }
            HeapFree(GetProcessHeap(), 0, buf);
        }
    }

    g_count = newCount;
    memcpy(g_printers, newPrinters, (size_t)newCount * sizeof(PrinterEntry));
    list_refresh();
}

static void on_add(HWND hwnd) {
    if (g_count >= MAX_PRINTERS) return;

    wchar_t dllPath[MAX_PATH];
    GetSystemDirectoryW(dllPath, MAX_PATH);
    wcsncat_s(dllPath, MAX_PATH, L"\\meddrivemon.dll", _TRUNCATE);
    if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(hwnd,
            L"meddrivemon.dll não encontrada em System32.\r\n"
            L"Execute o instalador principal antes de adicionar impressoras.",
            L"Pré-requisito ausente", MB_ICONERROR | MB_OK);
        return;
    }

    if (g_profileCount == 0) {
        MessageBoxW(hwnd,
            L"Nenhum perfil cadastrado.\r\n"
            L"Crie um perfil na aba PERFIS antes de adicionar uma impressora.",
            L"Sem perfis disponíveis", MB_ICONWARNING | MB_OK);
        return;
    }

    PrinterEntry entry = {0};
    if (!dlg_add_show(hwnd, &entry, g_profiles, g_profileCount, NULL)) return;
    if (!dlg_progress_run(hwnd, entry.name, entry.profileName)) return;
    sync_with_system();
}

static void on_remove(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_count) return;

    wchar_t confirm[512];
    _snwprintf_s(confirm, 512, _TRUNCATE,
        L"Remover a impressora \"%s\"?\r\n\r\n"
        L"A impressora, a porta e todas as configurações associadas\r\n"
        L"serão removidas do Windows.",
        g_printers[sel].name);
    if (MessageBoxW(hwnd, confirm, L"Confirmar remoção",
                    MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return;

    wchar_t name[PRINTER_NAME_MAX];
    wcsncpy_s(name, PRINTER_NAME_MAX, g_printers[sel].name, _TRUNCATE);
    if (!dlg_progress_remove(hwnd, name)) return;
    sync_with_system();
}

static void on_edit_printer(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= g_count) return;

    if (g_profileCount == 0) {
        MessageBoxW(hwnd,
            L"Nenhum perfil cadastrado.\r\n"
            L"Crie um perfil na aba PERFIS antes de editar.",
            L"Sem perfis disponíveis", MB_ICONWARNING | MB_OK);
        return;
    }

    wchar_t oldName[PRINTER_NAME_MAX];
    wcsncpy_s(oldName, PRINTER_NAME_MAX, g_printers[sel].name, _TRUNCATE);

    PrinterEntry entry = {0};
    if (!dlg_add_show(hwnd, &entry, g_profiles, g_profileCount, &g_printers[sel])) return;
    if (!dlg_progress_edit_printer(hwnd, oldName, entry.name, entry.profileName)) return;
    sync_with_system();
}

/* ── Helpers de negócio — Perfis ─────────────────────────────────────── */
static void profile_refresh(void) {
    if (!g_hwndCombo) return;
    int prev = (int)SendMessageW(g_hwndCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(g_hwndCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_profileCount; i++)
        SendMessageW(g_hwndCombo, CB_ADDSTRING, 0, (LPARAM)g_profiles[i].name);
    int n = (int)SendMessageW(g_hwndCombo, CB_GETCOUNT, 0, 0);
    if (n > 0)
        SendMessageW(g_hwndCombo, CB_SETCURSEL,
                     (prev >= 0 && prev < n) ? (WPARAM)prev : 0, 0);

    /* Statusbar: exibe contagem de perfis na aba Perfis */
    if (g_activeTab == 0 && g_hwndStatus) {
        wchar_t t[128];
        if (g_profileCount == 1)
            _snwprintf_s(t, 128, _TRUNCATE, L"1 perfil cadastrado");
        else
            _snwprintf_s(t, 128, _TRUNCATE, L"%d perfis cadastrados", g_profileCount);
        statusbar_set_text_raw(g_hwndStatus, t);
    }
    InvalidateRect(g_hwndMain, NULL, FALSE);
}

static void load_profiles(void) {
    ProfileEntry *loaded = NULL;
    int n = profile_load(&loaded);
    g_profileCount = (n > MAX_PRINTERS) ? MAX_PRINTERS : n;
    if (g_profileCount > 0)
        memcpy(g_profiles, loaded, (size_t)g_profileCount * sizeof(ProfileEntry));
    profile_free(loaded);
}

/* ── Troca de aba ────────────────────────────────────────────────────── */
static void switch_tab(int tab) {
    g_activeTab = tab;
    g_hoverRow  = -1;

    BOOL isPerfis      = (tab == 0);
    BOOL isImpressoras = (tab == 1);
    BOOL isConfig      = (tab == 2);

    ShowWindow(g_hwndCombo,          isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnNewProfile,  isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnEditProfile, isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnDupProfile,  isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnDelProfile,  isPerfis      ? SW_SHOW : SW_HIDE);

    ShowWindow(g_hwndList,           isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnAdd,          isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRemove,       isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnEditPrinter,  isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRefresh,      isImpressoras ? SW_SHOW : SW_HIDE);

    settings_tab_show(isConfig);
    if (isConfig) settings_tab_load();

    /* Statusbar: conteúdo depende da aba */
    if (g_hwndStatus) {
        if (isPerfis) {
            wchar_t t[128];
            if (g_profileCount == 1)
                _snwprintf_s(t, 128, _TRUNCATE, L"1 perfil cadastrado");
            else
                _snwprintf_s(t, 128, _TRUNCATE, L"%d perfis cadastrados", g_profileCount);
            statusbar_set_text_raw(g_hwndStatus, t);
        } else if (isConfig) {
            statusbar_set_text_raw(g_hwndStatus, L"Configurações do sistema");
        } else {
            statusbar_set_text(g_hwndStatus, g_count);
        }
    }

    InvalidateRect(g_hwndMain, NULL, TRUE);
}

/* ── Ações de perfil ─────────────────────────────────────────────────── */
static void on_new_profile(HWND hwnd) {
    ProfileEntry entry = {0};
    if (!dlg_profile_show(hwnd, &entry, NULL, NULL)) return;
    if (!dlg_progress_create_profile(hwnd,
                                     entry.name,
                                     entry.outputPath,
                                     entry.outputBaseName,
                                     (BOOL)entry.openAfterGenerate,
                                     (BOOL)entry.overwriteFile,
                                     (BOOL)entry.choosePath)) return;
    load_profiles();
    profile_refresh();
}

static void on_edit_profile(HWND hwnd) {
    int sel = (int)SendMessageW(g_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_profileCount) return;

    ProfileEntry edited = g_profiles[sel];
    if (!dlg_profile_show(hwnd, &edited, &g_profiles[sel], L"Editar Perfil")) return;

    /* Se o nome mudou, verifica impressoras vinculadas */
    if (wcscmp(edited.name, g_profiles[sel].name) != 0) {
        wchar_t linkedNames[512] = {0};
        int linkedCount = 0;
        for (int i = 0; i < g_count; i++) {
            if (_wcsicmp(g_printers[i].portName, g_profiles[sel].portName) == 0) {
                if (linkedCount > 0) wcsncat_s(linkedNames, 512, L"\r\n", _TRUNCATE);
                wcsncat_s(linkedNames, 512, L"  \x2022 ", _TRUNCATE);
                wcsncat_s(linkedNames, 512, g_printers[i].name, _TRUNCATE);
                linkedCount++;
            }
        }
        if (linkedCount > 0) {
            wchar_t msg[1024];
            _snwprintf_s(msg, 1024, _TRUNCATE,
                L"Renomear o perfil afetará as impressoras vinculadas:\r\n%s\r\n\r\n"
                L"As impressoras serão redirecionadas automaticamente. Continuar?",
                linkedNames);
            if (MessageBoxW(hwnd, msg, L"Impressoras vinculadas",
                            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
                return;
        }
    }

    if (!dlg_progress_edit_profile(hwnd,
                                   g_profiles[sel].name,
                                   edited.name,
                                   edited.outputPath,
                                   edited.outputBaseName,
                                   (BOOL)edited.openAfterGenerate,
                                   (BOOL)edited.overwriteFile,
                                   (BOOL)edited.choosePath)) return;
    load_profiles();
    profile_refresh();
    sync_with_system();
}

static void on_dup_profile(HWND hwnd) {
    int sel = (int)SendMessageW(g_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_profileCount) return;

    ProfileEntry copy = g_profiles[sel];
    _snwprintf_s(copy.name, PRINTER_NAME_MAX, _TRUNCATE,
                 L"%s - C\xF3pia", g_profiles[sel].name);
    copy.portName[0] = L'\0'; /* será derivado do novo nome no script */

    ProfileEntry result = copy;
    if (!dlg_profile_show(hwnd, &result, &copy, L"Duplicar Perfil")) return;
    if (!dlg_progress_create_profile(hwnd,
                                     result.name,
                                     result.outputPath,
                                     result.outputBaseName,
                                     (BOOL)result.openAfterGenerate,
                                     (BOOL)result.overwriteFile,
                                     (BOOL)result.choosePath)) return;
    load_profiles();
    profile_refresh();
}

static void on_del_profile(HWND hwnd) {
    int sel = (int)SendMessageW(g_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_profileCount) return;

    /* Verifica impressoras vinculadas — bloqueia se houver */
    for (int i = 0; i < g_count; i++) {
        if (_wcsicmp(g_printers[i].portName, g_profiles[sel].portName) == 0) {
            wchar_t msg[512];
            _snwprintf_s(msg, 512, _TRUNCATE,
                L"O perfil \"%s\" está vinculado à impressora \"%s\".\r\n"
                L"Remova a impressora antes de excluir o perfil.",
                g_profiles[sel].name, g_printers[i].name);
            MessageBoxW(hwnd, msg, L"Perfil em uso", MB_ICONWARNING | MB_OK);
            return;
        }
    }

    wchar_t confirm[512];
    _snwprintf_s(confirm, 512, _TRUNCATE,
        L"Excluir o perfil \"%s\"?\r\n"
        L"A porta e todas as configurações associadas serão removidas.",
        g_profiles[sel].name);
    if (MessageBoxW(hwnd, confirm, L"Confirmar exclusão",
                    MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return;

    if (!dlg_progress_remove_profile(hwnd, g_profiles[sel].name)) return;
    load_profiles();
    profile_refresh();
}

/* ── Estado vazio da aba Impressoras ─────────────────────────────────── */
static void paint_empty_state(HDC dc, RECT rcContent) {
    int cx = (rcContent.left + rcContent.right)  / 2;
    int cy = (rcContent.top  + rcContent.bottom) / 2;

    if (g_icoPrinter48) {
        DrawIconEx(dc, cx - 48, cy - 80,
                   g_icoPrinter48, 48, 48, 0, NULL, DI_NORMAL);
    }

    SetTextColor(dc, CLR_TEXT_PRIMARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
    RECT rt1 = {rcContent.left, cy - 20, rcContent.right, cy + 20};
    DrawTextW(dc, L"Nenhuma impressora cadastrada", -1, &rt1,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    SetTextColor(dc, CLR_TEXT_SECONDARY);
    of = (HFONT)SelectObject(dc, g_fontContent);
    RECT rt2 = {rcContent.left, cy + 28, rcContent.right, cy + 52};
    DrawTextW(dc, L"Clique em 'Adicionar' para cadastrar uma nova impressora.", -1, &rt2,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

/* ── Painel de perfil: tokens conhecidos ─────────────────────────────── */
static const struct { const wchar_t *tok; const wchar_t *desc; } s_tokens[] = {
    { L"{n}",         L"Contador sequencial"      },
    { L"{nn}",        L"Contador 2 dígitos"       },
    { L"{nnn}",       L"Contador 3 dígitos"       },
    { L"{data}",      L"Data atual (AAAA-MM-DD)"  },
    { L"{hora}",      L"Hora atual (HH-MM-SS)"    },
    { L"{documento}", L"Nome do documento"         },
};
#define KNOWN_TOKEN_COUNT 6

/* Gera um nome de arquivo de exemplo substituindo tokens pelos valores atuais. */
static void make_preview(const ProfileEntry *p, wchar_t *out, int cch) {
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t dateStr[16], timeStr[16];
    _snwprintf_s(dateStr, 16, _TRUNCATE, L"%04d-%02d-%02d",
                 st.wYear, st.wMonth, st.wDay);
    _snwprintf_s(timeStr, 16, _TRUNCATE, L"%02d-%02d-%02d",
                 st.wHour, st.wMinute, st.wSecond);

    const wchar_t *tmpl = p->outputBaseName;
    wchar_t buf[MAX_PATH] = {0};
    int di = 0;
    const wchar_t *q = tmpl;

    struct { const wchar_t *tok; const wchar_t *val; } subs[3] = {
        { L"{data}",      dateStr   },
        { L"{hora}",      timeStr   },
        { L"{documento}", L"Laudo"  },
    };

    while (*q && di < MAX_PATH - 1) {
        if (*q != L'{') { buf[di++] = *q++; continue; }

        BOOL matched = FALSE;
        for (int k = 0; k < 3 && !matched; k++) {
            int tlen = (int)wcslen(subs[k].tok);
            if (wcsncmp(q, subs[k].tok, (size_t)tlen) == 0) {
                for (const wchar_t *v = subs[k].val; *v && di < MAX_PATH-1; v++)
                    buf[di++] = *v;
                q += tlen; matched = TRUE;
            }
        }
        if (matched) continue;

        /* {n}, {nn}, {nnn} → contador de exemplo */
        const wchar_t *r = q + 1;
        int nc = 0;
        while (*r == L'n') { nc++; r++; }
        if (nc > 0 && *r == L'}') {
            wchar_t nStr[16];
            _snwprintf_s(nStr, 16, _TRUNCATE, L"%0*d", nc == 1 ? 1 : nc, 1);
            for (const wchar_t *v = nStr; *v && di < MAX_PATH-1; v++)
                buf[di++] = *v;
            q = r + 1; continue;
        }
        buf[di++] = *q++;
    }
    buf[di] = L'\0';
    _snwprintf_s(out, cch, _TRUNCATE, L"%s.pdf", buf);
}

/* Desenha uma linha label + valor. Retorna o próximo Y. */
/* isPill=TRUE: valor em badge preenchido (cor=valClr); FALSE: texto simples */
static int draw_prop_row(HDC dc, int x, int y, int w,
                         HICON ico,
                         const wchar_t *label, const wchar_t *val,
                         COLORREF valClr, BOOL isPill) {
    int rowH  = 34;
    int midY  = y + rowH / 2;
    int icoSz = 16;

    if (ico)
        DrawIconEx(dc, x, midY - icoSz / 2, ico, icoSz, icoSz, 0, NULL, DI_NORMAL);

    int textX = x + icoSz + 6;
    int valW  = 100;
    int valX  = x + w - valW;

    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontContent);
    SetTextColor(dc, CLR_TEXT_SECONDARY);
    RECT rlbl = {textX, y, valX - 4, y + rowH};
    DrawTextW(dc, label, -1, &rlbl,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (isPill) {
        SIZE tsz = {0};
        GetTextExtentPoint32W(dc, val, (int)wcslen(val), &tsz);
        int pw = tsz.cx + 16, ph = 20;
        int px = x + w - pw, py = midY - ph / 2;
        RECT rpill = {px, py, px + pw, py + ph};
        HBRUSH hbp = CreateSolidBrush(valClr);
        FillRect(dc, &rpill, hbp); DeleteObject(hbp);
        SelectObject(dc, g_fontSmall);
        SetTextColor(dc, RGB(255, 255, 255));
        DrawTextW(dc, val, -1, &rpill, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(dc, valClr);
        RECT rval = {valX, y, x + w, y + rowH};
        DrawTextW(dc, val, -1, &rval,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(dc, of);

    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, x, y + rowH, NULL);
    LineTo(dc, x + w, y + rowH);
    SelectObject(dc, op); DeleteObject(hp);

    return y + rowH + 1;
}

/* Linha de caminho/texto longo: label (cima, cinza) + valor (baixo, colorido) */
static int draw_prop_path_row(HDC dc, int x, int y, int w,
                               HICON ico, const wchar_t *label, const wchar_t *val,
                               COLORREF valClr) {
    int lnH = 17, icoSz = 16, rowH = lnH * 2 + 4;
    int indent = icoSz + 6;

    if (ico)
        DrawIconEx(dc, x, y + 3, ico, icoSz, icoSz, 0, NULL, DI_NORMAL);

    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
    SetTextColor(dc, CLR_TEXT_SECONDARY);
    RECT rlbl = {x + indent, y, x + w, y + lnH};
    DrawTextW(dc, label, -1, &rlbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, g_fontContent);
    SetTextColor(dc, valClr);
    RECT rval = {x + indent, y + lnH + 2, x + w, y + rowH};
    DrawTextW(dc, val, -1, &rval, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS);
    SelectObject(dc, of);

    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, x, y + rowH, NULL); LineTo(dc, x + w, y + rowH);
    SelectObject(dc, op); DeleteObject(hp);
    return y + rowH + 1;
}

static void draw_col_title(HDC dc, int x, int y, int w, const wchar_t *title, int *cy_out) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, CLR_ACCENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
    RECT rt = {x, y, x + w, y + 18};
    DrawTextW(dc, title, -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
    int cy = y + 22;

    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, x, cy, NULL); LineTo(dc, x + w, cy);
    SelectObject(dc, op); DeleteObject(hp);
    *cy_out = cy + 8;
}

/* Coluna 1 — Detalhes do perfil */
static void paint_col1(HDC dc, int x, int y, int w, int panH) {
    (void)panH;
    int sel = (int)SendMessageW(g_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_profileCount) return;
    ProfileEntry *p = &g_profiles[sel];

    int pad = 12;
    int cx = x + pad, cw = w - pad * 2;
    int cy;
    draw_col_title(dc, cx, y + 8, cw, L"DETALHES DO PERFIL", &cy);

    cy = draw_prop_row(dc, cx, cy, cw, g_icoSync20,
        L"Caso o arquivo já exista",
        p->overwriteFile ? L"Sobrescrever" : L"Incrementar",
        p->overwriteFile ? CLR_RED : CLR_GREEN, TRUE);

    cy = draw_prop_row(dc, cx, cy, cw, g_icoSettings20,
        L"Salvar automaticamente", L"Sim", CLR_GREEN, FALSE);

    cy = draw_prop_row(dc, cx, cy, cw, g_icoDocument16,
        L"Abrir após gerar",
        p->openAfterGenerate ? L"Sim" : L"Não",
        p->openAfterGenerate ? CLR_GREEN : CLR_RED, FALSE);

    cy = draw_prop_row(dc, cx, cy, cw, g_icoDelete20,
        L"Sobrescrever arquivo",
        p->overwriteFile ? L"Sim" : L"Não",
        p->overwriteFile ? CLR_GREEN : CLR_RED, FALSE);

    cy += 8;

    cy = draw_prop_path_row(dc, cx, cy, cw, g_icoFolder16,
        L"Pasta de destino", p->outputPath, CLR_ACCENT);

    draw_prop_path_row(dc, cx, cy, cw, g_icoDocument16,
        L"Nome do arquivo", p->outputBaseName, CLR_ACCENT);
}

/* Coluna 2 — Variáveis utilizadas + preview do nome gerado */
static void paint_col_vars(HDC dc, int x, int y, int w, int panH) {
    (void)panH;
    int sel = (int)SendMessageW(g_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_profileCount) return;
    ProfileEntry *p = &g_profiles[sel];

    int pad = 12;
    int cx = x + pad, cw = w - pad * 2;
    int cy;
    draw_col_title(dc, cx, y + 8, cw, L"VARIÁVEIS UTILIZADAS", &cy);

    /* Badges das variáveis usadas no padrão */
    for (int i = 0; i < KNOWN_TOKEN_COUNT; i++) {
        if (wcsstr(p->outputBaseName, s_tokens[i].tok) == NULL) continue;

        RECT badge = {cx, cy, cx + cw, cy + 36};
        HBRUSH hbBadge = CreateSolidBrush(CLR_ACCENT_LIGHT);
        FillRect(dc, &badge, hbBadge); DeleteObject(hbBadge);
        HBRUSH hbBord = CreateSolidBrush(CLR_BORDER);
        FrameRect(dc, &badge, hbBord); DeleteObject(hbBord);

        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontContent);
        SetTextColor(dc, CLR_ACCENT);
        RECT rtok = {cx + 8, cy + 2, cx + cw - 4, cy + 18};
        DrawTextW(dc, s_tokens[i].tok, -1, &rtok, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, g_fontSmall);
        SetTextColor(dc, CLR_TEXT_SECONDARY);
        RECT rdesc = {cx + 8, cy + 18, cx + cw - 4, cy + 34};
        DrawTextW(dc, s_tokens[i].desc, -1, &rdesc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
        cy += 40;
    }

    /* Separador */
    cy += 6;
    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, cx, cy, NULL); LineTo(dc, cx + cw, cy);
    SelectObject(dc, op); DeleteObject(hp);
    cy += 8;

    /* Exemplo do nome gerado */
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t caption[64];
    _snwprintf_s(caption, 64, _TRUNCATE, L"Exemplo do nome gerado (hoje às %02d:%02d):",
                 st.wHour, st.wMinute);

    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
    SetTextColor(dc, CLR_TEXT_SECONDARY);
    RECT rcap = {cx, cy, cx + cw, cy + 14};
    DrawTextW(dc, caption, -1, &rcap, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    cy += 18;

    wchar_t preview[MAX_PATH];
    make_preview(p, preview, MAX_PATH);

    SelectObject(dc, g_fontContent);
    SetTextColor(dc, CLR_GREEN);
    RECT rfn = {cx, cy, cx + cw, cy + 20};
    DrawTextW(dc, preview, -1, &rfn,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, of);
}

/* Painel de 2 colunas: detalhes | variáveis + preview */
static void paint_profile_panel(HDC dc, int clientW) {
    int panX = CONTENT_PAD;
    int panY = PROF_PANEL_Y;
    int panW = clientW - CONTENT_PAD * 2;
    int panH = WIN_H - STATUSBAR_H - 6 - panY;

    int colGap = 8;
    int col1W  = panW * 50 / 100;
    int col2W  = panW - col1W - colGap;
    int col2X  = panX + col1W + colGap;

    /* Fundo e borda do card */
    RECT rcPanel = {panX, panY, panX + panW, panY + panH};
    FillRect(dc, &rcPanel, g_hbrCard);
    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    FrameRect(dc, &rcPanel, hbrd);
    DeleteObject(hbrd);

    /* Separador vertical */
    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, col2X - 1, panY + 8, NULL); LineTo(dc, col2X - 1, panY + panH - 8);
    SelectObject(dc, op); DeleteObject(hp);

    paint_col1(dc,    panX, panY, col1W, panH);
    paint_col_vars(dc, col2X, panY, col2W, panH);
}

/* ── WM_CREATE ───────────────────────────────────────────────────────── */
static void on_create(HWND hwnd) {
    g_hwndMain = hwnd;
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    theme_init(hInst);

    SendMessageW(hwnd, WM_SETICON, ICON_BIG,
        (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICO_APP),
                           IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
        (LPARAM)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_ICO_APP),
                           IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

    titlebar_create_buttons(hwnd, hInst);

    int lvX = CONTENT_PAD;
    int lvW = WIN_W - CONTENT_PAD * 2;
    int printerBtnY = WIN_H - STATUSBAR_H - BTNBAR_H + (BTNBAR_H - BTN_H) / 2;

    /* ── ComboBox de seleção de perfil (tab 0) ──────────────────────── */
    g_hwndCombo = CreateWindowExW(0, WC_COMBOBOX, NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
        CONTENT_PAD, PROF_COMBO_Y,
        WIN_W - CONTENT_PAD * 2, 200,
        hwnd, (HMENU)(UINT_PTR)IDC_COMBO_PROFILE_SEL, hInst, NULL);
    SendMessageW(g_hwndCombo, WM_SETFONT, (WPARAM)g_fontContent, TRUE);

    /* ── Botões inline de perfil (tab 0) ────────────────────────────── */
    g_hwndBtnNewProfile = buttons_create(hwnd, hInst, IDC_BTN_NEW_PROFILE,
                                          L"+ Novo Perfil", BTN_STYLE_PRIMARY,
                                          CONTENT_PAD, PROF_BTN_Y, BTN_W, BTN_H);
    g_hwndBtnEditProfile = buttons_create(hwnd, hInst, IDC_BTN_EDIT_PROFILE,
                                           L"Editar", BTN_STYLE_SECONDARY,
                                           CONTENT_PAD + BTN_W + 6, PROF_BTN_Y, BTN_W, BTN_H);
    g_hwndBtnDupProfile = buttons_create(hwnd, hInst, IDC_BTN_DUP_PROFILE,
                                          L"Duplicar", BTN_STYLE_SECONDARY,
                                          CONTENT_PAD + (BTN_W + 6) * 2, PROF_BTN_Y, BTN_W, BTN_H);
    g_hwndBtnDelProfile = buttons_create(hwnd, hInst, IDC_BTN_DEL_PROFILE,
                                          L"Excluir", BTN_STYLE_SECONDARY,
                                          CONTENT_PAD + (BTN_W + 6) * 3, PROF_BTN_Y, BTN_W, BTN_H);

    /* ── ListView de impressoras (tab 1, começa oculto) ─────────────── */
    int lvY = TITLEBAR_H + NAVBAR_H + SUBTITLE_H + 5;
    int lvH = WIN_H - lvY - BTNBAR_H - STATUSBAR_H - CONTENT_PAD;
    g_hwndList = listview_create(hwnd, hInst, lvX, lvY, lvW, lvH);

    g_hwndBtnAdd = buttons_create(hwnd, hInst, IDC_BTN_ADD,
                                   L"Adicionar", BTN_STYLE_PRIMARY,
                                   CONTENT_PAD, printerBtnY, BTN_W, BTN_H);
    g_hwndBtnRemove = buttons_create(hwnd, hInst, IDC_BTN_REMOVE,
                                      L"Remover", BTN_STYLE_SECONDARY,
                                      CONTENT_PAD + BTN_W + 8, printerBtnY, BTN_W, BTN_H);
    g_hwndBtnEditPrinter = buttons_create(hwnd, hInst, IDC_BTN_EDIT_PRINTER,
                                           L"Editar", BTN_STYLE_SECONDARY,
                                           CONTENT_PAD + (BTN_W + 8) * 2, printerBtnY, BTN_W, BTN_H);
    g_hwndBtnRefresh = buttons_create(hwnd, hInst, IDC_BTN_REFRESH,
                                       L"Atualizar", BTN_STYLE_SECONDARY,
                                       CONTENT_PAD + (BTN_W + 8) * 3, printerBtnY, BTN_W, BTN_H);

    settings_tab_create(hwnd, hInst);

    /* Status bar */
    g_hwndStatus = statusbar_create(hwnd, hInst);
    statusbar_resize(g_hwndStatus, WIN_W, WIN_H);

    /* Carrega dados */
    sync_with_system();

    load_profiles();
    profile_refresh();

    /* Aplica visibilidade inicial (tab 0 = Perfis) */
    switch_tab(0);
}

/* ── WM_PAINT ────────────────────────────────────────────────────────── */
static void on_paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int w = rc.right;

    titlebar_paint(dc, w);
    navbar_paint(dc, w, g_activeTab);

    int contentTop = TITLEBAR_H + NAVBAR_H;
    int contentBot  = (g_activeTab == 0)
                    ? WIN_H - STATUSBAR_H
                    : WIN_H - STATUSBAR_H - BTNBAR_H;
    RECT rcContent = {0, contentTop, w, contentBot};
    FillRect(dc, &rcContent, g_hbrPrimary);

    /* Cabeçalho de seção */
    if (g_activeTab == 0 || g_activeTab == 1 || g_activeTab == 2) {
        const wchar_t *sub = (g_activeTab == 0) ? L"PERFIS DE IMPRESSÃO"
                           : (g_activeTab == 1) ? L"IMPRESSORAS CADASTRADAS"
                                                : L"CONFIGURAÇÕES";
        SetTextColor(dc, CLR_ACCENT);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
        RECT rcSub = {CONTENT_PAD, contentTop + 6,
                      w - CONTENT_PAD, contentTop + SUBTITLE_H};
        DrawTextW(dc, sub, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }

    /* Aba Perfis: label + painel */
    if (g_activeTab == 0) {
        SetTextColor(dc, CLR_TEXT_SECONDARY);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
        RECT rcLbl = {CONTENT_PAD, PROF_SUBLABEL_Y,
                      w - CONTENT_PAD, PROF_COMBO_Y};
        DrawTextW(dc, L"Perfil selecionado:", -1, &rcLbl,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);

        if (g_profileCount > 0)
            paint_profile_panel(dc, w);
        else {
            SetTextColor(dc, CLR_TEXT_DISABLED);
            SetBkMode(dc, TRANSPARENT);
            of = (HFONT)SelectObject(dc, g_fontSubtitle);
            RECT rcEmp = {0, PROF_PANEL_Y, w, WIN_H - STATUSBAR_H};
            DrawTextW(dc, L"Nenhum perfil cadastrado. Clique em '+ Novo Perfil' para começar.",
                      -1, &rcEmp, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, of);
        }
    }

    /* Aba Impressoras: estado vazio */
    if (g_activeTab == 1 && g_count == 0) {
        RECT rcEmp = {CONTENT_PAD, contentTop + CONTENT_PAD,
                      w - CONTENT_PAD, contentBot - CONTENT_PAD};
        paint_empty_state(dc, rcEmp);
    }

    /* Aba Configurações */
    if (g_activeTab == 2)
        settings_tab_paint(dc);

    /* Barra de botões (só Impressoras / Configurações) */
    if (g_activeTab != 0) {
        int btnBarTop = WIN_H - STATUSBAR_H - BTNBAR_H;
        RECT rcBtnBar = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
        FillRect(dc, &rcBtnBar, g_hbrSecondary);
        HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
        RECT rcSep = {0, btnBarTop, w, btnBarTop + 1};
        FillRect(dc, &rcSep, hbrd);
        DeleteObject(hbrd);
    }

    /* Borda da janela (1px) */
    HBRUSH hbWnd = CreateSolidBrush(CLR_BORDER);
    FrameRect(dc, &rc, hbWnd);
    DeleteObject(hbWnd);

    EndPaint(hwnd, &ps);
}

/* ── WndProc ─────────────────────────────────────────────────────────── */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        on_create(hwnd);
        return 0;

    case WM_PAINT:
        on_paint(hwnd);
        return 0;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, g_hbrPrimary);
        return 1;
    }

    case WM_NCHITTEST: {
        LRESULT def = DefWindowProcW(hwnd, msg, wp, lp);
        if (def == HTCLIENT) {
            LRESULT tb = titlebar_nchittest(hwnd,
                                            (int)(short)LOWORD(lp),
                                            (int)(short)HIWORD(lp));
            if (tb != HTCLIENT) return tb;
        }
        return def;
    }

    case WM_LBUTTONDOWN: {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        int tab = navbar_hittest(x, y);
        if (tab >= 0 && tab != g_activeTab)
            switch_tab(tab);
        return 0;
    }

    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lp;
        if (mis->CtlID == IDC_PRINTER_LIST) {
            listview_measure(mis);
            return TRUE;
        }
        return FALSE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (settings_tab_drawitem(dis)) return TRUE;
        if (dis->CtlID == IDC_PRINTER_LIST) {
            listview_draw_item(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_BTN_TITLEMIN || dis->CtlID == IDC_BTN_TITLECLOSE) {
            titlebar_draw_button(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_BTN_ADD || dis->CtlID == IDC_BTN_NEW_PROFILE)
            return buttons_draw(dis, BTN_STYLE_PRIMARY);
        if (dis->CtlID == IDC_BTN_REMOVE       || dis->CtlID == IDC_BTN_REFRESH      ||
            dis->CtlID == IDC_BTN_EDIT_PRINTER ||
            dis->CtlID == IDC_BTN_EDIT_PROFILE ||
            dis->CtlID == IDC_BTN_DUP_PROFILE  ||
            dis->CtlID == IDC_BTN_DEL_PROFILE)
            return buttons_draw(dis, BTN_STYLE_SECONDARY);
        return FALSE;
    }

    case WM_CTLCOLORSTATIC: {
        LRESULT br = settings_tab_ctlcolor((HWND)lp, (HDC)wp);
        if (br) return br;
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_COMBO_PROFILE_SEL && HIWORD(wp) == CBN_SELCHANGE) {
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (settings_tab_command(LOWORD(wp))) return 0;
        switch (LOWORD(wp)) {
        case IDC_BTN_ADD:              on_add(hwnd);             break;
        case IDC_BTN_REMOVE:           on_remove(hwnd);          break;
        case IDC_BTN_EDIT_PRINTER:     on_edit_printer(hwnd);    break;
        case IDC_BTN_REFRESH:          sync_with_system();       break;
        case IDC_BTN_TITLEMIN:      ShowWindow(hwnd, SW_MINIMIZE); break;
        case IDC_BTN_TITLECLOSE:    DestroyWindow(hwnd); break;
        case IDC_BTN_NEW_PROFILE:   on_new_profile(hwnd);          break;
        case IDC_BTN_EDIT_PROFILE:  on_edit_profile(hwnd); break;
        case IDC_BTN_DUP_PROFILE:   on_dup_profile(hwnd);  break;
        case IDC_BTN_DEL_PROFILE:   on_del_profile(hwnd);  break;
        }
        return 0;

    case WM_MOUSEMOVE: {
        int x = (int)(short)LOWORD(lp);
        int y = (int)(short)HIWORD(lp);
        if (navbar_set_hover(navbar_hittest(x, y))) {
            RECT rcNav = {0, TITLEBAR_H, WIN_W, TITLEBAR_H + NAVBAR_H};
            InvalidateRect(hwnd, &rcNav, FALSE);
        }
        if (!s_navTrackingMouse) {
            TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            s_navTrackingMouse = TRUE;
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        s_navTrackingMouse = FALSE;
        if (navbar_set_hover(-1)) {
            RECT rcNav = {0, TITLEBAR_H, WIN_W, TITLEBAR_H + NAVBAR_H};
            InvalidateRect(hwnd, &rcNav, FALSE);
        }
        return 0;

    case WM_DESTROY:
        theme_destroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ── Registro e criação ──────────────────────────────────────────────── */
BOOL mainwnd_register(HINSTANCE hInst) {
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WC_MAINWND;
    return RegisterClassExW(&wc) != 0;
}

HWND mainwnd_create(HINSTANCE hInst) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x  = (sw - WIN_W) / 2;
    int y  = (sh - WIN_H) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        WC_MAINWND,
        L"Meddrive Printer Manager",
        WS_POPUP | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x, y, WIN_W, WIN_H,
        NULL, NULL, hInst, NULL);

    if (!hwnd) return NULL;

    DWORD policy = DWMNCRP_ENABLED;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY,
                          &policy, sizeof(policy));

    MARGINS m = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &m);

    return hwnd;
}
