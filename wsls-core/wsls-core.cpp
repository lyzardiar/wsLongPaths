// wsls-core.cpp : Defines the entry point for the console application.

#include "stdafx.h"

#include <Windows.h>
#include <Shlwapi.h>
#include "../libwsls/libwsls.h"

#if defined(_DEBUG)
#pragma comment(lib, "../x64/Debug/libwsls.lib")
#else
#pragma comment(lib, "../x64/Release/libwsls.lib")
#endif

int main(int /*argc*/, char** /*argv*/)
{
    char fileName[4096];
    auto n = GetModuleFileNameA(GetModuleHandle(nullptr), fileName, sizeof(fileName));

    if (n > 0) {
        if (stricmp(PathFindFileNameA(fileName), "wsls-core.exe") == 0) {
            MessageBoxA(nullptr, "Please rename wsls-core.exe to what(such as make.exe, cc1plus.exe, arm-linux-androideabi-g++.exe) do you want to hook!", "tips", MB_OK | MB_ICONEXCLAMATION);
            return ERROR_OPERATION_ABORTED;
        }

        strcpy(fileName + n, ".bridge");
        std::string content = wsls::readFileData(fileName);
        if (content.empty())
            return ERROR_FILE_NOT_FOUND;

        std::wstring parameter = wsls::transcode$IL(content);
        std::wstring shell, app;

        size_t start = parameter.find_first_not_of(L" \t", 0);
        size_t colon = parameter.find_first_of(':', start);
        if (colon != std::wstring::npos) {
            size_t end = colon;
            end = parameter.find_last_not_of(L" \t", colon - 1);
            if (end > start) {
                shell = parameter.substr(start, end - start + 1);

                start = parameter.find_first_not_of(L" \t", colon + 1);
                end = parameter.find_last_not_of(L" \t", std::wstring::npos);
                if (end > start) {
                    app = parameter.substr(start, end - start + 1);
                }
            }
        }

        if (!shell.empty() && !app.empty())
            return wsls::make_bridge(shell.c_str(), app.c_str());
        else
            return ERROR_INVALID_PARAMETER;
    }

    return ERROR_UNKNOWN_FEATURE;
}
