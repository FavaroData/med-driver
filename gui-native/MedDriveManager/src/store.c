#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "store.h"

static void get_store_path(wchar_t *buf, int cch) {
    wchar_t profile[MAX_PATH];
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    _snwprintf_s(buf, cch, _TRUNCATE, L"%s\\meddrive-printers.json", profile);
}

/* Lê uma string JSON (começando em '"'), converte UTF-8 → UTF-16 e avança o ponteiro. */
static const char *read_json_str(const char *p, wchar_t *dst, int cch) {
    if (*p != '"') return p;
    p++;
    char tmp[2048] = {0};
    int i = 0;
    while (*p && *p != '"' && i < (int)sizeof(tmp) - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case '"':  tmp[i++] = '"';  break;
            case '\\': tmp[i++] = '\\'; break;
            case '/':  tmp[i++] = '/';  break;
            case 'n':  tmp[i++] = '\n'; break;
            case 'r':  tmp[i++] = '\r'; break;
            case 't':  tmp[i++] = '\t'; break;
            default:   tmp[i++] = *p;   break;
            }
        } else {
            tmp[i++] = *p;
        }
        p++;
    }
    if (*p == '"') p++;
    MultiByteToWideChar(CP_UTF8, 0, tmp, -1, dst, cch);
    return p;
}

/* Localiza o valor de uma chave JSON dentro do bloco de objeto (string de entrada). */
static const char *find_value(const char *obj, const char *key) {
    char needle[128];
    _snprintf_s(needle, sizeof(needle), _TRUNCATE, "\"%s\"", key);
    const char *p = strstr(obj, needle);
    if (!p) return NULL;
    p += strlen(needle);
    while (*p == ' ' || *p == ':') p++;
    return p;
}

int store_load(PrinterEntry **out) {
    wchar_t path[MAX_PATH];
    get_store_path(path, MAX_PATH);

    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        *out = NULL;
        return 0;
    }

    DWORD sz = GetFileSize(hf, NULL);
    char *buf = (char *)malloc((size_t)sz + 1);
    DWORD rd;
    ReadFile(hf, buf, sz, &rd, NULL);
    CloseHandle(hf);
    buf[rd] = '\0';

    int cap = 16, count = 0;
    PrinterEntry *entries = (PrinterEntry *)calloc((size_t)cap, sizeof(PrinterEntry));

    const char *p = buf;
    while ((p = strchr(p, '{')) != NULL) {
        const char *end = strchr(p, '}');
        if (!end) break;

        if (count >= cap) {
            cap *= 2;
            entries = (PrinterEntry *)realloc(entries, (size_t)cap * sizeof(PrinterEntry));
        }

        /* Copia o bloco {...} para um buffer temporário para evitar leitura além do '}' */
        int len = (int)(end - p + 1);
        char obj[4096] = {0};
        if (len > (int)sizeof(obj) - 1) len = (int)sizeof(obj) - 1;
        memcpy(obj, p, (size_t)len);

        const char *v;
        if ((v = find_value(obj, "name"))           != NULL) read_json_str(v, entries[count].name,           PRINTER_NAME_MAX);
        if ((v = find_value(obj, "portName"))       != NULL) read_json_str(v, entries[count].portName,       PRINTER_PORT_MAX);
        if ((v = find_value(obj, "outputPath"))     != NULL) read_json_str(v, entries[count].outputPath,     PRINTER_PATH_MAX);
        if ((v = find_value(obj, "outputBaseName")) != NULL) read_json_str(v, entries[count].outputBaseName, PRINTER_BASENAME_MAX);

        count++;
        p = end + 1;
    }

    free(buf);
    *out = entries;
    return count;
}

/* Escreve uma string wchar_t como valor JSON (UTF-16 → UTF-8, escapa " e \). */
static void write_json_str(char *dst, int cch, const wchar_t *src) {
    char tmp[2048];
    WideCharToMultiByte(CP_UTF8, 0, src, -1, tmp, sizeof(tmp), NULL, NULL);
    int di = 0;
    dst[di++] = '"';
    for (int i = 0; tmp[i] && di < cch - 3; i++) {
        if (tmp[i] == '"' || tmp[i] == '\\') dst[di++] = '\\';
        dst[di++] = tmp[i];
    }
    dst[di++] = '"';
    dst[di]   = '\0';
}

