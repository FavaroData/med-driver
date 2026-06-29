#pragma once

#define IDC_STATIC          (-1)

#define IDC_PRINTER_LIST    101
#define IDC_BTN_ADD         102
#define IDC_BTN_REMOVE      103
#define IDC_STATUS_BAR      104
#define IDC_BTN_REFRESH         105
#define IDC_BTN_EDIT_PRINTER    106

#define IDD_ADD_PRINTER          200
#define IDC_EDIT_NAME            201
#define IDC_COMBO_PROFILE        210
#define IDC_LBL_PRV_PATH         211
#define IDC_LBL_PRV_BASENAME     212
#define IDC_LBL_PRV_STRATEGY     213
#define IDC_LBL_PRV_OPEN_AFTER   214

#define IDM_EXIT            401

#define IDD_PROGRESS              500
#define IDC_EDIT_OUTPUT           501
#define IDC_BTN_CLOSE_PROGRESS    502

/* ── Ícones (Q6) ─────────────────────────────────────────────────── */
#define IDI_ICO_ADD20       601
#define IDI_ICO_DELETE20    602
#define IDI_ICO_SYNC20      603
#define IDI_ICO_PRINTER20   604
#define IDI_ICO_PRINTER16   605
#define IDI_ICO_PRINTER48   606
#define IDI_ICO_SETTINGS20  607
#define IDI_ICO_FOLDER20    608
#define IDI_ICO_FOLDER16    609
#define IDI_ICO_DOCUMENT16  610
#define IDI_ICO_INFO16      611
#define IDI_ICO_PLUG16      612
#define IDI_ICO_APP         613

/* ── Aba Perfis ──────────────────────────────────────────────────── */
#define IDC_PROFILE_LIST         110
#define IDC_BTN_NEW_PROFILE      111
#define IDC_BTN_EDIT_PROFILE     112
#define IDC_BTN_DUP_PROFILE      113
#define IDC_BTN_DEL_PROFILE      114
#define IDC_COMBO_PROFILE_SEL    115

/* ── Estilo compartilhado: rótulo de seção (azul accent) ─────────── */
#define IDC_SECTION_LBL            120

/* ── Dialog Novo Perfil ──────────────────────────────────────────── */
#define IDD_ADD_PROFILE            300
#define IDC_EDIT_PROFILE_NAME      301
#define IDC_EDIT_PROFILE_BASENAME  302
#define IDC_BTN_PROFILE_TOKEN      303
#define IDC_LBL_PROFILE_PREVIEW    304
#define IDC_EDIT_PROFILE_PATH      305
#define IDC_BTN_PROFILE_BROWSE     306
#define IDC_CHK_OPEN_AFTER         307
#define IDC_CHK_OVERWRITE          308
#define IDC_CHK_CHOOSE_PATH        309

/* ── Aba Configurações ───────────────────────────────────────────── */
#define IDC_CHK_AGENT_AUTOSTART    130
#define IDC_BTN_CFG_SAVE           131
#define IDC_BTN_CFG_DISCARD        132
#define IDC_CHK_REQUIRE_AGENT      133
#define IDC_LBL_GS_PATH            134
#define IDC_BTN_GS_CHANGE          135
#define IDC_BTN_GS_TEST            136

/* IDs dos controles do card Logs, que fica dentro da aba Configuracoes.
   O combobox controla o intervalo de limpeza automatica do arquivo de log.
   Os dois botoes abrem a pasta e limpam o conteudo do log. */
#define IDC_CMB_LOG_AUTOCLEAN   140
#define IDC_BTN_LOG_OPEN        141
#define IDC_BTN_LOG_CLEAR       142

/* IDs dos controles do card Backup, tambem dentro da aba Configuracoes.
   Os dois botoes gravam e restauram perfis, impressoras e configuracoes em JSON. */
#define IDC_BTN_BACKUP_EXPORT   150
#define IDC_BTN_BACKUP_IMPORT   151
