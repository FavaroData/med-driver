#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ── Paleta (#0F172A family) ─────────────────────────────────────────── */
#define CLR_BG_PRIMARY      RGB( 15,  23,  42)   /* #0F172A */
#define CLR_BG_SECONDARY    RGB( 17,  24,  39)   /* #111827 */
#define CLR_CARD            RGB( 30,  41,  59)   /* #1E293B */
#define CLR_BORDER          RGB( 51,  65,  85)   /* #334155 */
#define CLR_ACCENT          RGB( 59, 130, 246)   /* #3B82F6 */
#define CLR_ACCENT_HOVER    RGB( 96, 165, 250)   /* #60A5FA */
#define CLR_TEXT_PRIMARY    RGB(248, 250, 252)   /* #F8FAFC */
#define CLR_TEXT_SECONDARY  RGB(148, 163, 184)   /* #94A3B8 */
#define CLR_TEXT_DISABLED   RGB(100, 116, 139)   /* #64748B */
#define CLR_ROW_HOVER       RGB( 36,  50,  68)   /* #243244 */
#define CLR_ROW_SELECTED    RGB( 29,  78, 216)   /* #1D4ED8 */
#define CLR_TEXT_ROW        RGB(226, 232, 240)   /* #E2E8F0 */
#define CLR_BTN_PRIMARY     RGB( 37,  99, 235)   /* #2563EB */
#define CLR_BTN_PRIMARY_HOV RGB( 59, 130, 246)   /* #3B82F6 */
#define CLR_BTN_SECONDARY   RGB( 55,  65,  81)   /* #374151 */
#define CLR_BTN_SEC_HOV     RGB( 71,  85, 105)
#define CLR_STATUSBAR_BG    RGB( 11,  18,  32)   /* #0B1220 */
#define CLR_TITLEBTN_HOV    RGB( 51,  65,  85)
#define CLR_CLOSE_HOV       RGB(239,  68,  68)   /* #EF4444 */
#define CLR_CLOSE_PRS       RGB(185,  28,  28)

/* ── Layout (pixels físicos) ─────────────────────────────────────────── */
#define WIN_W           960
#define WIN_H           620
#define TITLEBAR_H       72
#define NAVBAR_H         48
#define CONTENT_PAD      24
#define STATUSBAR_H      36
#define BTNBAR_H         52
#define ROW_H            44
#define HDR_H            36
#define BTN_W           130
#define BTN_H            36
#define TITLEBTN_W       46
#define NAV_TAB_W       180

/* ── IDs dos botões da title bar ─────────────────────────────────────── */
#define IDC_BTN_TITLEMIN   901
#define IDC_BTN_TITLECLOSE 902

/* ── Fontes (criadas em theme_init, destruídas em theme_destroy) ──────── */
extern HFONT g_fontTitle;       /* Segoe UI Semibold 18px */
extern HFONT g_fontSubtitle;    /* Segoe UI 14px          */
extern HFONT g_fontContent;     /* Segoe UI 13px          */
extern HFONT g_fontSmall;       /* Segoe UI 11px          */

/* ── Brushes compartilhados com dialogs ──────────────────────────────── */
extern HBRUSH g_hbrPrimary;     /* CLR_BG_PRIMARY   */
extern HBRUSH g_hbrSecondary;   /* CLR_BG_SECONDARY */
extern HBRUSH g_hbrCard;        /* CLR_CARD         */

/* ── Ícones (LoadImage de recursos ICON) ─────────────────────────────── */
extern HICON g_icoAdd20;
extern HICON g_icoDelete20;
extern HICON g_icoSync20;
extern HICON g_icoPrinter20;
extern HICON g_icoPrinter16;
extern HICON g_icoPrinter48;
extern HICON g_icoSettings20;
extern HICON g_icoFolder20;
extern HICON g_icoFolder16;
extern HICON g_icoDocument16;
extern HICON g_icoInfo16;
extern HICON g_icoPlug16;

void theme_init(HINSTANCE hInst);
void theme_destroy(void);
