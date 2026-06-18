#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "theme.h"
#include "resource.h"

HFONT  g_fontTitle;
HFONT  g_fontSubtitle;
HFONT  g_fontContent;
HFONT  g_fontSmall;

HBRUSH g_hbrPrimary;
HBRUSH g_hbrSecondary;
HBRUSH g_hbrCard;

HICON  g_icoAdd20;
HICON  g_icoDelete20;
HICON  g_icoSync20;
HICON  g_icoPrinter20;
HICON  g_icoPrinter16;
HICON  g_icoPrinter48;
HICON  g_icoSettings20;
HICON  g_icoFolder20;
HICON  g_icoFolder16;
HICON  g_icoDocument16;
HICON  g_icoInfo16;
HICON  g_icoPlug16;

static HFONT make_font(int ptSize, int weight) {
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(ptSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    return CreateFontW(h, 0, 0, 0, weight,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

static HICON load_ico(HINSTANCE hInst, int id, int w, int h) {
    return (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(id),
                             IMAGE_ICON, w, h, LR_DEFAULTCOLOR);
}

void theme_init(HINSTANCE hInst) {
    g_fontTitle    = make_font(18, FW_SEMIBOLD);
    g_fontSubtitle = make_font(14, FW_NORMAL);
    g_fontContent  = make_font(13, FW_NORMAL);
    g_fontSmall    = make_font(11, FW_NORMAL);

    g_hbrPrimary   = CreateSolidBrush(CLR_BG_PRIMARY);
    g_hbrSecondary = CreateSolidBrush(CLR_BG_SECONDARY);
    g_hbrCard      = CreateSolidBrush(CLR_CARD);

    g_icoAdd20      = load_ico(hInst, IDI_ICO_ADD20,      20, 20);
    g_icoDelete20   = load_ico(hInst, IDI_ICO_DELETE20,   20, 20);
    g_icoSync20     = load_ico(hInst, IDI_ICO_SYNC20,     20, 20);
    g_icoPrinter20  = load_ico(hInst, IDI_ICO_PRINTER20,  20, 20);
    g_icoPrinter16  = load_ico(hInst, IDI_ICO_PRINTER16,  16, 16);
    g_icoPrinter48  = load_ico(hInst, IDI_ICO_PRINTER48,  48, 48);
    g_icoSettings20 = load_ico(hInst, IDI_ICO_SETTINGS20, 20, 20);
    g_icoFolder20   = load_ico(hInst, IDI_ICO_FOLDER20,   20, 20);
    g_icoFolder16   = load_ico(hInst, IDI_ICO_FOLDER16,   16, 16);
    g_icoDocument16 = load_ico(hInst, IDI_ICO_DOCUMENT16, 16, 16);
    g_icoInfo16     = load_ico(hInst, IDI_ICO_INFO16,     16, 16);
    g_icoPlug16     = load_ico(hInst, IDI_ICO_PLUG16,     16, 16);
}

void theme_destroy(void) {
    DeleteObject(g_fontTitle);
    DeleteObject(g_fontSubtitle);
    DeleteObject(g_fontContent);
    DeleteObject(g_fontSmall);

    DeleteObject(g_hbrPrimary);
    DeleteObject(g_hbrSecondary);
    DeleteObject(g_hbrCard);

    if (g_icoAdd20)      DestroyIcon(g_icoAdd20);
    if (g_icoDelete20)   DestroyIcon(g_icoDelete20);
    if (g_icoSync20)     DestroyIcon(g_icoSync20);
    if (g_icoPrinter20)  DestroyIcon(g_icoPrinter20);
    if (g_icoPrinter16)  DestroyIcon(g_icoPrinter16);
    if (g_icoPrinter48)  DestroyIcon(g_icoPrinter48);
    if (g_icoSettings20) DestroyIcon(g_icoSettings20);
    if (g_icoFolder20)   DestroyIcon(g_icoFolder20);
    if (g_icoFolder16)   DestroyIcon(g_icoFolder16);
    if (g_icoDocument16) DestroyIcon(g_icoDocument16);
    if (g_icoInfo16)     DestroyIcon(g_icoInfo16);
    if (g_icoPlug16)     DestroyIcon(g_icoPlug16);
}
