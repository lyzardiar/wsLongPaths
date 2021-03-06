// wsLongPaths.cpp : Defines the exported functions for the DLL application.
// V3.0

#include "stdafx.h"
#include <stdio.h>
#include <string>
#include <string_view>
#include <Windows.h>
#include <Shlwapi.h>
#include <io.h>
#include "../libwsls/libwsls.h"
#include "../minhook/include/MinHook.h"

#define HOOK_GETFULLPATHNAME 0
#define ENABLE_MSGBOX_TRACE 0

#define DEBUG_MODULE L"gnumake.exe"

#if defined(_DEBUG)
#pragma comment(lib, "../lib/Debug/libMinHook.x64.lib")
#else
#pragma comment(lib, "../lib/Release/libMinHook.x64.lib")
#endif

#define DEFINE_FUNCTION_PTR(f) static decltype(&f) f##_imp
#define GET_FUNCTION(h,f) f##_imp = (decltype(f##_imp))GetProcAddress(h, #f)
#define HOOK_FUNCTION(f) MH_CreateHook(f##_imp, f##_hook, (LPVOID*)&f##_imp)
#define HOOK_FUNCTION_SAFE(f) if(f##_imp) MH_CreateHook(f##_imp, f##_hook, (LPVOID*)&f##_imp)

/*
copy命令, 无论怎样均不支持超过249的长路径
echo命令, 可以支持260长路径, 但必须有UNC前缀, 且路径必须是反斜杠
ndk提供的echo.exe,(可以支持260长路径, 但必须有UNC前缀, 且路径必须是反斜杠)
del命令, 无论如何都不支持260长路径
*/
DEFINE_FUNCTION_PTR(CreateProcessA);
DEFINE_FUNCTION_PTR(CreateProcessW);
DEFINE_FUNCTION_PTR(CreateFileA);
DEFINE_FUNCTION_PTR(CreateFileW);
#if HOOK_GETFULLPATHNAME
DEFINE_FUNCTION_PTR(GetFullPathNameA);
#endif
DEFINE_FUNCTION_PTR(GetFileAttributesA);
DEFINE_FUNCTION_PTR(GetFileAttributesW);
DEFINE_FUNCTION_PTR(GetFileAttributesExW);
DEFINE_FUNCTION_PTR(FindFirstFileExW);

static std::wstring s_appName;
static std::wstring s_oringalAppName;

// replaceAsOriginal
static std::wstring replaceAsOriginal(const wchar_t* s)
{
    if (s != nullptr) {
        std::wstring o = s;
        wsls::replace_once(o, s_appName, s_oringalAppName);
        return o;
    }
    return L"";
}

