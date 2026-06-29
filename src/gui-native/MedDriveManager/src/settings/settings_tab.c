#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include "settings_tab.h"
#include "settings.h"
#include "../ui/theme.h"
#include "../ui/buttons.h"
#include "resource.h"

/* layout da aba */
#define CFG_CARD_Y   (TITLEBAR_H + NAVBAR_H + SUBTITLE_H + 8)
#define CFG_COL_GAP  8
#define CFG_COL_W    ((WIN_W - CONTENT_PAD * 2 - CFG_COL_GAP * 2) / 3)
#define CFG_INNER    10
#define CFG_HDR_H    45
#define CFG_CHK_H    22
#define CFG_CHK_GAP  6
#define CFG_CARD_H   (CFG_HDR_H + CFG_INNER + CFG_CHK_H + CFG_CHK_GAP + CFG_CHK_H + CFG_INNER)
#define CFG_BTN_Y    (WIN_H - STATUSBAR_H - BTNBAR_H + (BTNBAR_H - BTN_H) / 2)
#define CFG_SAVE_X   (WIN_W - CONTENT_PAD - BTN_W)
#define CFG_DISC_X   (CFG_SAVE_X - 8 - BTN_W)

static HWND        s_hwndParent;
static HWND        s_hwndChk;
static HWND        s_hwndChkRequireAgent;
static HWND        s_hwndSave;
static HWND        s_hwndDiscard;
static AppSettings s_saved;
static AppSettings s_pending;

void settings_tab_create(HWND parent, HINSTANCE hInst) {
    s_hwndParent = parent;
    s_hwndChk = CreateWindowExW(0, L"BUTTON",
        L"Iniciar MedDrive Printer Agent com o Windows",
        WS_CHILD | BS_AUTOCHECKBOX,
        CONTENT_PAD + CFG_INNER,
        CFG_CARD_Y + CFG_HDR_H + CFG_INNER,
        CFG_COL_W - CFG_INNER * 2, CFG_CHK_H,
        parent, (HMENU)(UINT_PTR)IDC_CHK_AGENT_AUTOSTART, hInst, NULL);
    SendMessageW(s_hwndChk, WM_SETFONT, (WPARAM)g_fontContent, FALSE);

    s_hwndChkRequireAgent = CreateWindowExW(0, L"BUTTON",
        L"Exigir agente ativo para criar impressoras e perfis",
        WS_CHILD | BS_AUTOCHECKBOX,
        CONTENT_PAD + CFG_INNER,
        CFG_CARD_Y + CFG_HDR_H + CFG_INNER + CFG_CHK_H + CFG_CHK_GAP,
        CFG_COL_W - CFG_INNER * 2, CFG_CHK_H,
        parent, (HMENU)(UINT_PTR)IDC_CHK_REQUIRE_AGENT, hInst, NULL);
    SendMessageW(s_hwndChkRequireAgent, WM_SETFONT, (WPARAM)g_fontContent, FALSE);

    s_hwndSave = buttons_create(parent, hInst, IDC_BTN_CFG_SAVE,
                                L"Salvar", BTN_STYLE_PRIMARY,
                                CFG_SAVE_X, CFG_BTN_Y, BTN_W, BTN_H);
    s_hwndDiscard = buttons_create(parent, hInst, IDC_BTN_CFG_DISCARD,
                                   L"Descartar", BTN_STYLE_SECONDARY,
                                   CFG_DISC_X, CFG_BTN_Y, BTN_W, BTN_H);
}

void settings_tab_show(BOOL visible) {
    int cmd = visible ? SW_SHOW : SW_HIDE;
    ShowWindow(s_hwndChk,             cmd);
    ShowWindow(s_hwndChkRequireAgent, cmd);
    ShowWindow(s_hwndSave,            cmd);
    ShowWindow(s_hwndDiscard,         cmd);
}

