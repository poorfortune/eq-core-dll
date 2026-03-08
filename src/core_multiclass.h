#pragma once

#include "MQ2Main.h"

// Runtime class IDs for the local player - set from a custom server packet handler
// for per-character support. Falls back to the static values in _options.h if 0.
BYTE g_secondaryClassID = 0;
BYTE g_tertiaryClassID  = 0;

// Static output buffers - single-threaded game client, these are safe
static char g_classDescBuf[64] = {0};
static char g_classCodeBuf[24] = {0};

// Helper: resolve effective secondary/tertiary IDs
static inline int GetSecondaryID() { return (g_secondaryClassID != 0) ? (int)g_secondaryClassID : secondaryClassID; }
static inline int GetTertiaryID()  { return (g_tertiaryClassID  != 0) ? (int)g_tertiaryClassID  : tertiaryClassID;  }

// -----------------------------------------------------------------------
// GetClassDesc detour
// Returns "Warrior/Cleric/Druid" style string for the local player
// -----------------------------------------------------------------------
char* __fastcall GetClassDesc_Trampoline(void* thisptr, void* edx, int classID);
char* __fastcall GetClassDesc_Detour(void* thisptr, void* edx, int classID)
{
    char* primary = GetClassDesc_Trampoline(thisptr, edx, classID);

    int secID = GetSecondaryID();
    int terID = GetTertiaryID();

    if (secID <= 0 && terID <= 0) return primary;

    char buf[64];
    strcpy_s(buf, sizeof(buf), primary);

    if (secID > 0 && secID != classID) {
        strcat_s(buf, sizeof(buf), "/");
        strcat_s(buf, sizeof(buf), GetClassDesc_Trampoline(thisptr, edx, secID));
    }
    if (terID > 0 && terID != classID && terID != secID) {
        strcat_s(buf, sizeof(buf), "/");
        strcat_s(buf, sizeof(buf), GetClassDesc_Trampoline(thisptr, edx, terID));
    }

    strcpy_s(g_classDescBuf, sizeof(g_classDescBuf), buf);
    return g_classDescBuf;
}
DETOUR_TRAMPOLINE_EMPTY(char* __fastcall GetClassDesc_Trampoline(void* thisptr, void* edx, int classID));

// -----------------------------------------------------------------------
// GetClassThreeLetterCode detour
// Returns "war/clr/dru" style combined code for the local player
// -----------------------------------------------------------------------
char* __fastcall GetClassThreeLetterCode_Trampoline(void* thisptr, void* edx, int classID);
char* __fastcall GetClassThreeLetterCode_Detour(void* thisptr, void* edx, int classID)
{
    char* primary = GetClassThreeLetterCode_Trampoline(thisptr, edx, classID);

    int secID = GetSecondaryID();
    int terID = GetTertiaryID();

    if (secID <= 0 && terID <= 0) return primary;

    char buf[24];
    strcpy_s(buf, sizeof(buf), primary);

    if (secID > 0 && secID != classID) {
        strcat_s(buf, sizeof(buf), "/");
        strcat_s(buf, sizeof(buf), GetClassThreeLetterCode_Trampoline(thisptr, edx, secID));
    }
    if (terID > 0 && terID != classID && terID != secID) {
        strcat_s(buf, sizeof(buf), "/");
        strcat_s(buf, sizeof(buf), GetClassThreeLetterCode_Trampoline(thisptr, edx, terID));
    }

    strcpy_s(g_classCodeBuf, sizeof(g_classCodeBuf), buf);
    return g_classCodeBuf;
}
DETOUR_TRAMPOLINE_EMPTY(char* __fastcall GetClassThreeLetterCode_Trampoline(void* thisptr, void* edx, int classID));

// -----------------------------------------------------------------------
// EQ_Character::CanUseItem detour
// If the item is usable by secondary or tertiary class, suppress the red highlight
// by returning 1 (usable) even when the primary class check fails.
//
// Requires EQ_Character__CanUseItem_x to be defined in eqgame.h.
// See the TODO comment there for how to locate the address.
// -----------------------------------------------------------------------
#ifdef EQ_Character__CanUseItem_x
int __fastcall CanUseItem_Trampoline(void* thisptr, void* edx, PCONTENTS pItem);
int __fastcall CanUseItem_Detour(void* thisptr, void* edx, PCONTENTS pItem)
{
    int result = CanUseItem_Trampoline(thisptr, edx, pItem);
    if (result != 0) return result;                  // primary class can already use it
    if (!pItem || !pItem->pItemInfo) return result;

    DWORD classes = pItem->pItemInfo->Classes;

    int secID = GetSecondaryID();
    if (secID > 0 && secID <= 16 && (classes & (1 << (secID - 1)))) return 1;

    int terID = GetTertiaryID();
    if (terID > 0 && terID <= 16 && (classes & (1 << (terID - 1)))) return 1;

    return result;
}
DETOUR_TRAMPOLINE_EMPTY(int __fastcall CanUseItem_Trampoline(void* thisptr, void* edx, PCONTENTS pItem));
#endif

// -----------------------------------------------------------------------
// Called from InitOptions() when isMulticlassDisplayEnabled is true
// -----------------------------------------------------------------------
void EnableMulticlassDisplay()
{
    EzDetour(CEverQuest__GetClassDesc_x,            GetClassDesc_Detour,            GetClassDesc_Trampoline);
    EzDetour(CEverQuest__GetClassThreeLetterCode_x, GetClassThreeLetterCode_Detour, GetClassThreeLetterCode_Trampoline);

#ifdef EQ_Character__CanUseItem_x
    DebugSpew("enabling multiclass CanUseItem hook");
    EzDetour(EQ_Character__CanUseItem_x, CanUseItem_Detour, CanUseItem_Trampoline);
#else
    DebugSpew("multiclass CanUseItem hook skipped - EQ_Character__CanUseItem_x not defined in eqgame.h");
#endif
}
