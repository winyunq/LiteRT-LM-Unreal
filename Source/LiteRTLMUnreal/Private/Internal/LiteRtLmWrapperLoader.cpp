// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"

void* FLiteRtLmWrapperLoader::DllHandle = nullptr;

FLiteRtLmWrapperLoader::PN_CreateEngine FLiteRtLmWrapperLoader::CreateEngine = nullptr;
FLiteRtLmWrapperLoader::PN_DestroyEngine FLiteRtLmWrapperLoader::DestroyEngine = nullptr;
FLiteRtLmWrapperLoader::PN_CreateConversation FLiteRtLmWrapperLoader::CreateConversation = nullptr;
FLiteRtLmWrapperLoader::PN_CreateConversationWithConfig FLiteRtLmWrapperLoader::CreateConversationWithConfig = nullptr;
FLiteRtLmWrapperLoader::PN_DestroyConversation FLiteRtLmWrapperLoader::DestroyConversation = nullptr;
FLiteRtLmWrapperLoader::PN_AppendUserMessage FLiteRtLmWrapperLoader::AppendUserMessage = nullptr;
FLiteRtLmWrapperLoader::PN_AppendMessageJson FLiteRtLmWrapperLoader::AppendMessageJson = nullptr;
FLiteRtLmWrapperLoader::PN_AppendAssistantMessage FLiteRtLmWrapperLoader::AppendAssistantMessage = nullptr;
FLiteRtLmWrapperLoader::PN_RunInference FLiteRtLmWrapperLoader::RunInference = nullptr;
FLiteRtLmWrapperLoader::PN_StopMessage FLiteRtLmWrapperLoader::StopMessage = nullptr;
FLiteRtLmWrapperLoader::PN_GetAvailableBackends FLiteRtLmWrapperLoader::GetAvailableBackends = nullptr;
FLiteRtLmWrapperLoader::PN_GetVRAMUsage FLiteRtLmWrapperLoader::GetVRAMUsage = nullptr;

bool FLiteRtLmWrapperLoader::LoadDll()
{
    if (DllHandle) return true;

    FString DllPath = FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("litert_lm_wrapper.dll"));

    if (!FPaths::FileExists(DllPath))
    {
        // Fallback to plugin directory if not in binaries (e.g. during development/unbuilt)
        // But since we use RuntimeDependencies, it should be in BaseDir.
        UE_LOG(LogTemp, Error, TEXT("LiteRT-LM DLL not found at: %s"), *DllPath);
        return false;
    }

    DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

    if (!DllHandle)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to load LiteRT-LM DLL: %s"), *DllPath);
        return false;
    }

    // Resolve Symbols
    CreateEngine = (PN_CreateEngine)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_CreateEngine"));
    DestroyEngine = (PN_DestroyEngine)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_DestroyEngine"));
    CreateConversation = (PN_CreateConversation)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_CreateConversation"));
    CreateConversationWithConfig = (PN_CreateConversationWithConfig)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_CreateConversationWithConfig"));
    DestroyConversation = (PN_DestroyConversation)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_DestroyConversation"));
    AppendUserMessage = (PN_AppendUserMessage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_AppendUserMessage"));
    AppendMessageJson = (PN_AppendMessageJson)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_AppendMessageJson"));
    AppendAssistantMessage = (PN_AppendAssistantMessage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_AppendAssistantMessage"));
    RunInference = (PN_RunInference)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_RunInference"));
    StopMessage = (PN_StopMessage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_StopMessage"));
    GetAvailableBackends = (PN_GetAvailableBackends)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_GetAvailableBackends"));
    GetVRAMUsage = (PN_GetVRAMUsage)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_GetVRAMUsage"));

    if (!CreateEngine || !RunInference)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to resolve key symbols in LiteRT-LM DLL."));
        UnloadDll();
        return false;
    }

    return true;
}

void FLiteRtLmWrapperLoader::UnloadDll()
{
    if (DllHandle)
    {
        FPlatformProcess::FreeDllHandle(DllHandle);
        DllHandle = nullptr;
    }

    CreateEngine = nullptr;
    DestroyEngine = nullptr;
    CreateConversation = nullptr;
    CreateConversationWithConfig = nullptr;
    DestroyConversation = nullptr;
    AppendUserMessage = nullptr;
    AppendMessageJson = nullptr;
    AppendAssistantMessage = nullptr;
    RunInference = nullptr;
    StopMessage = nullptr;
    GetAvailableBackends = nullptr;
    GetVRAMUsage = nullptr;
}
