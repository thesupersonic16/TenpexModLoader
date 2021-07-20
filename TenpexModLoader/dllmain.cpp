// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <string>
#include <vector>
#include <sstream>
#include <TenpexModLoader.h>
#include <CommonLoader.h>
#include <INIReader.h>
#include <detours.h>
#include "helpers.h"
#include "wars.h"
#include "Events.h"
#include "sigscanner.h"
#include <d3d11.h>
#include <chrono>

#pragma comment(linker, "/EXPORT:D3D11CreateDevice=C:\\Windows\\System32\\d3d11.D3D11CreateDevice")
#pragma comment(linker, "/EXPORT:D3D11CoreCreateDevice=C:\\Windows\\System32\\d3d11.D3D11CoreCreateDevice")
#pragma comment(linker, "/EXPORT:D3D11CreateDeviceAndSwapChain=C:\\Windows\\System32\\d3d11.D3D11CreateDeviceAndSwapChain")

#define FOREGROUND_WHITE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN)

#define DEFINE_SIGSCAN(NAME, BYTES, MASK) \
const char* _b##NAME = BYTES; \
const char* _m##NAME = MASK; \
size_t _a##NAME = 0;

#define DO_SIGSCAN(NAME) _a##NAME = SignatureScanner::FindSignature(BaseAddress, DetourGetModuleSize((HMODULE)BaseAddress), _b##NAME, _m##NAME);

