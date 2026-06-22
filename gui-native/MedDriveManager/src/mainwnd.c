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

#define MAX_PRINTERS 512

/* ── Subtítulo da aba Perfis ────────────────────────────────────────── */
#define SUBTITLE_H 28

/* ── Estado global ──────────────────────────────────────────────────── */
static HWND g_hwndMain;
static int  g_activeTab = 0; /* 0=Perfis  1=Impressoras  2=Configurações */

/* Impressoras */
static HWND g_hwndList;
static HWND g_hwndBtnAdd;
static HWND g_hwndBtnRemove;
static HWND g_hwndBtnRefresh;
static PrinterEntry g_printers[MAX_PRINTERS];
static int  g_count = 0;

/* Perfis */
static HWND g_hwndProfileList;
static HWND g_hwndBtnNewProfile;
static HWND g_hwndBtnEditProfile;
static HWND g_hwndBtnDupProfile;
static HWND g_hwndBtnDelProfile;
static ProfileEntry g_profiles[MAX_PRINTERS];
static int  g_profileCount = 0;

/* Status bar */
static HWND g_hwndStatus;

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
    if (!dlg_add_show(hwnd, &entry, g_profiles, g_profileCount)) return;
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

/* ── Helpers de negócio — Perfis ─────────────────────────────────────── */
static void profile_refresh(void) {
    ListView_DeleteAllItems(g_hwndProfileList);
    for (int i = 0; i < g_profileCount; i++) {
        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.pszText = g_profiles[i].name;
        ListView_InsertItem(g_hwndProfileList, &lvi);

        ListView_SetItemText(g_hwndProfileList, i, 1, g_profiles[i].outputBaseName);
        ListView_SetItemText(g_hwndProfileList, i, 2, g_profiles[i].outputPath);

        wchar_t *estrategia = g_profiles[i].overwriteFile ? L"Sobrescrever" : L"Incrementar";
        ListView_SetItemText(g_hwndProfileList, i, 3, estrategia);
    }
    statusbar_set_text(g_hwndStatus, g_count);
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

    ShowWindow(g_hwndProfileList,    isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnNewProfile,  isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnEditProfile, isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnDupProfile,  isPerfis      ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnDelProfile,  isPerfis      ? SW_SHOW : SW_HIDE);

    ShowWindow(g_hwndList,           isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnAdd,         isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRemove,      isImpressoras ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hwndBtnRefresh,     isImpressoras ? SW_SHOW : SW_HIDE);

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
                                     (BOOL)entry.overwriteFile)) return;
    load_profiles();
    profile_refresh();
}

static void on_edit_profile(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndProfileList, -1, LVNI_SELECTED);
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
                                   (BOOL)edited.overwriteFile)) return;
    load_profiles();
    profile_refresh();
    sync_with_system();
}

static void on_dup_profile(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndProfileList, -1, LVNI_SELECTED);
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
                                     (BOOL)result.overwriteFile)) return;
    load_profiles();
    profile_refresh();
}

