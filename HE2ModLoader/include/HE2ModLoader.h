#pragma once
#include <cstdio>
#include <fstream>
#include <vector>
#include <Shlwapi.h>

#define PATH_LIMIT 0x400

#define DataPointer(type, name, address) \
	static type &name = *(type *)address

#define UPDATE_FUNCTION_POINTER(NAME, ADDRESS) \
	*((void**)&NAME) = (void*)ADDRESS

#define DEFINE_SIGSCAN(NAME, BYTES, MASK) \
const char* _b##NAME = BYTES; \
const char* _m##NAME = MASK; \
size_t _a##NAME = 0;

#define DO_SIGSCAN(NAME) _a##NAME = SignatureScanner::FindSignature(BaseAddress, DetourGetModuleSize((HMODULE)BaseAddress), _b##NAME, _m##NAME);
#define LINK_SCAN(MAIN, SUB) if (_a##SUB && !_a##MAIN) _a##MAIN = _a##SUB;
#define CHECK_SCAN(NAME) \
PrintDebug("SIGSCAN: %s: %llX (%llX)", #NAME, _a##NAME, _a##NAME - BaseAddress + 0x140000000); \
if (!_a##NAME) MessageBoxA(NULL, "Could not find "###NAME"! The modloader may fail to load.", "Scan Error", NULL);

#define CHECK_SCAN_OPT(NAME) \
if (_a##NAME) PrintDebug("SIGSCAN: %s: %llX (%llX)", #NAME, _a##NAME, _a##NAME - BaseAddress + 0x140000000); \


static inline bool FileExists(const char* fileName)
{
    return GetFileAttributesA(fileName) != -1;
}

static inline bool DirExists(const std::string& dirName_in)
{
    DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
    if (ftyp == INVALID_FILE_ATTRIBUTES)
        return false;
    if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
        return true;
    return false;
}

enum Game
{
    Game_Unknown = 0,
    Game_Wars    = 637100,
    Game_Musashi = 981890,
    Game_Tenpex  = 1259790,
    Game_P5R     = 1687950
};

struct Mod
{
    const char* Name;
    const char* Path;
};

struct ModInfo
{
    std::vector<Mod*>* ModList;
    Mod* CurrentMod;
    Game CurrentGame;
};

typedef void(__cdecl* ModInitEvent)(ModInfo* modInfo);
typedef void(__cdecl* ModCallEvent)();

// IDA Types
typedef uint64_t _QWORD;
typedef uint32_t _DWORD;
typedef uint16_t _WORD;
typedef uint8_t _BYTE;