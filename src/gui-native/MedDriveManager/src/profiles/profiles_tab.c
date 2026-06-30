#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "profiles_tab.h"
#include "mainwnd.h"
#include "store.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/statusbar.h"
#include "ui/buttons.h"
#include "dialogs/dlg_profile.h"
#include "dialogs/dlg_progress.h"

#define MAX_PROFILES 512

#define PROF_SUBLABEL_Y  (TITLEBAR_H + NAVBAR_H + SUBTITLE_H + 4)
#define PROF_COMBO_Y     (PROF_SUBLABEL_Y + 16)
#define PROF_BTN_Y       (PROF_COMBO_Y + 28)
#define PROF_PANEL_Y     (PROF_BTN_Y + BTN_H + 10)

static HWND s_hwndParent;
static HWND s_hwndStatus;
static HWND s_hwndCombo;
static HWND s_hwndBtnNew;
static HWND s_hwndBtnEdit;
static HWND s_hwndBtnDup;
static HWND s_hwndBtnDel;

static ProfileEntry  s_profiles[MAX_PROFILES];
static int           s_count = 0;

static PrinterEntry  s_printers[MAX_PROFILES];
static int           s_printerCount = 0;

static const struct { const wchar_t *tok; const wchar_t *desc; } s_tokens[] = {
    { L"{n}",         L"Contador sequencial"      },
    { L"{nn}",        L"Contador 2 dígitos"       },
    { L"{nnn}",       L"Contador 3 dígitos"       },
    { L"{data}",      L"Data atual (AAAA-MM-DD)"  },
    { L"{hora}",      L"Hora atual (HH-MM-SS)"    },
    { L"{documento}", L"Nome do documento"         },
};
#define KNOWN_TOKEN_COUNT 6

/* ── Helpers privados ───────────────────────────────────────────────────── */

static void s_load(void) {
    ProfileEntry *loaded = NULL;
    int n = profile_load(&loaded);
    s_count = (n > MAX_PROFILES) ? MAX_PROFILES : n;
    if (s_count > 0)
        memcpy(s_profiles, loaded, (size_t)s_count * sizeof(ProfileEntry));
    profile_free(loaded);
}

static void profile_refresh(void) {
    if (!s_hwndCombo) return;
    int prev = (int)SendMessageW(s_hwndCombo, CB_GETCURSEL, 0, 0);
    SendMessageW(s_hwndCombo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < s_count; i++)
        SendMessageW(s_hwndCombo, CB_ADDSTRING, 0, (LPARAM)s_profiles[i].name);
    int n = (int)SendMessageW(s_hwndCombo, CB_GETCOUNT, 0, 0);
    if (n > 0)
        SendMessageW(s_hwndCombo, CB_SETCURSEL,
                     (prev >= 0 && prev < n) ? (WPARAM)prev : 0, 0);

    if (s_hwndStatus) {
        wchar_t t[128];
        if (s_count == 1)
            _snwprintf_s(t, 128, _TRUNCATE, L"1 perfil cadastrado");
        else
            _snwprintf_s(t, 128, _TRUNCATE, L"%d perfis cadastrados", s_count);
        statusbar_set_text_raw(s_hwndStatus, t);
    }
    InvalidateRect(s_hwndParent, NULL, FALSE);
}

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
        { L"{data}",      dateStr  },
        { L"{hora}",      timeStr  },
        { L"{documento}", L"Laudo" },
    };

    while (*q && di < MAX_PATH - 1) {
        if (*q != L'{') { buf[di++] = *q++; continue; }

        BOOL matched = FALSE;
        for (int k = 0; k < 3 && !matched; k++) {
            int tlen = (int)wcslen(subs[k].tok);
            if (wcsncmp(q, subs[k].tok, (size_t)tlen) == 0) {
                for (const wchar_t *v = subs[k].val; *v && di < MAX_PATH - 1; v++)
                    buf[di++] = *v;
                q += tlen; matched = TRUE;
            }
        }
        if (matched) continue;

        const wchar_t *r = q + 1;
        int nc = 0;
        while (*r == L'n') { nc++; r++; }
        if (nc > 0 && *r == L'}') {
            wchar_t nStr[16];
            _snwprintf_s(nStr, 16, _TRUNCATE, L"%0*d", nc == 1 ? 1 : nc, 1);
            for (const wchar_t *v = nStr; *v && di < MAX_PATH - 1; v++)
                buf[di++] = *v;
            q = r + 1; continue;
        }
        buf[di++] = *q++;
    }
    buf[di] = L'\0';
    _snwprintf_s(out, cch, _TRUNCATE, L"%s.pdf", buf);
}

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

