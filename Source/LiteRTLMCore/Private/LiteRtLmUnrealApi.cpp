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
    FTCHARToUTF8 Utf8Constraint; // Keep UTF8 string alive during inference

    FLiteRtLmCallbackContext(FLiteRtLmChunkCallback InChunk, FLiteRtLmDoneCallback InDone, const FString& InConstraint)
        : OnChunk(InChunk), OnDone(InDone), FullResponse(TEXT("")), Utf8Constraint(*InConstraint)
    {}
};

static LiteRtLm_SamplingParams MapSamplingParams(const FLiteRtLmSamplingParams& InParams, const FLiteRtLmCallbackContext* Ctx)
{
    LiteRtLm_SamplingParams OutParams;
    OutParams.temperature = InParams.Temperature;
    OutParams.top_p = InParams.TopP;
    OutParams.top_k = InParams.TopK;
    OutParams.max_tokens = InParams.MaxTokens;
    OutParams.constraint_type = (int)InParams.ConstraintType;
    OutParams.constraint_string = Ctx->Utf8Constraint.Get();
    return OutParams;
}

static void Internal_LiteRtLmCallback(LiteRtLm_Result Result, void* UserPtr)
{
    FLiteRtLmCallbackContext* Ctx = static_cast<FLiteRtLmCallbackContext*>(UserPtr);
    if (!Ctx) return;

    if (Result.text_chunk)
    {
        FString Chunk = UTF8_TO_TCHAR(Result.text_chunk);
        Ctx->FullResponse += Chunk;
        
        if (Ctx->OnChunk.IsBound())
        {
            FLiteRtLmChunkCallback ChunkCopy = Ctx->OnChunk;
            AsyncTask(ENamedThreads::GameThread, [ChunkCopy, Chunk]() {
                ChunkCopy.ExecuteIfBound(Chunk);
            });
        }
    }

    if (Result.bIsDone)
    {
        FLiteRtLmResult FinalResult;
        FinalResult.FullText = Ctx->FullResponse;
        FinalResult.FullJson = UTF8_TO_TCHAR(Result.full_json_chunk);
        FinalResult.ErrorMsg = UTF8_TO_TCHAR(Result.error_msg);
        FinalResult.TokensPerSec = Result.tokens_per_sec;
        FinalResult.bIsDone = true;

        if (Ctx->OnDone.IsBound())
        {
            FLiteRtLmDoneCallback DoneCopy = Ctx->OnDone;
            AsyncTask(ENamedThreads::GameThread, [DoneCopy, FinalResult, Ctx]() {
                DoneCopy.ExecuteIfBound(FinalResult);
                delete Ctx; // Lifecycle ends here
            });
        }
        else
        {
            delete Ctx;
        }
    }
}

void FLiteRtLmUnrealApi::GenerateOnce(const FString& Prompt, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone, const FLiteRtLmSamplingParams& Params)
{
    ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();
    if (!Subsystem) return;

    // Use a unique dummy context for one-shot
    void* TempCtx = new uint8(0);
    void* Session = Subsystem->GetOrCreateSession(TempCtx);
    if (!Session) {
        delete (uint8*)TempCtx;
        return;
    }

    FLiteRtLmWrapperLoader::AppendUserMessage(Session, TCHAR_TO_UTF8(*Prompt));

    FLiteRtLmDoneCallback CleanupDone = FLiteRtLmDoneCallback::CreateLambda([OnDone, Subsystem, TempCtx](const FLiteRtLmResult& Result) {
        OnDone.ExecuteIfBound(Result);
        Subsystem->ReleaseSession(TempCtx);
        delete (uint8*)TempCtx;
    });

    FLiteRtLmCallbackContext* CallbackCtx = new FLiteRtLmCallbackContext(OnChunk, CleanupDone, Params.ConstraintString);
    FLiteRtLmWrapperLoader::RunInference(Session, MapSamplingParams(Params, CallbackCtx), Internal_LiteRtLmCallback, CallbackCtx);
}

void FLiteRtLmUnrealApi::ChatWithPrompt(void* Ctx, const FString& UserMsg, const FString& SystemPrompt, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone, const FLiteRtLmSamplingParams& Params)
{
    ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();
    if (!Subsystem || !Ctx) return;

    void* Session = Subsystem->GetOrCreateSession(Ctx);
    if (!Session) return;

    if (!SystemPrompt.IsEmpty())
    {
        FLiteRtLmWrapperLoader::AppendAssistantMessage(Session, TCHAR_TO_UTF8(*(TEXT("System: ") + SystemPrompt)));
    }
    
    FLiteRtLmWrapperLoader::AppendUserMessage(Session, TCHAR_TO_UTF8(*UserMsg));

    FLiteRtLmCallbackContext* CallbackCtx = new FLiteRtLmCallbackContext(OnChunk, OnDone, Params.ConstraintString);
    FLiteRtLmWrapperLoader::RunInference(Session, MapSamplingParams(Params, CallbackCtx), Internal_LiteRtLmCallback, CallbackCtx);
}

void FLiteRtLmUnrealApi::AppendMessageJson(void* Ctx, const FString& JsonMsg)
{
    ULiteRtLmSubsystem* Subsystem = GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();
    if (!Subsystem || !Ctx) return;

    void* Session = Subsystem->GetOrCreateSession(Ctx);
    if (Session)
    {
        FLiteRtLmWrapperLoader::AppendMessageJson(Session, TCHAR_TO_UTF8(*JsonMsg));
    }
}

void FLiteRtLmUnrealApi::ChatWithContext(void* Ctx, const FString& HistoryJson, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone, const FLiteRtLmSamplingParams& Params)
{
    // For v3.0, we can use AppendMessageJson if HistoryJson is structured,
    // or just treat it as a message.
    ChatWithPrompt(Ctx, HistoryJson, TEXT(""), OnChunk, OnDone, Params);
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