int store_save(const PrinterEntry *entries, int count) {
    wchar_t path[MAX_PATH];
    get_store_path(path, MAX_PATH);

    HANDLE hf = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) return -1;

    DWORD written;
    WriteFile(hf, "[\n", 2, &written, NULL);

    for (int i = 0; i < count; i++) {
        char n[512], port[256], o[1024], bn[512], line[2560];
        write_json_str(n,    sizeof(n),    entries[i].name);
        write_json_str(port, sizeof(port), entries[i].portName);
        write_json_str(o,    sizeof(o),    entries[i].outputPath);
        write_json_str(bn,   sizeof(bn),   entries[i].outputBaseName);

        int len = _snprintf_s(line, sizeof(line), _TRUNCATE,
            "  {\"name\":%s,\"portName\":%s,\"outputPath\":%s,\"outputBaseName\":%s}%s\n",
            n, port, o, bn, (i < count - 1) ? "," : "");
        WriteFile(hf, line, (DWORD)len, &written, NULL);
    }

    WriteFile(hf, "]\n", 2, &written, NULL);
    CloseHandle(hf);
    return 0;
}

void store_free(PrinterEntry *entries) {
    free(entries);
}

int profile_load(ProfileEntry **out) {
    static const WCHAR PORTS_KEY[] =
        L"SYSTEM\\CurrentControlSet\\Control\\Print\\Monitors\\"
        L"Meddrive Printer MONITOR\\Ports";
    static const WCHAR PORT_PREFIX[] = L"Meddrive Printer PORT ";
    int prefixLen = (int)wcslen(PORT_PREFIX);

    HKEY hPorts;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, PORTS_KEY, 0, KEY_READ, &hPorts) != ERROR_SUCCESS) {
        *out = NULL;
        return 0;
    }

    int cap = 16, count = 0;
    ProfileEntry *entries = (ProfileEntry *)calloc((size_t)cap, sizeof(ProfileEntry));

    DWORD idx = 0;
    WCHAR portName[256];
    DWORD portNameLen;
    for (;;) {
        portNameLen = 256;
        LONG rc = RegEnumKeyExW(hPorts, idx, portName, &portNameLen,
                                NULL, NULL, NULL, NULL);
        if (rc != ERROR_SUCCESS) break;
        idx++;

        if (count >= cap) {
            cap *= 2;
            entries = (ProfileEntry *)realloc(entries, (size_t)cap * sizeof(ProfileEntry));
        }

        ProfileEntry *e = &entries[count];
        wcsncpy_s(e->portName, PRINTER_PORT_MAX, portName, _TRUNCATE);

        if (wcsncmp(portName, PORT_PREFIX, (size_t)prefixLen) == 0)
            wcsncpy_s(e->name, PRINTER_NAME_MAX, portName + prefixLen, _TRUNCATE);
        else
            wcsncpy_s(e->name, PRINTER_NAME_MAX, portName, _TRUNCATE);

        HKEY hPort;
        if (RegOpenKeyExW(hPorts, portName, 0, KEY_READ, &hPort) == ERROR_SUCCESS) {
            DWORD type, sz;
            sz = PRINTER_PATH_MAX * sizeof(WCHAR);
            RegQueryValueExW(hPort, L"OutputPath",        NULL, &type, (BYTE *)e->outputPath,     &sz);
            sz = PRINTER_BASENAME_MAX * sizeof(WCHAR);
            RegQueryValueExW(hPort, L"OutputBaseName",    NULL, &type, (BYTE *)e->outputBaseName,  &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPort, L"OpenAfterGenerate", NULL, &type, (BYTE *)&e->openAfterGenerate, &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPort, L"OverwriteFile",     NULL, &type, (BYTE *)&e->overwriteFile,  &sz);
            RegCloseKey(hPort);
        }
        count++;
    }

    RegCloseKey(hPorts);
    *out = entries;
    return count;
}

void profile_free(ProfileEntry *entries) {
    free(entries);
}