#define LINK_SCAN(MAIN, SUB) if (_a##SUB && !_a##MAIN) _a##MAIN = _a##SUB;
#define CHECK_SCAN(NAME) \
PrintDebug("SIGSCAN: %s: %llX (%llX)", #NAME, _a##NAME, _a##NAME - BaseAddress + 0x140000000); \
if (!_a##NAME) MessageBoxA(NULL, "Could not find "###NAME"! The modloader may fail to load.", "Scan Error", NULL);



using std::string;
using std::wstring;
using std::vector;

vector<char*> ReplaceDirs;
std::map<string, string> FileCache;
bool ConsoleEnabled = false;
string* saveFilePath = new string();
bool useSaveFilePath = false;
HANDLE stdoutHandle = nullptr;
intptr_t BaseAddress = (intptr_t)GetModuleHandle(nullptr);
ModInfo* ModsInfo;
Game CurrentGame = Game_Unknown;

// Criware
DEFINE_SIGSCAN(criFsIoWin_Exists,         "\x48\x89\x5C\x24\x00\x57\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x84\x24\x00\x00\x00\x00\x48\x8B\xDA\x48\x8B\xF9\x48\x85\xC9", "xxxx?xxxx????xxx????xxxxxxx????xxxxxxxxx")
DEFINE_SIGSCAN(criFsIoWin_Exists2,        "\x48\x89\x5C\x24\x00\x57\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\xDA\x48\x8B\xF9\x48\x85\xC9\x74\x64\x48\x85\xD2\x74\x5F\x83\x3D\x00\x00\x00\x00\x00\x74\x38\xE8\x00", "xxxx?xxxx????xxxxxxxxxxxxxxxxxx?????xxx?")
DEFINE_SIGSCAN(crifsiowin_CreateFile,     "\x40\x53\x55\x56\x57\x41\x54\x41\x56\x41\x57\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x84\x24\x00\x00\x00\x00\x83\x3D\x00\x00", "xxxxxxxxxxxxxx????xxx????xxxxxxx????xx??")
DEFINE_SIGSCAN(crifsiowin_CreateFile2,    "\x48\x89\x5C\x24\x00\x48\x89\x6C\x24\x00\x48\x89\x74\x24\x00\x57\x48\x81\xEC\x00\x00\x00\x00\x83\x3D\x00\x00\x00\x00\x00\x49\x8B\xF9\x41\x8B\xF0\x8B\xEA\x48\x8B", "xxxx?xxxx?xxxx?xxxx????xx?????xxxxxxxxxx")
DEFINE_SIGSCAN(criErr_NotifyGeneric,      "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20\x41\x56\x48\x83\xEC\x30\x45\x33\xC9\x48\x8B\xFA\x4C\x39\x0D\x00\x00\x00\x00\x8B\xF1", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx????xx")
DEFINE_SIGSCAN(criErr_NotifyGeneric2,     "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x57\x41\x56\x41\x57\x48\x83\xEC\x30\x4C\x8D\x70\xD8\x4C\x8D\x78\xD8\x48\x8B\xEA\x8B\xF9\x4C\x89\x40", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx")
DEFINE_SIGSCAN(criFsBinder_BindDirectory, "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20\x41\x54\x41\x56\x41\x57\x48\x83\xEC\x30\x48\x8B\x74\x24\x00\x33\xED\x49\x8B\xD9\x4D", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx?xxxxxx")
DEFINE_SIGSCAN(criFsBinder_BindCpk,       "\x48\x83\xEC\x48\x48\x8B\x44\x24\x00\xC7\x44\x24\x00\x00\x00\x00\x00\x48\x89\x44\x24\x00\x8B\x44\x24\x70\x89\x44\x24\x20\xE8\x00\x00\x00\x00\x48\x83\xC4\x48\xC3", "xxxxxxxx?xxx?????xxxx?xxxxxxxxx????xxxxx")
DEFINE_SIGSCAN(criFsBinder_SetPriority,   "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x8B\xFA\xE8\x00\x00\x00\x00\x48\x8B\xD8\x48\x85\xC0\x75\x18\x8D\x58\xFE\x33\xC9\x44\x8B\xC3\x48\x8D\x15\x00\x00\x00\x00", "xxxx?xxxxxxxx????xxxxxxxxxxxxxxxxxxx????")
DEFINE_SIGSCAN(criFsBinder_SetPriority2,  "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x8B\xFA\xE8\x00\x00\x00\x00\x48\x8B\xD8\x48\x85\xC0\x75\x18\x8D\x58\xFE\x48\x8D\x15\x00\x00\x00\x00\x33\xC9\x44\x8B\xC3\xE8\x00\x00\x00\x00\x8B\xC3\xEB\x3E\x48\x83\x38\x00\x75\x13\x48\x8D\x15\x00\x00\x00\x00\x33\xC9\xE8\x00\x00\x00\x00\x83", "xxxx?xxxxxxxx????xxxxxxxxxxxxxx????xxxxxx????xxxxxxxxxxxxx????xxx????x")
DEFINE_SIGSCAN(criFsBinder_GetStatus,     "\x48\x89\x5C\x24\x00\x57\x48\x83\xEC\x20\x48\x8B\xDA\x8B\xF9\x85\xC9\x74\x36\x48\x85\xD2\x74\x3C\xE8\x00\x00\x00\x00\x48\x85\xC0\x75\x0A\xC7\x03\x00\x00\x00\x00", "xxxx?xxxxxxxxxxxxxxxxxxxx????xxxxxxx????")

// Engine
DEFINE_SIGSCAN(RunCore,                   "\x40\x53\x48\x83\xEC\x20\x48\x8B\xD9\x48\x8B\x0D\x00\x00\x00\x00\xE8\x00\x00\x00\x00\x48\x8B\x4B\x70\xE8\x00\x00\x00\x00\x48\x8B\x4B\x78\x48\x85\xC9\x74\x10\x48", "xxxxxxxxxxxx????x????xxxxx????xxxxxxxxxx")
DEFINE_SIGSCAN(RunCore2,                  "\x40\x53\x48\x83\xEC\x40\x48\x8B\x51\x28\x48\x8B\xD9\x48\x8B\x89\x00\x00\x00\x00\x48\x83\xC2\x04\xE8\x00\x00\x00\x00\x48\x8B\x43\x28\x80\x78\x19\x00\x74\x7A\x48", "xxxxxxxxxxxxxxxx????xxxxx????xxxxxxxxxxx")

void PrintError(const char* text, ...)
{
    if (!ConsoleEnabled)
        return;
    va_list ap;
    va_start(ap, text);
    if (!stdoutHandle)
        stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    char buffer[512];
    _vsprintf_p(buffer, 512, text, ap);
    SetConsoleTextAttribute(stdoutHandle, FOREGROUND_RED | FOREGROUND_INTENSITY);
    printf("%s%s\n", "[TenpexML] [ERROR] ", buffer);
    SetConsoleTextAttribute(stdoutHandle, FOREGROUND_WHITE);
    va_end(ap);
}

void PrintWarn(const char* text, ...)
{
    if (!ConsoleEnabled)
        return;
    va_list ap;
    va_start(ap, text);
    if (!stdoutHandle)
        stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    char buffer[512];
    _vsprintf_p(buffer, 512, text, ap);
    SetConsoleTextAttribute(stdoutHandle, FOREGROUND_YELLOW | FOREGROUND_INTENSITY);
    printf("%s%s\n", "[TenpexML] [WARN]  ", buffer);
    SetConsoleTextAttribute(stdoutHandle, FOREGROUND_WHITE);
    va_end(ap);
}

void PrintDebug(const char* text, ...)
{
    if (!ConsoleEnabled)
        return;
    va_list ap;
    va_start(ap, text);
    if (!stdoutHandle)
        stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    char buffer[512];
    _vsprintf_p(buffer, 512, text, ap);
    SetConsoleTextAttribute(stdoutHandle, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf("%s%s\n", "[TenpexML] [DEBUG] ", buffer);
    SetConsoleTextAttribute(stdoutHandle, FOREGROUND_WHITE);
    va_end(ap);
}

void PrintInfo(const char* text, ...)
{
    if (!ConsoleEnabled)
        return;
    va_list ap;
    va_start(ap, text);
    if (!stdoutHandle)
        stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    char buffer[512];
    _vsprintf_p(buffer, 512, text, ap);
    printf("%s%s\n", "[TenpexML] [INFO]  ", buffer);
    va_end(ap);
}

void InitMods();
void InitLoader();

HOOK(bool, __fastcall, SteamAPI_RestartAppIfNecessary, PROC_ADDRESS("steam_api64.dll", "SteamAPI_RestartAppIfNecessary"), uint32_t appid)
{
    originalSteamAPI_RestartAppIfNecessary(appid);
    std::ofstream ofs("steam_appid.txt");
    ofs << appid;
    ofs.close();
    CurrentGame = (Game)appid;
    PrintDebug("Game ID is %d", CurrentGame);
    InitLoader();
    InitMods();
    return false;
}

HOOK(bool, __fastcall, SteamAPI_IsSteamRunning, PROC_ADDRESS("steam_api64.dll", "SteamAPI_IsSteamRunning"))
{
    originalSteamAPI_IsSteamRunning();
    return true;
}

HOOK(void, __fastcall, SteamAPI_Shutdown, PROC_ADDRESS("steam_api64.dll", "SteamAPI_Shutdown"))
{
    RaiseEvents(modExitEvents);
    originalSteamAPI_Shutdown();
}

void GetModDirectoryFromCPKREDIR(char* buffer)
{
    PrintDebug("Loading CPKREDIR Config...");
    INIReader cpkredir("cpkredir.ini");
    auto str = cpkredir.GetString("CPKREDIR", "ModsDbIni", "mods\\ModsDB.ini");
    str = str.substr(0, str.find_last_of("\\"));
    strcpy_s(buffer, MAX_PATH, str.c_str());
}

bool CompareModCount(int id, int count, bool reverse)
{
    if (reverse)
        return id >= 0;
    else
        return id < count;
}
void InDecrease(int* num, bool decrease)
{
    if (decrease)
        (*num)--;
    else
        (*num)++;
}

const char* PathSubString(const char* text)
{
    if (CurrentGame != Game_Tenpex)
        return text + 5;
    const char* result = strstr(text, "raw");
    if (result)
        return result + 4;
    return text;
}

// TODO: Add caching for Tenpex
FastcallFunctionPointer(void, criError_NotifyGeneric, (CriErrorLevel level, const CriChar8* error_id, CriError error_no), _acriErr_NotifyGeneric);
HOOK(CriError, __fastcall, crifsiowin_CreateFile, _acrifsiowin_CreateFile, CriChar8* path, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, int dwFlagsAndAttributes, __int64 hTemplateFile)
{
    if (CurrentGame != Game_Tenpex)
    {
        string newPath = path + 5;
        std::transform(newPath.begin(), newPath.end(), newPath.begin(), ::tolower);
        auto it = FileCache.find(newPath);
        if (it != FileCache.end() && !it->second.empty())
        {
            const char* replacePath = it->second.c_str();
            PrintInfo("Loading File: %s", replacePath);
            strcpy(path, replacePath);
        }
    }
    else
    {
        // Mod Check
        DWORD attributes = -1;
        for (auto& value : ReplaceDirs)
        {
            string filePath = value;
            filePath += PathSubString(path);
            attributes = GetFileAttributesA(filePath.c_str());
            if (attributes != -1)
            {
                strcpy(path, filePath.c_str());
                PrintInfo("Loading File: %s", path);
                break;
            }
        }
    }
    return originalcrifsiowin_CreateFile(path, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

// TODO: Add caching for Tenpex
//DataPointer(bool, crifsiowin_utf8_path, ASLR(0x142E54748));
HOOK(CriError, __fastcall, criFsIoWin_Exists, _acriFsIoWin_Exists, CriChar8* path, bool* exists)
{
    if (CurrentGame != Game_Tenpex)
    {
        *exists = false;
        string newPath = path + 5;
        std::transform(newPath.begin(), newPath.end(), newPath.begin(), ::tolower);
        auto it = FileCache.find(newPath);
        if (it != FileCache.end())
        {
            strcpy(path, it->second.c_str());
            *exists = true;
        }
    }
    else
    {
        DWORD attributes = -1;
        for (auto& value : ReplaceDirs)
        {
            string filePath = value;
            filePath += PathSubString(path);
            attributes = GetFileAttributesA(filePath.c_str());
            if (attributes != -1)
            {
                strcpy(path, filePath.c_str());
                *exists = true;
                break;
            }
        }

        if (path && attributes == -1)
        {
            // TODO: Add proper UTF8 support
            //if (crifsiowin_utf8_path)
            //{
            //    WCHAR buffer[MAX_PATH];
            //    MultiByteToWideChar(65001, 0, path, strlen(path) + 1, buffer, MAX_PATH);
            //    attributes = GetFileAttributesW(buffer);
            //}
            //else
            //{
            attributes = GetFileAttributesA(path);
            //}
            *exists = attributes != -1 && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
        }
        else if (!*exists)
        {
            criError_NotifyGeneric(CRIERR_LEVEL_ERROR, "E2015091137", CRIERR_INVALID_PARAMETER);
            return CRIERR_NG;
        }
    }
    return originalcriFsIoWin_Exists(path, exists);
}

HOOK(void, __fastcall, criErr_NotifyGeneric, _acriErr_NotifyGeneric, CriErrorLevel level, const CriChar8* error_id, CriError error_no)
{
    std::string ss;
    ss.append("[criErr_NotifyGeneric] Level: %d - ");
    ss.append(error_id);
    if (level == CRIERR_LEVEL_WARNING)
        PrintWarn((char*)ss.c_str(), level);
    else
        PrintError((char*)ss.c_str(), level);
}

static bool directoryBinded = false;

FastcallFunctionPointer(CriError, criFsBinder_BindDirectory, (CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid), _acriFsBinder_BindDirectory);
FastcallFunctionPointer(CriError, criFsBinder_GetStatus, (CriFsBindId bndrid, CriFsBinderStatus* status), _acriFsBinder_GetStatus);
FastcallFunctionPointer(CriError, criFsBinder_SetPriority, (CriFsBindId bndrid, CriSint32 priority), _acriFsBinder_SetPriority);
HOOK(CriError, __fastcall, criFsBinder_BindCpk, _acriFsBinder_BindCpk, CriFsBinderHn bndrhn, CriFsBinderHn srcbndrhn, const CriChar8* path, void* work, CriSint32 worksize, CriFsBindId* bndrid)
{
    // Tenpex does not require binding
    if (!directoryBinded && CurrentGame != Game_Tenpex)
    {
        // Someone wants it to say wars
        PrintDebug("Binding Directory: \"wars\"");
        criFsBinder_BindDirectory(bndrhn, srcbndrhn, "wars", work, worksize, bndrid);
        CriFsBinderStatus status = CRIFSBINDER_STATUS_ANALYZE;
        while (status != CRIFSBINDER_STATUS_COMPLETE)
        {
            criFsBinder_GetStatus(*bndrid, &status);
            if (status == CRIFSBINDER_STATUS_ERROR)
                PrintError("Failed to bind! Mod loading may fail!");
            Sleep(10);
        }
        criFsBinder_SetPriority(*bndrid, 70000000);
        PrintDebug("Directory bind completed");
        directoryBinded = true;
    }
    PrintDebug("Binding CPK: \"%s\"", path);
    return originalcriFsBinder_BindCpk(bndrhn, srcbndrhn, path, work, worksize, bndrid);
}

HOOK(void*, __fastcall, RunCore, _aRunCore, void* a1)
{
    void* result = originalRunCore(a1);
    RaiseEvents(modFrameEvents);
    CommonLoader::CommonLoader::RaiseUpdates();
    return result;
}


void InitLoader()
{
    std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
    // Wars' signatures are different which to save scanning time, it is separated
    if (CurrentGame == Game_Wars)
    {
        DO_SIGSCAN(criFsIoWin_Exists2);
        DO_SIGSCAN(crifsiowin_CreateFile2);
        DO_SIGSCAN(criErr_NotifyGeneric2);
        DO_SIGSCAN(criFsBinder_BindDirectory);
        DO_SIGSCAN(criFsBinder_BindCpk);
        DO_SIGSCAN(criFsBinder_SetPriority2);
        DO_SIGSCAN(criFsBinder_GetStatus);
        DO_SIGSCAN(RunCore2);
    }
    else
    {
        DO_SIGSCAN(criFsIoWin_Exists);
        DO_SIGSCAN(crifsiowin_CreateFile);
        DO_SIGSCAN(criErr_NotifyGeneric);
        DO_SIGSCAN(criFsBinder_BindDirectory);
        DO_SIGSCAN(criFsBinder_BindCpk);
        DO_SIGSCAN(criFsBinder_SetPriority);
        DO_SIGSCAN(criFsBinder_GetStatus);
        DO_SIGSCAN(RunCore);
    }

    std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
    std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    PrintDebug("Scan completed in %d ms", diff.count());

    // Link scans
    LINK_SCAN(criErr_NotifyGeneric, criErr_NotifyGeneric2);
    LINK_SCAN(crifsiowin_CreateFile, crifsiowin_CreateFile2);
    LINK_SCAN(criFsIoWin_Exists, criFsIoWin_Exists2);
    LINK_SCAN(criFsBinder_SetPriority, criFsBinder_SetPriority2);
    LINK_SCAN(RunCore, RunCore2);

    // Check scans
    CHECK_SCAN(criFsIoWin_Exists);
    CHECK_SCAN(crifsiowin_CreateFile);
    CHECK_SCAN(criErr_NotifyGeneric);
    CHECK_SCAN(criFsBinder_BindDirectory);
    CHECK_SCAN(criFsBinder_BindCpk);
    CHECK_SCAN(criFsBinder_SetPriority);
    CHECK_SCAN(criFsBinder_GetStatus);
    CHECK_SCAN(RunCore);

    // Install all the needed hooks
    INSTALL_HOOK_SIG(crifsiowin_CreateFile);
    INSTALL_HOOK_SIG(criFsIoWin_Exists);
    INSTALL_HOOK_SIG(criErr_NotifyGeneric);
    INSTALL_HOOK_SIG(criFsBinder_BindCpk);
    INSTALL_HOOK_SIG(RunCore);

    // Update function pointers with the scan results
    UPDATE_FUNCTION_POINTER(criFsBinder_BindDirectory,  _acriFsBinder_BindDirectory);
    UPDATE_FUNCTION_POINTER(criFsBinder_GetStatus,      _acriFsBinder_GetStatus);
    UPDATE_FUNCTION_POINTER(criFsBinder_SetPriority,    _acriFsBinder_SetPriority);
    UPDATE_FUNCTION_POINTER(criError_NotifyGeneric,     _acriErr_NotifyGeneric);

    if (CurrentGame == Game_Wars)
        InitLoaderWars();
}

void indexInclude(string s, int rootIndex)
{
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA((s + "\\*").c_str(), &ffd);
    if (INVALID_HANDLE_VALUE == hFind)
        return;
    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (ffd.cFileName[0] == '.')
                continue;
            indexInclude(s + "\\" + ffd.cFileName, rootIndex);
        }
        else
        {
            string key = (s + "\\" + ffd.cFileName).substr(rootIndex);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            FileCache[key] = s + "\\" + ffd.cFileName;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);

}

void InitMods()
{
    PrintDebug("Loading ModsDB...");
    ModsInfo = new ModInfo();
    ModsInfo->ModList = new vector<Mod*>();
    ModsInfo->CurrentGame = CurrentGame;
    
    char modsDir[MAX_PATH];
    GetModDirectoryFromCPKREDIR(modsDir);
    INIReader ini((string(modsDir) + "\\ModsDB.ini").c_str());
    
    char pathbuf[MAX_PATH];
    GetModuleFileNameA(NULL, pathbuf, MAX_PATH);
    string exePath(pathbuf);
    string exeDir = exePath.substr(0, exePath.find_last_of("\\"));

    vector<ModInitEvent> postEvents;

    PrintInfo("Loading Mods...");
    int modCount = ini.GetInteger("Main", "ActiveModCount", 0);
    bool reverse = ini.GetBoolean("Main", "ReverseLoadOrder", false);

    if (reverse)
        PrintInfo("Mods will now be loaded in Reverse!");
    for (int i = (reverse ? (modCount - 1) : 0); CompareModCount(i, modCount, reverse); InDecrease(&i, reverse))
    {
        char key[14];
        snprintf(key, sizeof(key), "ActiveMod%u", i);

        auto modKey = ini.GetString("Main", string(key), "");
        if (modKey.empty())
        {
            PrintError("Invalid key mod detected in ModsDB! \"%s\"", key);
            continue;
        }

        string path = ini.GetString("Mods", modKey, "");
        string dir = path.substr(0, path.find_last_of("\\")) + "\\";
        std::replace(path.begin(), path.end(), '/', '\\');

        INIReader modConfig(path);

        auto errorLine = modConfig.ParseError();
        if (errorLine != 0)
        {
            PrintError("Failed to load mod at \"%s\"", path.c_str());
            PrintError("    at %d", errorLine);
        }
        else
        {
            const string mod_nameA = modConfig.GetString("Desc", "Title", "");
            PrintInfo("Loading Mod %d. %s", i, mod_nameA.c_str());

            int dirs = modConfig.GetInteger("Main", "IncludeDirCount", -1);
            if (dirs == -1)
                continue;

            for (unsigned int i = 0; i < dirs; i++)
            {
                char key2[14];
                snprintf(key2, sizeof(key2), "IncludeDir%u", i);
                auto path = modConfig.GetString("Main", string(key2), "");
                if (path.empty())
                    break;

                SetCurrentDirectoryA(dir.c_str());
                SetCurrentDirectoryA(path.c_str());
                char* buffer2 = new char[MAX_PATH];
                GetCurrentDirectoryA(MAX_PATH, buffer2);
                string* replacedir = new string(buffer2);
                if (CurrentGame == Game_Tenpex)
                    (*replacedir) += "\\raw\\";
                else if (CurrentGame == Game_Musashi)
                    (*replacedir) += "\\disk\\musashi_0\\";
                else if (CurrentGame == Game_Wars)
                    (*replacedir) += "\\disk\\wars_patch\\";
                else
                    (*replacedir) += "\\data\\";
                PrintDebug("    Added Include: %s", replacedir->c_str());
                ReplaceDirs.insert(ReplaceDirs.begin(), (char*)replacedir->c_str());
            }

            // Check if the mod has a DLL file.
            auto dllFile = modConfig.GetString("Main", "DLLFile", "");
            if (!dllFile.empty())
            {
                SetDllDirectoryA(dir.c_str());
                SetCurrentDirectoryA(dir.c_str());
                std::stringstream stream(dllFile);
                string dllName;
                while (std::getline(stream, dllName, ','))
                {
                    if (ConsoleEnabled)
                        PrintInfo("    Loading DLL: %s", dllName.c_str());
                    HMODULE module = LoadLibraryA((dir + dllName).c_str());
                    if (module)
                    {
                        ModInitEvent init = (ModInitEvent)GetProcAddress(module, "Init");
                        ModInitEvent postInit = (ModInitEvent)GetProcAddress(module, "PostInit");

                        if (init)
                            init(ModsInfo);
                        if (postInit)
                            postEvents.push_back(postInit);

                        RegisterEvent(modFrameEvents, module, "OnFrame");
                        RegisterEvent(modExitEvents, module, "OnExit");
                    }
                    else
                    {
                        DWORD error = GetLastError();
                        LPSTR msgBuffer = nullptr;

                        DWORD msgSize = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msgBuffer, 0, NULL);

                        std::string msg = "Failed to load " + dllName + "\n" + std::string(msgBuffer, msgSize);
                        MessageBoxA(NULL, msg.c_str(), "TenpexModLoader", MB_OK);

                        LocalFree(msgBuffer);
                    }
                }
            }
        }
        SetCurrentDirectoryA(exeDir.c_str());
        for (ModInitEvent event : postEvents)
            event(ModsInfo);
    }

    for (auto& value : ReplaceDirs)
    {
        auto root = string(value).substr(0, strlen(value) - 1);
        indexInclude(root, root.length() + 1);
    }


    // Init CommonLoader
    PrintInfo("Loading Codes...");
    CommonLoader::CommonLoader::InitializeAssemblyLoader((string(modsDir) + "\\Codes.dll").c_str());
    CommonLoader::CommonLoader::RaiseInitializers();

    PrintDebug("InitMods() Completed");
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    INIReader cpkredir;

    bool useFileLogging = false;
    string logType, logFile, saveFileFallback, saveFileOverride;
    long enableSaveFileRedirection;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        cpkredir = INIReader("cpkredir.ini");

        logType = cpkredir.GetString("CPKREDIR", "LogType", "");
        logFile = cpkredir.GetString("CPKREDIR", "LogFile", "cpkredir.log");
        saveFileFallback = cpkredir.GetString("CPKREDIR", "SaveFileFallback", "");
        saveFileOverride = cpkredir.GetString("CPKREDIR", "SaveFileOverride", "");
        enableSaveFileRedirection = cpkredir.GetInteger("CPKREDIR", "EnableSaveFileRedirection", -1);

        if (!logType.empty())
        {
            ConsoleEnabled = !strcmp(logType.c_str(), "console");
            useFileLogging = !strcmp(logType.c_str(), "file");
        }

        if (ConsoleEnabled)
        {
            AllocConsole();
            freopen("CONOUT$", "w", stdout);
        }
        if (useFileLogging)
        {
            freopen(logFile.c_str(), "w", stdout);
            ConsoleEnabled = true;
        }

        if (!saveFileFallback.empty())
        {
            saveFilePath->clear();
            saveFilePath->append(saveFileFallback);
        }

        if (!saveFileOverride.empty())
        {
            saveFilePath->clear();
            saveFilePath->append(saveFileOverride);
        }

        if (enableSaveFileRedirection != -1)
            useSaveFilePath = enableSaveFileRedirection != 0;

        PrintInfo("Starting TenpexModLoader %s...", "v2.0");
        INSTALL_HOOK(SteamAPI_RestartAppIfNecessary);
        INSTALL_HOOK(SteamAPI_IsSteamRunning);
        INSTALL_HOOK(SteamAPI_Shutdown);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

