#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <unistd.h>
#include "substrate.h"
#include "memUtils.h"
#include "PVZ2ExampleMod.h"
#include "SexyTypes.h"
#include "PvZ2/Board.h"


#pragma region Alias to ID

class ZombieAlmanac
{
public:
    void* vftable;
    std::map<SexyString, uint> m_aliasToId;
};

class PlantNameMapper
{
public:
    void* vftable;
    std::map<SexyString, uint> m_aliasToId;
};

typedef ZombieAlmanac* (*ZombieAlmanacCtor)(ZombieAlmanac*);
ZombieAlmanacCtor oZombieAlmanacCtor = NULL;

void* hkCreateZombieTypenameMap(ZombieAlmanac* a1)
{
    // Let the game create the original alias->id map
    ZombieAlmanac* obj = oZombieAlmanacCtor(a1);
    // Now add our own zombie aliases to it
    obj->m_aliasToId["placeholder_armor2"] = 446;
    return obj;
}


typedef PlantNameMapper* (*PlantNameMapperCtor)(PlantNameMapper*);
PlantNameMapperCtor oPlantNameMapperCtor = NULL;

void* hkCreatePlantNameMapper(PlantNameMapper* self)
{
    // Same deal with the ZombieAlamanc::ctor hook
    PlantNameMapper* obj = oPlantNameMapperCtor(self);
    // Register our own plant aliases
    obj->m_aliasToId["gatlingpea"] = 191;
    return obj;
}


#pragma endregion

#pragma region Mummy Memory Fix

// The original function is broken for some reason. We don't need it
uint oCamelZombieFunc = NULL;

typedef void(*camelMinigameModuleFunc)(int, int, int);
camelMinigameModuleFunc cmmFunc = (camelMinigameModuleFunc)getActualOffset(0x7D4DA8);

void hkCamelZombieFunc(int a1, int a2, int a3)
{
    // Redirect call to some function in CamelMinigameModule
    // This fixes the crash when camels are rising from the ground
    cmmFunc(a1, a2, a3);
}

#pragma endregion

#pragma region Piano Zombie List

typedef bool (*initZombiePianoList)(int, int);
initZombiePianoList oInitZombiePianoList = NULL;

std::vector<SexyString>* g_pianoList = NULL;
bool g_pianoListInitialized = false;

bool hkInitZombiePianoList(int a1, int a2)
{
    // This function is called every frame when a piano zombie is on screen
    // So this global bool is needed to prevent wasting a massive amount of cpu time
    if (!g_pianoListInitialized)
    {
        bool orig = oInitZombiePianoList(a1, a2);

        uint ptrAddr = getActualOffset(0x1E5429C); // address of piano zombie's list in memory
        g_pianoList = reinterpret_cast<std::vector<SexyString>*>(ptrAddr);

        // @todo: add this to piano zombie's props instead?
        g_pianoList->clear();
        g_pianoList->push_back("cowboy");
        g_pianoList->push_back("cowboy_armor1");
        g_pianoList->push_back("cowboy_armor2");
        g_pianoList->push_back("cowboy_armor4");
        g_pianoListInitialized = true;
    }
    return oInitZombiePianoList(a1, a2);
}

#pragma endregion

#pragma region Board Zoom

int gWidth = 0;
int gHeight = 0;

enum AspectRatio
{
    Letterbox,
    Widescreen,
    Ultrawide,
};

typedef int(*worldMapDoMovement)(void*, float, float, bool);
worldMapDoMovement oWorldMapDoMovement = NULL;

int hkWorldMapDoMovement(void* self, float fX, float fY, bool allowVerticalMovement)
{
    return oWorldMapDoMovement(self, fX, fY, true);
}

AspectRatio GetAspectRatio()
{
    float ratio = (float)gWidth / (float)gHeight;
    if (ratio <= 1.4f)
    {
        return Letterbox;
    }
    else if (ratio >= 1.41f && ratio <= 1.85f)
    {
        return Widescreen;
    }
    else if (ratio >= 1.86f)
    {
        return Ultrawide;
    }
}

typedef void(*ReinitForSurfaceChange)(int, int, int, int, int);
ReinitForSurfaceChange oRFSC = nullptr;