static void draw_col_title(HDC dc, int x, int y, int w,
                            const wchar_t *title, int *cy_out) {
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

static void paint_col1(HDC dc, int x, int y, int w, int panH) {
    (void)panH;
    int sel = (int)SendMessageW(s_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= s_count) return;
    ProfileEntry *p = &s_profiles[sel];

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

static void paint_col_vars(HDC dc, int x, int y, int w, int panH) {
    (void)panH;
    int sel = (int)SendMessageW(s_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= s_count) return;
    ProfileEntry *p = &s_profiles[sel];

    int pad = 12;
    int cx = x + pad, cw = w - pad * 2;
    int cy;
    draw_col_title(dc, cx, y + 8, cw, L"VARIÁVEIS UTILIZADAS", &cy);

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

    cy += 6;
    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, cx, cy, NULL); LineTo(dc, cx + cw, cy);
    SelectObject(dc, op); DeleteObject(hp);
    cy += 8;

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

static void paint_profile_panel(HDC dc, int clientW) {
    int panX = CONTENT_PAD;
    int panY = PROF_PANEL_Y;
    int panW = clientW - CONTENT_PAD * 2;
    int panH = WIN_H - STATUSBAR_H - 6 - panY;

    int colGap = 8;
    int col1W  = panW * 50 / 100;
    int col2W  = panW - col1W - colGap;
    int col2X  = panX + col1W + colGap;

    RECT rcPanel = {panX, panY, panX + panW, panY + panH};
    FillRect(dc, &rcPanel, g_hbrCard);
    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    FrameRect(dc, &rcPanel, hbrd);
    DeleteObject(hbrd);

    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, col2X - 1, panY + 8, NULL); LineTo(dc, col2X - 1, panY + panH - 8);
    SelectObject(dc, op); DeleteObject(hp);

    paint_col1(dc,     panX,  panY, col1W, panH);
    paint_col_vars(dc, col2X, panY, col2W, panH);
}

/* ── Ações ──────────────────────────────────────────────────────────────── */

static void on_new_profile(void) {
    ProfileEntry entry = {0};
    if (!dlg_profile_show(s_hwndParent, &entry, NULL, NULL)) return;
    if (!dlg_progress_create_profile(s_hwndParent,
                                     entry.name, entry.outputPath,
                                     entry.outputBaseName,
                                     (BOOL)entry.openAfterGenerate,
                                     (BOOL)entry.overwriteFile,
                                     (BOOL)entry.choosePath)) return;
    s_load();
    profile_refresh();
    PostMessageW(s_hwndParent, WM_APP_PROFILES_CHANGED, 0, 0);
}

static void on_edit_profile(void) {
    int sel = (int)SendMessageW(s_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= s_count) return;

    ProfileEntry edited = s_profiles[sel];
    if (!dlg_profile_show(s_hwndParent, &edited, &s_profiles[sel], L"Editar Perfil")) return;

    if (wcscmp(edited.name, s_profiles[sel].name) != 0) {
        wchar_t linkedNames[512] = {0};
        int linkedCount = 0;
        for (int i = 0; i < s_printerCount; i++) {
            if (_wcsicmp(s_printers[i].portName, s_profiles[sel].portName) == 0) {
                if (linkedCount > 0) wcsncat_s(linkedNames, 512, L"\r\n", _TRUNCATE);
                wcsncat_s(linkedNames, 512, L"  \x2022 ", _TRUNCATE);
                wcsncat_s(linkedNames, 512, s_printers[i].name, _TRUNCATE);
                linkedCount++;
            }
        }
        if (linkedCount > 0) {
            wchar_t msg[1024];
            _snwprintf_s(msg, 1024, _TRUNCATE,
                L"Renomear o perfil afetará as impressoras vinculadas:\r\n%s\r\n\r\n"
                L"As impressoras serão redirecionadas automaticamente. Continuar?",
                linkedNames);
            if (MessageBoxW(s_hwndParent, msg, L"Impressoras vinculadas",
                            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
                return;
        }
    }

    if (!dlg_progress_edit_profile(s_hwndParent,
                                   s_profiles[sel].name, edited.name,
                                   edited.outputPath, edited.outputBaseName,
                                   (BOOL)edited.openAfterGenerate,
                                   (BOOL)edited.overwriteFile,
                                   (BOOL)edited.choosePath)) return;
    s_load();
    profile_refresh();
    PostMessageW(s_hwndParent, WM_APP_PROFILES_CHANGED, 0, 0);
}

static void on_dup_profile(void) {
    int sel = (int)SendMessageW(s_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= s_count) return;

    ProfileEntry copy = s_profiles[sel];
    _snwprintf_s(copy.name, PRINTER_NAME_MAX, _TRUNCATE,
                 L"%s - C\xF3pia", s_profiles[sel].name);
    copy.portName[0] = L'\0';

    ProfileEntry result = copy;
    if (!dlg_profile_show(s_hwndParent, &result, &copy, L"Duplicar Perfil")) return;
    if (!dlg_progress_create_profile(s_hwndParent,
                                     result.name, result.outputPath,
                                     result.outputBaseName,
                                     (BOOL)result.openAfterGenerate,
                                     (BOOL)result.overwriteFile,
                                     (BOOL)result.choosePath)) return;
    s_load();
    profile_refresh();
    PostMessageW(s_hwndParent, WM_APP_PROFILES_CHANGED, 0, 0);
}

static void on_del_profile(void) {
    int sel = (int)SendMessageW(s_hwndCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= s_count) return;

    for (int i = 0; i < s_printerCount; i++) {
        if (_wcsicmp(s_printers[i].portName, s_profiles[sel].portName) == 0) {
            wchar_t msg[512];
            _snwprintf_s(msg, 512, _TRUNCATE,
                L"O perfil \"%s\" está vinculado à impressora \"%s\".\r\n"
                L"Remova a impressora antes de excluir o perfil.",
                s_profiles[sel].name, s_printers[i].name);
            MessageBoxW(s_hwndParent, msg, L"Perfil em uso", MB_ICONWARNING | MB_OK);
            return;
        }
    }

    wchar_t confirm[512];
    _snwprintf_s(confirm, 512, _TRUNCATE,
        L"Excluir o perfil \"%s\"?\r\n"
        L"A porta e todas as configurações associadas serão removidas.",
        s_profiles[sel].name);
    if (MessageBoxW(s_hwndParent, confirm, L"Confirmar exclusão",
                    MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return;

    if (!dlg_progress_remove_profile(s_hwndParent, s_profiles[sel].name)) return;
    s_load();
    profile_refresh();
    PostMessageW(s_hwndParent, WM_APP_PROFILES_CHANGED, 0, 0);
}

/* ── API pública ─────────────────────────────────────────────────────────── */

void profiles_tab_create(HWND parent, HINSTANCE hInst, HWND hwndStatus) {
    s_hwndParent = parent;
    s_hwndStatus = hwndStatus;

    s_hwndCombo = CreateWindowExW(0, WC_COMBOBOX, NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL,
        CONTENT_PAD, PROF_COMBO_Y,
        WIN_W - CONTENT_PAD * 2, 200,
        parent, (HMENU)(UINT_PTR)IDC_COMBO_PROFILE_SEL, hInst, NULL);
    SendMessageW(s_hwndCombo, WM_SETFONT, (WPARAM)g_fontContent, TRUE);

    s_hwndBtnNew  = buttons_create(parent, hInst, IDC_BTN_NEW_PROFILE,
                                   L"+ Novo Perfil", BTN_STYLE_PRIMARY,
                                   CONTENT_PAD, PROF_BTN_Y, BTN_W, BTN_H);
    s_hwndBtnEdit = buttons_create(parent, hInst, IDC_BTN_EDIT_PROFILE,
                                   L"Editar", BTN_STYLE_SECONDARY,
                                   CONTENT_PAD + BTN_W + 6, PROF_BTN_Y, BTN_W, BTN_H);
    s_hwndBtnDup  = buttons_create(parent, hInst, IDC_BTN_DUP_PROFILE,
                                   L"Duplicar", BTN_STYLE_SECONDARY,
                                   CONTENT_PAD + (BTN_W + 6) * 2, PROF_BTN_Y, BTN_W, BTN_H);
    s_hwndBtnDel  = buttons_create(parent, hInst, IDC_BTN_DEL_PROFILE,
                                   L"Excluir", BTN_STYLE_SECONDARY,
                                   CONTENT_PAD + (BTN_W + 6) * 3, PROF_BTN_Y, BTN_W, BTN_H);
}

void profiles_tab_show(BOOL visible) {
    int sw = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(s_hwndCombo,   sw);
    ShowWindow(s_hwndBtnNew,  sw);
    ShowWindow(s_hwndBtnEdit, sw);
    ShowWindow(s_hwndBtnDup,  sw);
    ShowWindow(s_hwndBtnDel,  sw);
}

void profiles_tab_load(void) {
    s_load();
    profile_refresh();
}

void profiles_tab_paint(HDC dc, int clientW) {
    SetTextColor(dc, CLR_TEXT_SECONDARY);
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
    RECT rcLbl = {CONTENT_PAD, PROF_SUBLABEL_Y, clientW - CONTENT_PAD, PROF_COMBO_Y};
    DrawTextW(dc, L"Perfil selecionado:", -1, &rcLbl,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    if (s_count > 0) {
        paint_profile_panel(dc, clientW);
    } else {
        SetTextColor(dc, CLR_TEXT_DISABLED);
        SetBkMode(dc, TRANSPARENT);
        of = (HFONT)SelectObject(dc, g_fontSubtitle);
        RECT rcEmp = {0, PROF_PANEL_Y, clientW, WIN_H - STATUSBAR_H};
        DrawTextW(dc, L"Nenhum perfil cadastrado. Clique em '+ Novo Perfil' para começar.",
                  -1, &rcEmp, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }
}

BOOL profiles_tab_command(UINT id) {
    switch (id) {
    case IDC_BTN_NEW_PROFILE:  on_new_profile();  return TRUE;
    case IDC_BTN_EDIT_PROFILE: on_edit_profile(); return TRUE;
    case IDC_BTN_DUP_PROFILE:  on_dup_profile();  return TRUE;
    case IDC_BTN_DEL_PROFILE:  on_del_profile();  return TRUE;
    }
    return FALSE;
}

BOOL profiles_tab_drawitem(DRAWITEMSTRUCT *dis) {
    if (dis->CtlID == IDC_BTN_NEW_PROFILE)
        return buttons_draw(dis, BTN_STYLE_PRIMARY);
    if (dis->CtlID == IDC_BTN_EDIT_PROFILE ||
        dis->CtlID == IDC_BTN_DUP_PROFILE  ||
        dis->CtlID == IDC_BTN_DEL_PROFILE)
        return buttons_draw(dis, BTN_STYLE_SECONDARY);
    return FALSE;
}

void profiles_tab_update_printers(const PrinterEntry *printers, int count) {
    s_printerCount = (count > MAX_PROFILES) ? MAX_PROFILES : count;
    if (s_printerCount > 0)
        memcpy(s_printers, printers, (size_t)s_printerCount * sizeof(PrinterEntry));
}

const ProfileEntry* profiles_tab_get(int *out_count) {
    if (out_count) *out_count = s_count;
    return s_profiles;
}

void profiles_tab_enable(BOOL enabled) {
    EnableWindow(s_hwndCombo,   enabled);
    EnableWindow(s_hwndBtnNew,  enabled);
    EnableWindow(s_hwndBtnEdit, enabled);
    EnableWindow(s_hwndBtnDup,  enabled);
    EnableWindow(s_hwndBtnDel,  enabled);
}
