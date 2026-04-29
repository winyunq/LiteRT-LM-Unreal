// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Async/Async.h"

FLiteRtLmConfig FLiteRtLmUnrealApi::GetAutoConfig(int32 TargetVramMB)
{
    FLiteRtLmConfig Config;
    // Basic heuristics
    if (TargetVramMB < 2048) {
        Config.MaxNumTokens = 1024;
        Config.Backend = TEXT("cpu");
    } else {
        Config.MaxNumTokens = 2048;
        Config.Backend = TEXT("gpu");
    }
    return Config;
}

bool FLiteRtLmUnrealApi::LoadModel(const FLiteRtLmConfig& Config)
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>())
    {
        return Subsystem->LoadModel(Config);
    }
    return false;
}

void FLiteRtLmUnrealApi::UnloadModel()
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>())
    {
        Subsystem->UnloadModel();
    }
}

// Internal helper for C-style callbacks
struct FLiteRtLmCallbackContext
{
    FLiteRtLmChunkCallback OnChunk;
    FLiteRtLmDoneCallback OnDone;
    FString FullResponse;
};

static void Internal_LiteRtLmCallback(LiteRtLm_Result Result, void* UserPtr)
{
    FLiteRtLmCallbackContext* Ctx = static_cast<FLiteRtLmCallbackContext*>(UserPtr);
    if (!Ctx) return;

    if (Result.text_chunk)
    {
        FString Chunk = UTF8_TO_TCHAR(Result.text_chunk);
        Ctx->FullResponse += Chunk;
        
        // Use AsyncTask to push to GameThread
        FLiteRtLmChunkCallback ChunkCopy = Ctx->OnChunk;
        AsyncTask(ENamedThreads::GameThread, [ChunkCopy, Chunk]() {
            ChunkCopy.ExecuteIfBound(Chunk);
        });
    }

    if (Result.bIsDone)
    {
        FLiteRtLmResult FinalResult;
        FinalResult.FullText = Ctx->FullResponse;
        FinalResult.ErrorMsg = UTF8_TO_TCHAR(Result.error_msg);
        FinalResult.TokensPerSec = Result.tokens_per_sec;

        FLiteRtLmDoneCallback DoneCopy = Ctx->OnDone;
        AsyncTask(ENamedThreads::GameThread, [DoneCopy, FinalResult, Ctx]() {
            DoneCopy.ExecuteIfBound(FinalResult);
            delete Ctx; // Lifecycle ends here
        });
    }
}

void FLiteRtLmUnrealApi::ChatWithPrompt(void* Ctx, const FString& SystemPrompt, const FString& UserMsg, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone)
{
    ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();
    if (!Subsystem) return;

    void* Session = Subsystem->GetOrCreateSession(Ctx);
    if (!Session) return;

    // Implementation of ChatWithPrompt via Appends
    if (!SystemPrompt.IsEmpty())
    {
        FLiteRtLmWrapperLoader::AppendAssistantMessage(Session, TCHAR_TO_UTF8(*(TEXT("System: ") + SystemPrompt)));
    }
    
    FLiteRtLmWrapperLoader::AppendUserMessage(Session, TCHAR_TO_UTF8(*UserMsg));

    FLiteRtLmCallbackContext* CallbackCtx = new FLiteRtLmCallbackContext{ OnChunk, OnDone, TEXT("") };
    LiteRtLm_SamplingParams Params = { 0.7f, 0.9f, 40, 512 }; // Default params

    FLiteRtLmWrapperLoader::RunInference(Session, Params, Internal_LiteRtLmCallback, CallbackCtx);
}

void FLiteRtLmUnrealApi::ChatWithContext(void* Ctx, const FString& HistoryJson, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone)
{
    // For MVP, we treat HistoryJson as the latest message if simple, 
    // but in a commercial version, you'd parse JSON and call Append incrementally.
    // Here we demonstrate the incremental Append logic.
    ChatWithPrompt(Ctx, TEXT(""), HistoryJson, OnChunk, OnDone);
}

float FLiteRtLmUnrealApi::GetVRAMUsage()
{
    ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();
    if (Subsystem && Subsystem->GetEngineHandle())
    {
        return FLiteRtLmWrapperLoader::GetVRAMUsage(Subsystem->GetEngineHandle());
    }
    return 0.0f;
}