void settings_tab_load(void) {
    settings_load(&s_saved);
    s_pending = s_saved;
    SendMessageW(s_hwndChk, BM_SETCHECK,
                 s_pending.agentAutoStart ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(s_hwndChkRequireAgent, BM_SETCHECK,
                 s_pending.requireAgentRunning ? BST_CHECKED : BST_UNCHECKED, 0);
}

void settings_tab_paint(HDC dc) {
    int cardX = CONTENT_PAD;
    int cardY = CFG_CARD_Y;

    RECT rc = {cardX, cardY, cardX + CFG_COL_W, cardY + CFG_CARD_H};
    FillRect(dc, &rc, g_hbrCard);
    HBRUSH hbrd = CreateSolidBrush(CLR_BORDER);
    FrameRect(dc, &rc, hbrd);
    DeleteObject(hbrd);

    if (g_icoSettings20)
        DrawIconEx(dc, cardX + CFG_INNER, cardY + CFG_INNER,
                   g_icoSettings20, 20, 20, 0, NULL, DI_NORMAL);

    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, g_fontSubtitle);
    SetTextColor(dc, CLR_TEXT_PRIMARY);
    RECT rt = {cardX + CFG_INNER + 26, cardY + CFG_INNER,
               cardX + CFG_COL_W - CFG_INNER, cardY + CFG_INNER + 20};
    DrawTextW(dc, L"Geral", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);

    int sepY = cardY + CFG_HDR_H - 1;
    HPEN hp = CreatePen(PS_SOLID, 1, CLR_BORDER);
    HPEN op = (HPEN)SelectObject(dc, hp);
    MoveToEx(dc, cardX + 1, sepY, NULL);
    LineTo(dc,   cardX + CFG_COL_W - 1, sepY);
    SelectObject(dc, op);
    DeleteObject(hp);
}

BOOL settings_tab_command(UINT id) {
    if (id == IDC_BTN_CFG_SAVE) {
        int confirm = MessageBoxW(s_hwndParent,
            L"Deseja aplicar as alterações?",
            L"Meddrive Printer",
            MB_YESNO | MB_ICONQUESTION);
        if (confirm != IDYES) return TRUE;

        s_pending.agentAutoStart =
            (SendMessageW(s_hwndChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
        s_pending.requireAgentRunning =
            (SendMessageW(s_hwndChkRequireAgent, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (settings_save(&s_pending)) {
            s_saved = s_pending;
            MessageBoxW(s_hwndParent,
                L"Configurações salvas com sucesso.",
                L"Meddrive Printer",
                MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxW(s_hwndParent,
                L"Falha ao aplicar as configurações.\n"
                L"Verifique se o agente está instalado.",
                L"Meddrive Printer",
                MB_OK | MB_ICONERROR);
        }
        return TRUE;
    }
    if (id == IDC_BTN_CFG_DISCARD) {
        s_pending = s_saved;
        SendMessageW(s_hwndChk, BM_SETCHECK,
                     s_pending.agentAutoStart ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(s_hwndChkRequireAgent, BM_SETCHECK,
                     s_pending.requireAgentRunning ? BST_CHECKED : BST_UNCHECKED, 0);
        return TRUE;
    }
    return FALSE;
}

LRESULT settings_tab_ctlcolor(HWND hctl, HDC hdc) {
    if (hctl == s_hwndChk || hctl == s_hwndChkRequireAgent) {
        SetBkColor(hdc, CLR_CARD);
        SetTextColor(hdc, CLR_TEXT_PRIMARY);
        return (LRESULT)g_hbrCard;
    }
    return 0;
}

static BOOL agent_process_running(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return FALSE;
    PROCESSENTRY32W pe = {sizeof(pe)};
    BOOL found = FALSE;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"MeddrivePrinterAgent.exe") == 0) {
                found = TRUE; break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

BOOL settings_tab_require_agent(void) {
    if (SendMessageW(s_hwndChkRequireAgent, BM_GETCHECK, 0, 0) != BST_CHECKED)
        return FALSE;
    return !agent_process_running();
}

BOOL settings_tab_drawitem(DRAWITEMSTRUCT *dis) {
    if (dis->CtlID == IDC_BTN_CFG_SAVE)
        return buttons_draw(dis, BTN_STYLE_PRIMARY);
    if (dis->CtlID == IDC_BTN_CFG_DISCARD)
        return buttons_draw(dis, BTN_STYLE_SECONDARY);
    return FALSE;
}
