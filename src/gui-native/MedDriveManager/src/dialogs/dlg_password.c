#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef MEDDRIVE_XP
#include <wincrypt.h>   /* XP: bcrypt.dll e Vista+; usa CryptoAPI (advapi32) */
#else
#include <bcrypt.h>
#endif
#include <stdio.h>
#include "dlg_password.h"
#include "resource.h"

#ifdef MEDDRIVE_XP
/* XP: SHA-256 via CryptoAPI legada (advapi32). PROV_RSA_AES tem SHA-256 no XP SP3;
   no SP2 cai no provedor "Prototype". Mesmo digest que o bcrypt -> hash compativel. */
#ifndef CALG_SHA_256
#define CALG_SHA_256 0x0000800c
#endif
#ifndef PROV_RSA_AES
#define PROV_RSA_AES 24
#endif
static void sha256_hex(const wchar_t *pwd, wchar_t out[65]) {
    int u8len = WideCharToMultiByte(CP_UTF8, 0, pwd, -1, NULL, 0, NULL, NULL);
    char *u8 = (char *)HeapAlloc(GetProcessHeap(), 0, (size_t)u8len);
    WideCharToMultiByte(CP_UTF8, 0, pwd, -1, u8, u8len, NULL, NULL);

    BYTE hash[32] = {0};
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    if (CryptAcquireContextW(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) ||
        CryptAcquireContextW(&hProv, NULL,
            L"Microsoft Enhanced RSA and AES Cryptographic Provider (Prototype)",
            PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptHashData(hHash, (BYTE *)u8, (DWORD)(u8len - 1), 0);
            DWORD hlen = 32;
            CryptGetHashParam(hHash, HP_HASHVAL, hash, &hlen, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    HeapFree(GetProcessHeap(), 0, u8);

    for (int i = 0; i < 32; i++)
        _snwprintf_s(out + i * 2, 3, _TRUNCATE, L"%02x", hash[i]);
    out[64] = 0;
}
#else
static void sha256_hex(const wchar_t *pwd, wchar_t out[65]) {
    int u8len = WideCharToMultiByte(CP_UTF8, 0, pwd, -1, NULL, 0, NULL, NULL);
    char *u8 = (char *)HeapAlloc(GetProcessHeap(), 0, (size_t)u8len);
    WideCharToMultiByte(CP_UTF8, 0, pwd, -1, u8, u8len, NULL, NULL);

    BCRYPT_ALG_HANDLE alg;
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCRYPT_HASH_HANDLE hHash;
    BCryptCreateHash(alg, &hHash, NULL, 0, NULL, 0, 0);
    BCryptHashData(hHash, (PBYTE)u8, (ULONG)(u8len - 1), 0);
    BYTE hash[32];
    BCryptFinishHash(hHash, hash, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(alg, 0);
    HeapFree(GetProcessHeap(), 0, u8);

    for (int i = 0; i < 32; i++)
        _snwprintf_s(out + i * 2, 3, _TRUNCATE, L"%02x", hash[i]);
    out[64] = 0;
}
#endif

static INT_PTR CALLBACK dlg_unlock_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL) { EndDialog(hwnd, 0); return TRUE; }
        if (LOWORD(wp) == IDOK) {
            static const wchar_t HASH[] =
                L"4a18ff0209286b695cfe5099c77843d8e96711decdf93da7e8892ba38d0c6fbd";
            wchar_t pwd[128] = {0};
            GetDlgItemTextW(hwnd, IDC_EDIT_SENHA, pwd, 128);
            wchar_t hash[65];
            sha256_hex(pwd, hash);
            if (wcscmp(hash, HASH) != 0) {
                MessageBoxW(hwnd, L"Senha incorreta.", L"Desbloquear",
                            MB_ICONERROR | MB_OK);
                SetDlgItemTextW(hwnd, IDC_EDIT_SENHA, L"");
                SetFocus(GetDlgItem(hwnd, IDC_EDIT_SENHA));
                return TRUE;
            }
            EndDialog(hwnd, 1);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

BOOL dlg_password_unlock(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE);
    return (BOOL)DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_PASSWORD_UNLOCK),
                                 parent, dlg_unlock_proc, 0);
}