void HkReinitForSurfaceChange(int thisptr, int a2, int width, int height, int a5)
{
    gWidth = width;
    gHeight = height;
    return oRFSC(thisptr, a2, width, height, a5);
}

typedef void* (*boardCtor)(void*);
boardCtor oBoardCtor = NULL;

void* hkBoardCtor(Board* board)
{
    oBoardCtor(board);

    board->m_lawnRect.mX = 200; //200
    board->m_lawnRect.mY = 160; //160

    switch (GetAspectRatio())
    {
    case Letterbox:
    {
        board->m_lawnRect.mWidth = 576; //576
        board->m_lawnRect.mHeight = 500; //380
        break;
    }
    case Widescreen:
    {
        board->m_lawnRect.mWidth = 576; //576
        board->m_lawnRect.mHeight = 450; //380
        break;
    }
    case Ultrawide:
    {
        board->m_lawnRect.mX = 450;
        board->m_lawnRect.mY = 160;
        board->m_lawnRect.mWidth = 576; //576
        board->m_lawnRect.mHeight = 525; //380
        break;
    }
    }

    return board;
}

#pragma endregion

#pragma region Dumb Hardcoded Immunities (Healer/Magician)

// we don't need to save a pointer to the original functions
uint dispose;

void hkMagicianHealerConditionFunc(int a1, int condition)
{
    // Hardcoded immunity checks would go here
    return;
}

bool hkMagicianHealerImmuneToShrink(int a1)
{
    // true = immune to shrinking
    return false;
}

bool hkMagicianInitializeFamilyImmunities(int a1, int64_t a2)
{
    typedef bool(*zFamilyFunc)(int);
    zFamilyFunc func = (zFamilyFunc)getActualOffset(0x905110); // function 93 in Zombie's vftable 
    return func(a1);
}

#pragma endregion

__attribute__((constructor))
// This is automatically executed when the lib is loaded
// Run your initialization code here
void libPVZ2ExampleMod_main()
{
    // Function hooks
    PVZ2HookFunction(0x105123C, (void*)hkCreateZombieTypenameMap, (void**)&oZombieAlmanacCtor, "ZombieAlmanac::ZombieAlamanc");
    PVZ2HookFunction(0xD994B8, (void*)hkCreatePlantNameMapper, (void**)&oPlantNameMapperCtor, "PlantNameMapper::PlantNameMapper");
    PVZ2HookFunction(0x7942BC, (void*)hkCamelZombieFunc, (void**)&oCamelZombieFunc, "CamelZombie::vftable_func_0xEC");
    PVZ2HookFunction(0x890108, (void*)hkInitZombiePianoList, (void**)&oInitZombiePianoList, "ZombiePiano::getTypenameList");
    PVZ2HookFunction(0x127415C, (void*)HkReinitForSurfaceChange, (void**)&oRFSC, "ReinitForSurfaceChanged");
    PVZ2HookFunction(0x71A004, (void*)hkBoardCtor, (void**)&oBoardCtor, "Board::Board");
    PVZ2HookFunction(0x884B6C, (void*)hkMagicianHealerImmuneToShrink, (void**)&dispose, "ZombieCarnieMagician::IsImmuneToShrink");
    PVZ2HookFunction(0x881C4C, (void*)hkMagicianHealerImmuneToShrink, (void**)&dispose, "ZombieRomanHealer::IsImmuneToShrink");
    PVZ2HookFunction(0x86CCAC, (void*)hkMagicianHealerConditionFunc, (void**)&dispose, "ZombieCarnieMagician::ConditionFunc");
    PVZ2HookFunction(0x84EAA0, (void*)hkMagicianHealerConditionFunc, (void**)&dispose, "ZombieRomanHealer::ConditionFunc");
    PVZ2HookFunction(0x86CCC0, (void*)hkMagicianInitializeFamilyImmunities, (void**)&dispose, "ZombieRomanHealer::InitializeFamilyImmunities");
    PVZ2HookFunction(0x44E604, (void*)hkWorldMapDoMovement, (void**)&oWorldMapDoMovement, "WorldMap::doMovement");

    LOGI("Finished initializing");
}