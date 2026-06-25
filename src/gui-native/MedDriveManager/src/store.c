#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include "store.h"

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
            RegQueryValueExW(hPort, L"OutputPath",        NULL, &type, (BYTE *)e->outputPath,        &sz);
            sz = PRINTER_BASENAME_MAX * sizeof(WCHAR);
            RegQueryValueExW(hPort, L"OutputBaseName",    NULL, &type, (BYTE *)e->outputBaseName,    &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPort, L"OpenAfterGenerate", NULL, &type, (BYTE *)&e->openAfterGenerate, &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPort, L"OverwriteFile",     NULL, &type, (BYTE *)&e->overwriteFile,     &sz);
            sz = sizeof(DWORD);
            RegQueryValueExW(hPort, L"ChoosePath",        NULL, &type, (BYTE *)&e->choosePath,        &sz);
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
