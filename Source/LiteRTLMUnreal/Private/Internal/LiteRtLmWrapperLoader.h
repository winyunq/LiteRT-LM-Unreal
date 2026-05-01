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

    // Function pointers – signatures must match litert_lm_wrapper.h exactly
    typedef void* (*PN_CreateEngine)(LiteRtLm_Config);
    typedef void (*PN_DestroyEngine)(void*);
    typedef void* (*PN_CreateConversation)(void*);
    typedef void* (*PN_CreateConversationWithConfig)(void*, const char*, int);
    typedef void (*PN_DestroyConversation)(void*);
    typedef void (*PN_AppendUserMessage)(void*, const char*);
    typedef void (*PN_AppendMessageJson)(void*, const char*);
    typedef void (*PN_AppendAssistantMessage)(void*, const char*);
    typedef void (*PN_RunInference)(void*, LiteRtLm_SamplingParams, LiteRtLmCallback, void*);
    typedef void (*PN_StopMessage)(void*);
    typedef int (*PN_WaitUntilDone)(void*, int);
    typedef const char* (*PN_GetAvailableBackends)();

    static PN_CreateEngine CreateEngine;
    static PN_DestroyEngine DestroyEngine;
    static PN_CreateConversation CreateConversation;
    static PN_CreateConversationWithConfig CreateConversationWithConfig;
    static PN_DestroyConversation DestroyConversation;
    static PN_AppendUserMessage AppendUserMessage;
    static PN_AppendMessageJson AppendMessageJson;
    static PN_AppendAssistantMessage AppendAssistantMessage;
    static PN_RunInference RunInference;
    static PN_StopMessage StopMessage;
    static PN_WaitUntilDone WaitUntilDone;
    static PN_GetAvailableBackends GetAvailableBackends;

private:
    static void* DllHandle;
};