static void on_del_profile(HWND hwnd) {
    int sel = ListView_GetNextItem(g_hwndProfileList, -1, LVNI_SELECTED);
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
    int btnY = WIN_H - STATUSBAR_H - BTNBAR_H + (BTNBAR_H - BTN_H) / 2;

    /* ── ListView de perfis (tab 0, começa visível) ─────────────────── */
    int lvProfileY = TITLEBAR_H + NAVBAR_H + SUBTITLE_H;
    int lvProfileH = WIN_H - lvProfileY - BTNBAR_H - STATUSBAR_H - CONTENT_PAD;
    g_hwndProfileList = listview_create_profile(hwnd, hInst,
                                                 lvX, lvProfileY, lvW, lvProfileH);

    g_hwndBtnNewProfile = buttons_create(hwnd, hInst, IDC_BTN_NEW_PROFILE,
                                          L"Novo Perfil", BTN_STYLE_PRIMARY,
                                          CONTENT_PAD, btnY, BTN_W, BTN_H);
    g_hwndBtnEditProfile = buttons_create(hwnd, hInst, IDC_BTN_EDIT_PROFILE,
                                           L"Editar", BTN_STYLE_SECONDARY,
                                           CONTENT_PAD + BTN_W + 8, btnY, BTN_W, BTN_H);
    g_hwndBtnDupProfile = buttons_create(hwnd, hInst, IDC_BTN_DUP_PROFILE,
                                          L"Duplicar", BTN_STYLE_SECONDARY,
                                          CONTENT_PAD + (BTN_W + 8) * 2, btnY, BTN_W, BTN_H);
    g_hwndBtnDelProfile = buttons_create(hwnd, hInst, IDC_BTN_DEL_PROFILE,
                                          L"Excluir", BTN_STYLE_SECONDARY,
                                          CONTENT_PAD + (BTN_W + 8) * 3, btnY, BTN_W, BTN_H);

    /* ── ListView de impressoras (tab 1, começa oculto) ─────────────── */
    int lvY = TITLEBAR_H + NAVBAR_H + SUBTITLE_H;
    int lvH = WIN_H - lvY - BTNBAR_H - STATUSBAR_H - CONTENT_PAD;
    g_hwndList = listview_create(hwnd, hInst, lvX, lvY, lvW, lvH);

    g_hwndBtnAdd = buttons_create(hwnd, hInst, IDC_BTN_ADD,
                                   L"Adicionar", BTN_STYLE_PRIMARY,
                                   CONTENT_PAD, btnY, BTN_W, BTN_H);
    g_hwndBtnRemove = buttons_create(hwnd, hInst, IDC_BTN_REMOVE,
                                      L"Remover", BTN_STYLE_SECONDARY,
                                      CONTENT_PAD + BTN_W + 8, btnY, BTN_W, BTN_H);
    g_hwndBtnRefresh = buttons_create(hwnd, hInst, IDC_BTN_REFRESH,
                                       L"Atualizar", BTN_STYLE_SECONDARY,
                                       CONTENT_PAD + (BTN_W + 8) * 2, btnY, BTN_W, BTN_H);

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
    int contentBot  = WIN_H - STATUSBAR_H - BTNBAR_H;
    RECT rcContent = {0, contentTop, w, contentBot};
    FillRect(dc, &rcContent, g_hbrPrimary);

    /* Subtítulo da aba ativa */
    if (g_activeTab == 0 || g_activeTab == 1) {
        const wchar_t *sub = (g_activeTab == 0) ? L"PERFIS DE IMPRESSÃO"
                                                 : L"IMPRESSORAS CADASTRADAS";
        SetTextColor(dc, CLR_TEXT_SECONDARY);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSmall);
        RECT rcSub = {CONTENT_PAD, contentTop + 8,
                      w - CONTENT_PAD, contentTop + SUBTITLE_H};
        DrawTextW(dc, sub, -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }

    /* Estado vazio da aba Impressoras */
    if (g_activeTab == 1 && g_count == 0) {
        RECT rcEmp = {CONTENT_PAD, contentTop + CONTENT_PAD,
                      w - CONTENT_PAD, contentBot - CONTENT_PAD};
        paint_empty_state(dc, rcEmp);
    }

    /* Aba Configurações */
    if (g_activeTab == 2) {
        SetTextColor(dc, CLR_TEXT_DISABLED);
        SetBkMode(dc, TRANSPARENT);
        HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
        DrawTextW(dc, L"Configurações — em breve.", -1, &rcContent,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, of);
    }

    /* Barra de botões */
    int btnBarTop = WIN_H - STATUSBAR_H - BTNBAR_H;
    RECT rcBtnBar = {0, btnBarTop, w, btnBarTop + BTNBAR_H};
    FillRect(dc, &rcBtnBar, g_hbrSecondary);

    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    RECT rcSep = {0, btnBarTop, w, btnBarTop + 1};
    FillRect(dc, &rcSep, hbrd);
    DeleteObject(hbrd);

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
        if (mis->CtlID == IDC_PRINTER_LIST || mis->CtlID == IDC_PROFILE_LIST) {
            listview_measure(mis);
            return TRUE;
        }
        return FALSE;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lp;
        if (dis->CtlID == IDC_PRINTER_LIST || dis->CtlID == IDC_PROFILE_LIST) {
            listview_draw_item(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_BTN_TITLEMIN || dis->CtlID == IDC_BTN_TITLECLOSE) {
            titlebar_draw_button(dis);
            return TRUE;
        }
        if (dis->CtlID == IDC_BTN_ADD || dis->CtlID == IDC_BTN_NEW_PROFILE)
            return buttons_draw(dis, BTN_STYLE_PRIMARY);
        if (dis->CtlID == IDC_BTN_REMOVE  || dis->CtlID == IDC_BTN_REFRESH  ||
            dis->CtlID == IDC_BTN_EDIT_PROFILE ||
            dis->CtlID == IDC_BTN_DUP_PROFILE  ||
            dis->CtlID == IDC_BTN_DEL_PROFILE)
            return buttons_draw(dis, BTN_STYLE_SECONDARY);
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_ADD:           on_add(hwnd);       break;
        case IDC_BTN_REMOVE:        on_remove(hwnd);    break;
        case IDC_BTN_REFRESH:       sync_with_system(); break;
        case IDC_BTN_TITLEMIN:      ShowWindow(hwnd, SW_MINIMIZE); break;
        case IDC_BTN_TITLECLOSE:    DestroyWindow(hwnd); break;
        case IDC_BTN_NEW_PROFILE:   on_new_profile(hwnd);          break;
        case IDC_BTN_EDIT_PROFILE:  on_edit_profile(hwnd); break;
        case IDC_BTN_DUP_PROFILE:   on_dup_profile(hwnd);  break;
        case IDC_BTN_DEL_PROFILE:   on_del_profile(hwnd);  break;
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

    MARGINS m = {0, 0, 0, 1};
    DwmExtendFrameIntoClientArea(hwnd, &m);

    return hwnd;
}
