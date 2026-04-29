// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmSubsystem.h"
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Misc/CoreDelegates.h"

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

    if (!FLiteRtLmWrapperLoader::CreateEngine) return false;

    CurrentConfig = InConfig;
    EngineHandle = FLiteRtLmWrapperLoader::CreateEngine(
        TCHAR_TO_UTF8(*CurrentConfig.ModelPath),
        TCHAR_TO_UTF8(*CurrentConfig.Backend)
    );

    return EngineHandle != nullptr;
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

    if (FLiteRtLmWrapperLoader::DestroyEngine)
    {
        FLiteRtLmWrapperLoader::DestroyEngine(EngineHandle);
    }
    EngineHandle = nullptr;
}

void* ULiteRtLmSubsystem::GetOrCreateSession(void* Ctx)
{
    if (!IsModelLoaded()) return nullptr;

    if (void** Found = SessionMap.Find(Ctx))
    {
        return *Found;
    }

    if (FLiteRtLmWrapperLoader::CreateConversation)
    {
        void* NewSession = FLiteRtLmWrapperLoader::CreateConversation(EngineHandle);
        if (NewSession)
        {
            SessionMap.Add(Ctx, NewSession);
            return NewSession;
        }
    }

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
    }
}
