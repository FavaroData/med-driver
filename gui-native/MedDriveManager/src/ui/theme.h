#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ── Paleta light corporativo ────────────────────────────────────────── */
#define CLR_BG_PRIMARY      RGB(255, 255, 255)   /* #FFFFFF */
#define CLR_BG_SECONDARY    RGB(247, 248, 250)   /* #F7F8FA */
#define CLR_CARD            RGB(255, 255, 255)   /* #FFFFFF */
#define CLR_BORDER          RGB(229, 231, 235)   /* #E5E7EB */
#define CLR_ACCENT          RGB( 37,  99, 235)   /* #2563EB */
#define CLR_ACCENT_HOVER    RGB( 29,  78, 216)   /* #1D4ED8 */
#define CLR_TEXT_PRIMARY    RGB( 31,  41,  55)   /* #1F2937 */
#define CLR_TEXT_SECONDARY  RGB(107, 114, 128)   /* #6B7280 */
#define CLR_TEXT_DISABLED   RGB(156, 163, 175)   /* #9CA3AF */
#define CLR_ROW_HOVER       RGB(241, 245, 249)   /* #F1F5F9 */
#define CLR_ROW_SELECTED    RGB(219, 234, 254)   /* #DBEAFE */
#define CLR_TEXT_ROW        RGB( 31,  41,  55)   /* #1F2937 */
#define CLR_BTN_PRIMARY     RGB( 37,  99, 235)   /* #2563EB */
#define CLR_BTN_PRIMARY_HOV RGB( 29,  78, 216)   /* #1D4ED8 */
#define CLR_BTN_SECONDARY   RGB(255, 255, 255)   /* #FFFFFF */
#define CLR_BTN_SEC_HOV     RGB(247, 248, 250)   /* #F7F8FA */
#define CLR_STATUSBAR_BG    RGB(247, 248, 250)   /* #F7F8FA */
#define CLR_TITLEBTN_HOV    RGB(243, 244, 246)   /* #F3F4F6 */
#define CLR_CLOSE_HOV       RGB(220,  38,  38)   /* #DC2626 */
#define CLR_CLOSE_PRS       RGB(185,  28,  28)   /* #B91C1C */
#define CLR_GREEN           RGB( 22, 163,  74)   /* #16A34A */
#define CLR_RED             RGB(220,  38,  38)   /* #DC2626 */
#define CLR_ACCENT_LIGHT    RGB(239, 246, 255)   /* #EFF6FF */

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
