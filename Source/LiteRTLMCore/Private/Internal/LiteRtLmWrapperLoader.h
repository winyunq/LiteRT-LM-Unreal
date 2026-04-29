// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "litert_lm_wrapper.h"

/**
 * Helper to dynamically load the LiteRT-LM DLL and resolve function pointers.
 */
class FLiteRtLmWrapperLoader
{
public:
    static bool LoadDll();
    static void UnloadDll();

    // Function pointers
    typedef void* (*PN_CreateEngine)(const char*, const char*);
    typedef void (*PN_DestroyEngine)(void*);
    typedef void* (*PN_CreateConversation)(void*);
    typedef void (*PN_DestroyConversation)(void*);
    typedef void (*PN_AppendUserMessage)(void*, const char*);
    typedef void (*PN_AppendAssistantMessage)(void*, const char*);
    typedef void (*PN_RunInference)(void*, LiteRtLm_SamplingParams, LiteRtLmCallback, void*);
    typedef void (*PN_StopMessage)(void*);
    typedef const char* (*PN_GetAvailableBackends)();
    typedef float (*PN_GetVRAMUsage)(void*);

    static PN_CreateEngine CreateEngine;
    static PN_DestroyEngine DestroyEngine;
    static PN_CreateConversation CreateConversation;
    static PN_DestroyConversation DestroyConversation;
    static PN_AppendUserMessage AppendUserMessage;
    static PN_AppendAssistantMessage AppendAssistantMessage;
    static PN_RunInference RunInference;
    static PN_StopMessage StopMessage;
    static PN_GetAvailableBackends GetAvailableBackends;
    static PN_GetVRAMUsage GetVRAMUsage;

private:
    static void* DllHandle;
};
