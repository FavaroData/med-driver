#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <winspool.h>
#include "printers_tab.h"
#include "store.h"
#include "resource.h"
#include "ui/theme.h"
#include "ui/statusbar.h"
#include "ui/listview.h"
#include "ui/buttons.h"
#include "dialogs/dlg_add.h"
#include "dialogs/dlg_progress.h"

#define MAX_PRINTERS 512

static HWND s_hwndParent;
static HWND s_hwndStatus;
static HWND s_hwndList;
static HWND s_hwndBtnAdd;
static HWND s_hwndBtnRemove;
static HWND s_hwndBtnEditPrinter;
static HWND s_hwndBtnRefresh;

static PrinterEntry  s_printers[MAX_PRINTERS];
static int           s_count = 0;

static void (*s_on_change_cb)(void) = NULL;

/* ── Helpers privados ───────────────────────────────────────────────────── */

static void list_refresh(void) {
    ListView_DeleteAllItems(s_hwndList);
    for (int i = 0; i < s_count; i++) {
        LVITEMW lvi = {0};
        lvi.mask    = LVIF_TEXT;
        lvi.iItem   = i;
        lvi.pszText = s_printers[i].name;
        ListView_InsertItem(s_hwndList, &lvi);
        ListView_SetItemText(s_hwndList, i, 1, s_printers[i].profileName);
    }
    statusbar_set_text(s_hwndStatus, s_count);
    InvalidateRect(s_hwndParent, NULL, FALSE);
}

static void do_sync(void) {
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

    s_count = newCount;
    memcpy(s_printers, newPrinters, (size_t)newCount * sizeof(PrinterEntry));
    list_refresh();
}

/* ── Ações ──────────────────────────────────────────────────────────────── */