BOOL
WINAPI
CreateProcessA_hook(
    _In_opt_ LPCSTR lpApplicationName,
    _Inout_opt_ LPSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOA lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: CreateProcessA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    if (lpApplicationName != nullptr && stricmp(PathFindExtensionA(lpApplicationName), ".bat") == 0) {
        std::string fileContent = wsls::readFileData(lpApplicationName);

        size_t offset = std::string::npos;
        if (fileContent.find("wsls-echo.exe") == std::string::npos && (offset = fileContent.find("echo.exe")) != std::string::npos)
        {
            fileContent.replace(offset, sizeof("echo.exe") - 1, "wsls-echo.exe");
            auto rpos = fileContent.find(">>", offset + sizeof("wsls-echo.exe") - 1);
            if (rpos != std::string::npos) {
                fileContent.replace(rpos, 2, "-a");
            }
            else {
                rpos = fileContent.find_first_of('>');
                if(rpos != std::string::npos) fileContent.replace(rpos, 1, "-w");
            }

            if (rpos != std::string::npos)
            {
                wsls::convertPathToWinStyle(fileContent, rpos + 2);

                wsls::writeFileData(lpApplicationName, fileContent);
            }
        }
        else if (fileContent.find("wsls-copy") == std::string::npos && wsls::replace_once(fileContent, "copy /b/y", "wsls-copy /b/y"))
        {// Replace the system copy which does not support long path >= 249
            wsls::writeFileData(lpApplicationName, fileContent);
        }
        else if (fileContent.find("wsls-del") == std::string::npos && wsls::replace_once(fileContent, "del", "wsls-del"))
        {// Replace the system del which does not support long path >= 249
            wsls::writeFileData(lpApplicationName, fileContent);
        }
        else if (fileContent.find("wsls-md") == std::string::npos && wsls::replace_once(fileContent, R"(md ")", R"(wsls-md ")"))
        {// Replace the system del which does not support long path >= 249, Fix windows 7 md LongPath issue
            wsls::writeFileData(lpApplicationName, fileContent);
        }

#if defined(_DEBUG)
        OutputDebugStringA(wsls::sfmt("Create Process:%s, content:%s\n",
            lpApplicationName,
            fileContent.c_str()
        ).c_str());
#endif
    }
#if defined(_DEBUG)
    else { // execute command line
        OutputDebugStringA(wsls::sfmt("execute command: %s\n", lpCommandLine).c_str());
    }
#endif
    return CreateProcessA_imp(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

BOOL
WINAPI
CreateProcessW_hook(
    _In_opt_ LPCWSTR lpApplicationName,
    _Inout_opt_ LPWSTR lpCommandLine,
    _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
    _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
    _In_ BOOL bInheritHandles,
    _In_ DWORD dwCreationFlags,
    _In_opt_ LPVOID lpEnvironment,
    _In_opt_ LPCWSTR lpCurrentDirectory,
    _In_ LPSTARTUPINFOW lpStartupInfo,
    _Out_ LPPROCESS_INFORMATION lpProcessInformation
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: CreateProcessW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    std::wstring strApplication = replaceAsOriginal(lpApplicationName);
    std::wstring strCmdLine = replaceAsOriginal(lpCommandLine);
    
    return CreateProcessW_imp(!strApplication.empty() ? strApplication.c_str() : nullptr, 
        &strCmdLine.front(), 
        lpProcessAttributes, lpThreadAttributes,
        bInheritHandles, dwCreationFlags, lpEnvironment, 
        lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

HANDLE
WINAPI
CreateFileA_hook(
    _In_ LPCSTR lpFileName,
    _In_ DWORD dwDesiredAccess,
    _In_ DWORD dwShareMode,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ DWORD dwCreationDisposition,
    _In_ DWORD dwFlagsAndAttributes,
    _In_opt_ HANDLE hTemplateFile
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: CreateFileA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    auto styledPath = wsls::makeStyledPath(lpFileName);
    if (styledPath.empty())
        return CreateFileA_imp(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    return CreateFileW_imp(styledPath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE
WINAPI
CreateFileW_hook(
    _In_ LPCWSTR lpFileName,
    _In_ DWORD dwDesiredAccess,
    _In_ DWORD dwShareMode,
    _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    _In_ DWORD dwCreationDisposition,
    _In_ DWORD dwFlagsAndAttributes,
    _In_opt_ HANDLE hTemplateFile
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: CreateFileW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    auto styledPath = wsls::makeStyledPath(lpFileName);
    if (styledPath.empty())
        return CreateFileW_imp(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);

    return CreateFileW_imp(styledPath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

#if HOOK_GETFULLPATHNAME
DWORD
WINAPI
GetFullPathNameA_hook(
    _In_ LPCSTR lpFileName,
    _In_ DWORD nBufferLength,
    _Out_writes_to_opt_(nBufferLength, return +1) LPSTR lpBuffer,
    _Outptr_opt_ LPSTR* lpFilePart
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: GetFullPathNameA...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif

    auto styledPath = wsls::makeStyledPath(lpFileName);
    if (styledPath.empty())
        return GetFullPathNameA_imp(lpFileName, nBufferLength, lpBuffer, lpFilePart);

    int n = ::WideCharToMultiByte(CP_ACP, 0, styledPath.c_str(), styledPath.length() + 1, NULL, 0, NULL, NULL);
    if (nBufferLength > n)
        return ::WideCharToMultiByte(CP_ACP, 0, styledPath.c_str(), styledPath.length() + 1, lpBuffer, nBufferLength, NULL, NULL);
    else
        return n;
}
#endif

DWORD
WINAPI
GetFileAttributesW_hook(
    _In_ LPCWSTR lpFileName
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: GetFileAttributesW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    auto styledPath = wsls::makeStyledPath(lpFileName);
    if (styledPath.empty()) {
        return GetFileAttributesW_imp(lpFileName);
    }
    return GetFileAttributesW_imp(styledPath.c_str());
}

BOOL
WINAPI
GetFileAttributesExW_hook(
    _In_ LPCWSTR lpFileName,
    _In_ GET_FILEEX_INFO_LEVELS fInfoLevelId,
    _Out_writes_bytes_(sizeof(WIN32_FILE_ATTRIBUTE_DATA)) LPVOID lpFileInformation
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: GetFileAttributesExW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    auto styledPath = wsls::makeStyledPath(lpFileName);
    if (styledPath.empty())
        return GetFileAttributesExW_imp(lpFileName, fInfoLevelId, lpFileInformation);

    return GetFileAttributesExW_imp(styledPath.c_str(), fInfoLevelId, lpFileInformation);
}

HANDLE
WINAPI
FindFirstFileExW_hook(
    _In_ LPCWSTR lpFileName,
    _In_ FINDEX_INFO_LEVELS fInfoLevelId,
    _Out_writes_bytes_(sizeof(WIN32_FIND_DATAW)) LPVOID lpFindFileData,
    _In_ FINDEX_SEARCH_OPS fSearchOp,
    _Reserved_ LPVOID lpSearchFilter,
    _In_ DWORD dwAdditionalFlags
)
{
#if ENABLE_MSGBOX_TRACE
    MessageBox(nullptr, L"Call API: FindFirstFileExW...", L"Waiting debugger to attach...", MB_OK | MB_ICONEXCLAMATION);
#endif
    auto styledPath = wsls::makeStyledPath(lpFileName);
    if (styledPath.empty())
        return FindFirstFileExW_imp(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

    return FindFirstFileExW_imp(styledPath.c_str(), fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

void InstallHook()
{
    wchar_t appName[MAX_PATH * 2+ 1];
    auto hAppModule = GetModuleHandle(nullptr);
    GetModuleFileNameW(hAppModule, appName, MAX_PATH * 2); // Kernel32.dll --> KernelBase.dll: GetModuleFileNameA --> GetModuleFileNameW
    // The API CreateProcess call flow
    // Windows 7: kernel32.dll --> ntdll.dll
    // Windows 10: kernel32.dll --> kernelbase.dll --> ntdll.dll
    HMODULE hModule = GetModuleHandle(L"Kernel32.dll");
    GET_FUNCTION(hModule, CreateProcessA);
    GET_FUNCTION(hModule, CreateProcessW);

    hModule = GetModuleHandle(L"KernelBase.dll");
    GET_FUNCTION(hModule, CreateFileA);
    GET_FUNCTION(hModule, CreateFileW);
#if HOOK_GETFULLPATHNAME
    GET_FUNCTION(hModule, GetFullPathNameA);
#endif
    GET_FUNCTION(hModule, GetFileAttributesW);
    GET_FUNCTION(hModule, GetFileAttributesExW);
    GET_FUNCTION(hModule, FindFirstFileExW);

    MH_Initialize();

    HOOK_FUNCTION(CreateProcessA);
    HOOK_FUNCTION(CreateProcessW);
    HOOK_FUNCTION(CreateFileA);
    HOOK_FUNCTION(CreateFileW); // _stat64 --> CreateFileW
#if HOOK_GETFULLPATHNAME
    HOOK_FUNCTION(GetFullPathNameA);
#endif
    HOOK_FUNCTION(GetFileAttributesW); // GetFileAttributesA --> GetFileAttributesW
    HOOK_FUNCTION(GetFileAttributesExW); // GetFileAttributesEx --> GetFileAttributesExW
    HOOK_FUNCTION(FindFirstFileExW); // FindFirstFile(A/W), FindFirstFileExA --> FindFirstFileExW

    MH_EnableHook(MH_ALL_HOOKS);

    s_appName = PathFindFileName(appName);
    if (s_appName.find(L"ndk-") != std::wstring::npos) {
        s_oringalAppName = s_appName.substr(sizeof("ndk-") - 1);
    } 
    else {
        s_oringalAppName = s_appName;
    }
#if defined(_DEBUG)
    if (wcscmp(s_appName.c_str(), DEBUG_MODULE) == 0) {
        MessageBoxW(nullptr, wsls::sfmt(L"Install patch: wsLongPaths.dll for %s succeed.", appName).c_str(), L"Wating for debugger to attaching...", MB_OK | MB_ICONEXCLAMATION);
    }
#endif
}
