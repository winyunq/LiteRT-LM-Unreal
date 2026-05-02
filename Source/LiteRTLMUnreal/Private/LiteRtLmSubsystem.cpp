// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Misc/CoreDelegates.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dxgi1_4.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma comment(lib, "dxgi.lib")
#endif

void ULiteRtLmSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FLiteRtLmWrapperLoader::LoadDll();
}

void ULiteRtLmSubsystem::Deinitialize()
{
    UnloadModel();
    FLiteRtLmWrapperLoader::UnloadDll();
    Super::Deinitialize();
}

bool ULiteRtLmSubsystem::LoadModel(const FLiteRtLmConfig& InConfig)
{
    if (IsModelLoaded()) UnloadModel();

    if (!FLiteRtLmWrapperLoader::CreateEngine)
    {
        UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] CreateEngine function pointer is null. DLL not loaded?"));
        return false;
    }

    CurrentConfig = InConfig;

    // Construct the C-style config struct matching DLL's LiteRtLm_Config
    LiteRtLm_Config CConfig;
    FTCHARToUTF8 Utf8ModelPath(*CurrentConfig.ModelPath);
    FTCHARToUTF8 Utf8Backend(*CurrentConfig.Backend);

    CConfig.model_path      = Utf8ModelPath.Get();
    CConfig.backend         = Utf8Backend.Get();
    CConfig.max_num_tokens  = CurrentConfig.MaxNumTokens;
    CConfig.num_threads     = CurrentConfig.NumThreads;
    CConfig.bEnableBenchmark = CurrentConfig.bEnableBenchmark ? 1 : 0;
    CConfig.bOptimizeShader  = CurrentConfig.bOptimizeShader ? 1 : 0;

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Loading model: Path=%s, Backend=%s, MaxTokens=%d, Threads=%d"),
        *CurrentConfig.ModelPath, *CurrentConfig.Backend,
        CurrentConfig.MaxNumTokens, CurrentConfig.NumThreads);

    EngineHandle = FLiteRtLmWrapperLoader::CreateEngine(CConfig);

    if (!EngineHandle)
    {
        UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] CreateEngine returned nullptr. Model path may be invalid or GPU backend unavailable."));
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Model loaded successfully. EngineHandle=%p"), EngineHandle);
    return true;
}

void ULiteRtLmSubsystem::UnloadModel()
{
    if (!EngineHandle) return;

    // Release all sessions first
    for (auto& It : SessionMap)
    {
        if (It.Value && FLiteRtLmWrapperLoader::DestroyConversation)
        {
            FLiteRtLmWrapperLoader::DestroyConversation(It.Value);
        }
    }
    SessionMap.Empty();
    SessionMsgCountMap.Empty();
    SessionToolsMap.Empty();

    if (FLiteRtLmWrapperLoader::DestroyEngine)
    {
        FLiteRtLmWrapperLoader::DestroyEngine(EngineHandle);
    }
    EngineHandle = nullptr;
}

void* ULiteRtLmSubsystem::GetOrCreateSession(void* Ctx, const FString& ToolsJson)
{
    if (!IsModelLoaded()) return nullptr;

    // Check if session exists and tools haven't changed
    if (void** Found = SessionMap.Find(Ctx))
    {
        const FString* ExistingTools = SessionToolsMap.Find(Ctx);
        if (ExistingTools && *ExistingTools == ToolsJson)
        {
            return *Found;
        }
        // Tools changed – destroy old session and create new one
        if (FLiteRtLmWrapperLoader::DestroyConversation)
        {
            FLiteRtLmWrapperLoader::DestroyConversation(*Found);
        }
        SessionMap.Remove(Ctx);
        SessionMsgCountMap.Remove(Ctx);
        SessionToolsMap.Remove(Ctx);
    }

    void* NewSession = nullptr;

    if (!ToolsJson.IsEmpty() && FLiteRtLmWrapperLoader::CreateConversationWithConfig)
    {
        // Build json_preface with tools
        FString JsonPreface = FString::Printf(TEXT("{\"tools\":%s}"), *ToolsJson);
        FTCHARToUTF8 Utf8Preface(*JsonPreface);
        NewSession = FLiteRtLmWrapperLoader::CreateConversationWithConfig(
            EngineHandle,
            Utf8Preface.Get(),
            1  // Enable constrained decoding for tool-aware sessions
        );
    }
    else if (FLiteRtLmWrapperLoader::CreateConversation)
    {
        NewSession = FLiteRtLmWrapperLoader::CreateConversation(EngineHandle);
    }

    if (NewSession)
    {
        SessionMap.Add(Ctx, NewSession);
        SessionMsgCountMap.Add(Ctx, 0);
        SessionToolsMap.Add(Ctx, ToolsJson);
        return NewSession;
    }

    UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] Failed to create conversation session."));
    return nullptr;
}

void ULiteRtLmSubsystem::ReleaseSession(void* Ctx)
{
    if (void** Found = SessionMap.Find(Ctx))
    {
        if (*Found && FLiteRtLmWrapperLoader::DestroyConversation)
        {
            FLiteRtLmWrapperLoader::DestroyConversation(*Found);
        }
        SessionMap.Remove(Ctx);
        SessionMsgCountMap.Remove(Ctx);
        SessionToolsMap.Remove(Ctx);
    }
}

int32 ULiteRtLmSubsystem::GetSessionMsgCount(void* Ctx) const
{
    const int32* Count = SessionMsgCountMap.Find(Ctx);
    return Count ? *Count : 0;
}

void ULiteRtLmSubsystem::SetSessionMsgCount(void* Ctx, int32 Count)
{
    SessionMsgCountMap.Add(Ctx, Count);
}

int32 ULiteRtLmSubsystem::QueryAvailableVramMB(int32 DefaultMB)
{
#if PLATFORM_WINDOWS
    IDXGIFactory4* DxgiFactory = nullptr;
    HRESULT Hr = CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&DxgiFactory);
    if (FAILED(Hr) || !DxgiFactory)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LiteRtLm] DXGI query failed, using default %d MB"), DefaultMB);
        return DefaultMB;
    }

    IDXGIAdapter3* Adapter = nullptr;
    Hr = DxgiFactory->EnumAdapters(0, reinterpret_cast<IDXGIAdapter**>(&Adapter));
    if (FAILED(Hr) || !Adapter)
    {
        DxgiFactory->Release();
        return DefaultMB;
    }

    // Re-query as IDXGIAdapter3 for QueryVideoMemoryInfo
    IDXGIAdapter3* Adapter3 = nullptr;
    Hr = Adapter->QueryInterface(__uuidof(IDXGIAdapter3), (void**)&Adapter3);
    Adapter->Release();
    if (FAILED(Hr) || !Adapter3)
    {
        DxgiFactory->Release();
        return DefaultMB;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO MemInfo = {};
    Hr = Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &MemInfo);
    Adapter3->Release();
    DxgiFactory->Release();

    if (FAILED(Hr))
    {
        return DefaultMB;
    }

    const int64 AvailableBytes = static_cast<int64>(MemInfo.Budget) - static_cast<int64>(MemInfo.CurrentUsage);
    const int32 AvailableMB = static_cast<int32>(FMath::Max(AvailableBytes, (int64)0) / (1024 * 1024));

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] DXGI VRAM: Budget=%llu MB, Used=%llu MB, Available=%d MB"),
        MemInfo.Budget / (1024 * 1024), MemInfo.CurrentUsage / (1024 * 1024), AvailableMB);

    return AvailableMB;
#else
    return DefaultMB;
#endif
}