static void on_add(void) {
    if (s_count >= MAX_PRINTERS) return;

    wchar_t dllPath[MAX_PATH];
    GetSystemDirectoryW(dllPath, MAX_PATH);
    wcsncat_s(dllPath, MAX_PATH, L"\\meddrivemon.dll", _TRUNCATE);
    if (GetFileAttributesW(dllPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(s_hwndParent,
            L"meddrivemon.dll não encontrada em System32.\r\n"
            L"Execute o instalador principal antes de adicionar impressoras.",
            L"Pré-requisito ausente", MB_ICONERROR | MB_OK);
        return;
    }

    ProfileEntry *profiles = NULL;
    int profileCount = profile_load(&profiles);
    if (profileCount == 0) {
        profile_free(profiles);
        MessageBoxW(s_hwndParent,
            L"Nenhum perfil cadastrado.\r\n"
            L"Crie um perfil na aba PERFIS antes de adicionar uma impressora.",
            L"Sem perfis disponíveis", MB_ICONWARNING | MB_OK);
        return;
    }

    PrinterEntry entry = {0};
    BOOL added = dlg_add_show(s_hwndParent, &entry, profiles, profileCount, NULL);
    profile_free(profiles);
    if (!added) return;
    if (!dlg_progress_run(s_hwndParent, entry.name, entry.profileName)) return;
    do_sync();
    if (s_on_change_cb) s_on_change_cb();
}

static void on_remove(void) {
    int sel = ListView_GetNextItem(s_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= s_count) return;

    wchar_t confirm[512];
    _snwprintf_s(confirm, 512, _TRUNCATE,
        L"Remover a impressora \"%s\"?\r\n\r\n"
        L"A impressora, a porta e todas as configurações associadas\r\n"
        L"serão removidas do Windows.",
        s_printers[sel].name);
    if (MessageBoxW(s_hwndParent, confirm, L"Confirmar remoção",
                    MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
        return;

    wchar_t name[PRINTER_NAME_MAX];
    wcsncpy_s(name, PRINTER_NAME_MAX, s_printers[sel].name, _TRUNCATE);
    if (!dlg_progress_remove(s_hwndParent, name)) return;
    do_sync();
    if (s_on_change_cb) s_on_change_cb();
}

static void on_edit_printer(void) {
    int sel = ListView_GetNextItem(s_hwndList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= s_count) return;

    ProfileEntry *profiles = NULL;
    int profileCount = profile_load(&profiles);
    if (profileCount == 0) {
        profile_free(profiles);
        MessageBoxW(s_hwndParent,
            L"Nenhum perfil cadastrado.\r\n"
            L"Crie um perfil na aba PERFIS antes de editar.",
            L"Sem perfis disponíveis", MB_ICONWARNING | MB_OK);
        return;
    }

    wchar_t oldName[PRINTER_NAME_MAX];
    wcsncpy_s(oldName, PRINTER_NAME_MAX, s_printers[sel].name, _TRUNCATE);

    PrinterEntry entry = {0};
    BOOL edited = dlg_add_show(s_hwndParent, &entry, profiles, profileCount, &s_printers[sel]);
    profile_free(profiles);
    if (!edited) return;
    if (!dlg_progress_edit_printer(s_hwndParent, oldName, entry.name, entry.profileName)) return;
    do_sync();
    if (s_on_change_cb) s_on_change_cb();
}

/* ── API pública ─────────────────────────────────────────────────────────── */

void printers_tab_create(HWND parent, HINSTANCE hInst, HWND hwndStatus) {
    s_hwndParent = parent;
    s_hwndStatus = hwndStatus;

    int lvX = CONTENT_PAD;
    int lvW = WIN_W - CONTENT_PAD * 2;
    int lvY = TITLEBAR_H + NAVBAR_H + SUBTITLE_H + 5;
    int lvH = WIN_H - lvY - BTNBAR_H - STATUSBAR_H - CONTENT_PAD;
    int btnY = WIN_H - STATUSBAR_H - BTNBAR_H + (BTNBAR_H - BTN_H) / 2;

    s_hwndList = listview_create(parent, hInst, lvX, lvY, lvW, lvH);

    s_hwndBtnAdd = buttons_create(parent, hInst, IDC_BTN_ADD,
                                   L"Adicionar", BTN_STYLE_PRIMARY,
                                   CONTENT_PAD, btnY, BTN_W, BTN_H);
    s_hwndBtnRemove = buttons_create(parent, hInst, IDC_BTN_REMOVE,
                                      L"Remover", BTN_STYLE_SECONDARY,
                                      CONTENT_PAD + BTN_W + 8, btnY, BTN_W, BTN_H);
    s_hwndBtnEditPrinter = buttons_create(parent, hInst, IDC_BTN_EDIT_PRINTER,
                                           L"Editar", BTN_STYLE_SECONDARY,
                                           CONTENT_PAD + (BTN_W + 8) * 2, btnY, BTN_W, BTN_H);
    s_hwndBtnRefresh = buttons_create(parent, hInst, IDC_BTN_REFRESH,
                                       L"Atualizar", BTN_STYLE_SECONDARY,
                                       CONTENT_PAD + (BTN_W + 8) * 3, btnY, BTN_W, BTN_H);
}

void printers_tab_show(BOOL visible) {
    int sw = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(s_hwndList,            sw);
    ShowWindow(s_hwndBtnAdd,          sw);
    ShowWindow(s_hwndBtnRemove,       sw);
    ShowWindow(s_hwndBtnEditPrinter,  sw);
    ShowWindow(s_hwndBtnRefresh,      sw);
}

void printers_tab_sync(void) {
    do_sync();
    if (s_on_change_cb) s_on_change_cb();
}

void printers_tab_paint(HDC dc, RECT rcContent) {
    if (s_count > 0) return;

    int cx = (rcContent.left + rcContent.right)  / 2;
    int cy = (rcContent.top  + rcContent.bottom) / 2;

    if (g_icoPrinter48)
        DrawIconEx(dc, cx - 48, cy - 80, g_icoPrinter48, 48, 48, 0, NULL, DI_NORMAL);

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

BOOL printers_tab_command(UINT id) {
    switch (id) {
    case IDC_BTN_ADD:          on_add();          return TRUE;
    case IDC_BTN_REMOVE:       on_remove();       return TRUE;
    case IDC_BTN_EDIT_PRINTER: on_edit_printer(); return TRUE;
    case IDC_BTN_REFRESH:      printers_tab_sync(); return TRUE;
    }
    return FALSE;
}

BOOL printers_tab_drawitem(DRAWITEMSTRUCT *dis) {
    if (dis->CtlID == IDC_BTN_ADD)
        return buttons_draw(dis, BTN_STYLE_PRIMARY);
    if (dis->CtlID == IDC_BTN_REMOVE      ||
        dis->CtlID == IDC_BTN_REFRESH     ||
        dis->CtlID == IDC_BTN_EDIT_PRINTER)
        return buttons_draw(dis, BTN_STYLE_SECONDARY);
    return FALSE;
}

BOOL printers_tab_measure(MEASUREITEMSTRUCT *mis) {
    if (mis->CtlID == IDC_PRINTER_LIST) {
        listview_measure(mis);
        return TRUE;
    }
    return FALSE;
}

void printers_tab_set_on_change(void (*cb)(void)) {
    s_on_change_cb = cb;
}

const PrinterEntry* printers_tab_get(int *out_count) {
    if (out_count) *out_count = s_count;
    return s_printers;
}

void printers_tab_enable(BOOL enabled) {
    EnableWindow(s_hwndList,            enabled);
    EnableWindow(s_hwndBtnAdd,          enabled);
    EnableWindow(s_hwndBtnRemove,       enabled);
    EnableWindow(s_hwndBtnEditPrinter,  enabled);
    EnableWindow(s_hwndBtnRefresh,      enabled);
}
