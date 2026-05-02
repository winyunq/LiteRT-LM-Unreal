// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"

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
FLiteRtLmWrapperLoader::PN_WaitUntilDone FLiteRtLmWrapperLoader::WaitUntilDone = nullptr;
FLiteRtLmWrapperLoader::PN_GetAvailableBackends FLiteRtLmWrapperLoader::GetAvailableBackends = nullptr;

/**
 * Pre-load dependency DLLs from the same directory as the main wrapper DLL.
 */
static void PreloadDependencyDlls(const FString& DllDir)
{
    static const TCHAR* DepNames[] = {
        TEXT("dxcompiler.dll"),
        TEXT("dxil.dll"),
        TEXT("libLiteRt.dll"),
        TEXT("libLiteRtWebGpuAccelerator.dll"),
        TEXT("libLiteRtTopKWebGpuSampler.dll"),
        TEXT("libGemmaModelConstraintProvider.dll"),
    };

    for (const TCHAR* DepName : DepNames)
    {
        FString DepPath = FPaths::Combine(DllDir, DepName);
        if (FPaths::FileExists(DepPath))
        {
            void* H = FPlatformProcess::GetDllHandle(*DepPath);
            UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Pre-load %s: %s"), DepName, H ? TEXT("OK") : TEXT("FAILED"));
        }
    }
}

/**
 * Recursively search for a file within a directory.
 */
static FString FindFileRecursive(const FString& StartDir, const FString& TargetFileName)
{
    TArray<FString> FoundFiles;
    IFileManager::Get().FindFilesRecursive(FoundFiles, *StartDir, *TargetFileName, true, false, false);
    
    for (const FString& FilePath : FoundFiles)
    {
        // Ignore files in Intermediate or metadata directories
        if (!FilePath.Contains(TEXT("Intermediate")) && !FilePath.Contains(TEXT(".git")))
        {
            return FilePath;
        }
    }
    return TEXT("");
}

bool FLiteRtLmWrapperLoader::LoadDll()
{
    if (DllHandle) return true;

    const FString TargetDllName = TEXT("litert_lm_wrapper.dll");

    // 1. Primary path: BaseDir (Standard for Packaged builds or after UBT staging)
    FString DllPath = FPaths::Combine(FPlatformProcess::BaseDir(), TargetDllName);

    if (!FPaths::FileExists(DllPath))
    {
        UE_LOG(LogTemp, Warning, TEXT("[LiteRtLm] DLL not found at BaseDir: %s. Performing global adaptive search..."), *DllPath);

        // 2. Adaptive Search: Scan project and engine plugins directories
        // This removes dependency on a specific plugin name ("LiteRT-LM-Unreal")
        TArray<FString> SearchRoots;
        SearchRoots.Add(FPaths::ProjectPluginsDir());
        SearchRoots.Add(FPaths::EnginePluginsDir());

        for (const FString& Root : SearchRoots)
        {
            DllPath = FindFileRecursive(Root, TargetDllName);
            if (!DllPath.IsEmpty()) break;
        }

        if (DllPath.IsEmpty() || !FPaths::FileExists(DllPath))
        {
            UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] DLL not found in any plugin directory."));
            return false;
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Loading DLL from: %s"), *DllPath);

    // Pre-load dependency DLLs from the same directory to resolve implicit linking
    PreloadDependencyDlls(FPaths::GetPath(DllPath));

    DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

    if (!DllHandle)
    {
        UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] GetDllHandle failed for: %s (Check if dependencies are missing)"), *DllPath);
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
    WaitUntilDone = (PN_WaitUntilDone)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_WaitUntilDone"));
    GetAvailableBackends = (PN_GetAvailableBackends)FPlatformProcess::GetDllExport(DllHandle, TEXT("LiteRtLm_GetAvailableBackends"));

    if (!CreateEngine || !RunInference)
    {
        UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] Failed to resolve key symbols."));
        UnloadDll();
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] DLL loaded. All symbols resolved."));
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
    WaitUntilDone = nullptr;
    GetAvailableBackends = nullptr;
}
