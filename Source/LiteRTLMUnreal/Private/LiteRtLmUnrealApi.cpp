// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"
#include "Engine/Engine.h"
#include "Internal/LiteRtLmWrapperLoader.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================
// Lifecycle
// ============================================================

FLiteRtLmConfig FLiteRtLmUnrealApi::GetAutoConfig()
{
    FLiteRtLmConfig Config;

    // Query actual available VRAM
    const int32 AvailableMB = ULiteRtLmSubsystem::QueryAvailableVramMB(4096);

    // Conservative: use at most 4GB, or whatever is available
    static constexpr int32 MaxDefaultVramMB = 4096;
    const int32 TargetVramMB = FMath::Min(AvailableMB, MaxDefaultVramMB);

    if (TargetVramMB < 2048)
    {
        Config.MaxNumTokens = 1024;
        Config.Backend = TEXT("cpu");
    }
    else
    {
        Config.MaxNumTokens = 2048;
        Config.Backend = TEXT("gpu");
    }

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] AutoConfig: AvailableVRAM=%d MB, Target=%d MB, Backend=%s"),
        AvailableMB, TargetVramMB, *Config.Backend);

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

bool FLiteRtLmUnrealApi::IsModelLoaded()
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr)
    {
        return Subsystem->IsModelLoaded();
    }
    return false;
}

// ============================================================
// Internal: C-style callback context
// ============================================================

struct FLiteRtLmCallbackContext
{
    FLiteRtLmChunkCallback OnChunk;
    FLiteRtLmDoneCallback OnDone;
    FString FullResponse;
    FString FullJsonResponse;
    FTCHARToUTF8 Utf8Constraint; // Keep UTF8 string alive during inference

    FLiteRtLmCallbackContext(FLiteRtLmChunkCallback InChunk, FLiteRtLmDoneCallback InDone, const FString& InConstraint)
        : OnChunk(InChunk), OnDone(InDone), FullResponse(TEXT("")), FullJsonResponse(TEXT("")), Utf8Constraint(*InConstraint)
    {}
};

static LiteRtLm_SamplingParams MapSamplingParams(const FLiteRtLmSamplingParams& InParams, const FLiteRtLmCallbackContext* Ctx)
{
    LiteRtLm_SamplingParams OutParams = {};
    OutParams.temperature = InParams.Temperature;
    OutParams.top_p = InParams.TopP;
    OutParams.top_k = InParams.TopK;
    OutParams.max_tokens = InParams.MaxTokens;
    OutParams.constraint_type = (int)InParams.ConstraintType;
    
    // Crucial: Only pass string if type is not None, otherwise pass nullptr
    if (InParams.ConstraintType != ELiteRtLmConstraintType::None)
    {
        OutParams.constraint_string = Ctx->Utf8Constraint.Get();
    }
    else
    {
        OutParams.constraint_string = nullptr;
    }
    return OutParams;
}

/**
 * Parse tool_calls from the full JSON response collected during streaming.
 * The DLL returns chunks like: {"role":"assistant","tool_calls":[{"type":"function","function":{"name":"...","arguments":{...}}}]}
 * We parse tool_calls from the accumulated full_json if present.
 */
static TArray<TSharedPtr<FJsonObject>> ParseToolCallsFromJson(const FString& FullJsonAccumulated)
{
    TArray<TSharedPtr<FJsonObject>> ToolCalls;
    if (FullJsonAccumulated.IsEmpty()) return ToolCalls;

    // The full_json_chunk from DLL may contain multiple JSON fragments.
    // The last chunk with tool_calls is the definitive one.
    // Try parsing the accumulated JSON as a single object first.
    TSharedPtr<FJsonObject> Parsed;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FullJsonAccumulated);
    if (FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* ToolCallsArr;
        if (Parsed->TryGetArrayField(TEXT("tool_calls"), ToolCallsArr))
        {
            for (const auto& TCVal : *ToolCallsArr)
            {
                TSharedPtr<FJsonObject> TC = TCVal->AsObject();
                if (TC.IsValid())
                {
                    // The DLL returns {"type":"function","function":{"name":"...","arguments":{...}}}
                    // We need to serialize arguments back to string for OpenAI compatibility
                    const TSharedPtr<FJsonObject>* FuncPtr;
                    if (TC->TryGetObjectField(TEXT("function"), FuncPtr) && FuncPtr)
                    {
                        const TSharedPtr<FJsonObject>* ArgsObjPtr;
                        if ((*FuncPtr)->TryGetObjectField(TEXT("arguments"), ArgsObjPtr) && ArgsObjPtr)
                        {
                            FString ArgsJson;
                            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ArgsJson);
                            FJsonSerializer::Serialize((*ArgsObjPtr).ToSharedRef(), Writer);
                            (*FuncPtr)->SetStringField(TEXT("arguments"), ArgsJson);
                        }
                    }
                    // Add a unique ID if not present
                    if (!TC->HasField(TEXT("id")))
                    {
                        FString FuncName;
                        if (TC->TryGetObjectField(TEXT("function"), FuncPtr))
                        {
                            (*FuncPtr)->TryGetStringField(TEXT("name"), FuncName);
                        }
                        TC->SetStringField(TEXT("id"),
                            FString::Printf(TEXT("call_%s_%s"), *FuncName,
                                *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8)));
                    }
                    ToolCalls.Add(TC);
                }
            }
        }
    }

    return ToolCalls;
}

static void Internal_LiteRtLmCallback(LiteRtLm_Result Result, void* UserPtr)
{
    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Internal_LiteRtLmCallback invoked. bIsDone=%d"), Result.bIsDone);
    FLiteRtLmCallbackContext* Ctx = static_cast<FLiteRtLmCallbackContext*>(UserPtr);
    if (!Ctx) return;

    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Callback: text=%s, json=%s, done=%d, err=%s"),
        Result.text_chunk ? UTF8_TO_TCHAR(Result.text_chunk) : TEXT("(null)"),
        Result.full_json_chunk ? TEXT("(has json)") : TEXT("(null)"),
        Result.bIsDone,
        Result.error_msg ? UTF8_TO_TCHAR(Result.error_msg) : TEXT("(none)"));

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

    // Accumulate full_json_chunk for tool_call parsing
    if (Result.full_json_chunk)
    {
        FString JsonChunk = UTF8_TO_TCHAR(Result.full_json_chunk);
        // Keep only the latest full JSON (each chunk is a complete message snapshot)
        Ctx->FullJsonResponse = JsonChunk;
    }

    if (Result.bIsDone)
    {
        FLiteRtLmResult FinalResult;
        FinalResult.FullText = Ctx->FullResponse;
        FinalResult.FullJson = Ctx->FullJsonResponse;
        FinalResult.ErrorMsg = Result.error_msg ? UTF8_TO_TCHAR(Result.error_msg) : TEXT("");
        FinalResult.TokensPerSec = Result.tokens_per_sec;
        FinalResult.bIsDone = true;

        // Parse tool_calls from the accumulated JSON
        FinalResult.ToolCalls = ParseToolCallsFromJson(Ctx->FullJsonResponse);

        // Clean up response text (remove any residual turn markers if DLL leaks them)
        FinalResult.FullText.ReplaceInline(TEXT("<end_of_turn>"), TEXT(""));
        FinalResult.FullText.ReplaceInline(TEXT("<start_of_turn>"), TEXT(""));
        FinalResult.FullText.TrimStartAndEndInline();

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

// ============================================================
// Internal: Message normalization
// ============================================================

/**
 * Normalize messages for LiteRT-LM (Gemma has no system role).
 * - Merge system messages into the next user message
 * - Convert tool-result messages to user messages with structured JSON
 * - Pass through user/assistant messages
 */
static TArray<TSharedPtr<FJsonObject>> NormalizeMessages(
    const TArray<TSharedPtr<FJsonObject>>& InMessages)
{
    TArray<TSharedPtr<FJsonObject>> Result;
    FString SystemContent;

    for (const TSharedPtr<FJsonObject>& Msg : InMessages)
    {
        FString Role = Msg->GetStringField(TEXT("role"));
        FString Content = Msg->GetStringField(TEXT("content"));

        if (Role == TEXT("system"))
        {
            SystemContent += Content + TEXT("\n\n");
            continue;
        }

        TSharedPtr<FJsonObject> NewMsg = MakeShared<FJsonObject>();

        if (Role == TEXT("user") && !SystemContent.IsEmpty())
        {
            NewMsg->SetStringField(TEXT("role"), TEXT("user"));
            NewMsg->SetStringField(TEXT("content"), SystemContent + Content);
            SystemContent.Empty();
            Result.Add(NewMsg);
            continue;
        }

        if (Role == TEXT("tool"))
        {
            // Convert tool results to a user message
            FString ToolCallId;
            Msg->TryGetStringField(TEXT("tool_call_id"), ToolCallId);
            NewMsg->SetStringField(TEXT("role"), TEXT("user"));
            NewMsg->SetStringField(TEXT("content"),
                FString::Printf(TEXT("[Tool Result] (id: %s)\n%s"), *ToolCallId, *Content));
            Result.Add(NewMsg);
            continue;
        }

        if (Role == TEXT("assistant"))
        {
            // If assistant message has tool_calls, serialize them as part of content
            const TArray<TSharedPtr<FJsonValue>>* ToolCallsArr;
            if (Msg->TryGetArrayField(TEXT("tool_calls"), ToolCallsArr) && ToolCallsArr && ToolCallsArr->Num() > 0)
            {
                FString TextContent = Content;
                for (const auto& TCVal : *ToolCallsArr)
                {
                    TSharedPtr<FJsonObject> TC = TCVal->AsObject();
                    if (!TC) continue;
                    const TSharedPtr<FJsonObject>* FuncPtr;
                    if (TC->TryGetObjectField(TEXT("function"), FuncPtr))
                    {
                        FString FuncName, FuncArgs;
                        (*FuncPtr)->TryGetStringField(TEXT("name"), FuncName);
                        (*FuncPtr)->TryGetStringField(TEXT("arguments"), FuncArgs);
                        TextContent += FString::Printf(
                            TEXT("\n```tool_code\n%s(%s)\n```"),
                            *FuncName, *FuncArgs);
                    }
                }
                NewMsg->SetStringField(TEXT("role"), TEXT("assistant"));
                NewMsg->SetStringField(TEXT("content"), TextContent);
                Result.Add(NewMsg);
                continue;
            }
        }

        // Default: pass through
        NewMsg->SetStringField(TEXT("role"), Role);
        NewMsg->SetStringField(TEXT("content"), Content);
        Result.Add(NewMsg);
    }

    // Any remaining system content (no user message followed)
    if (!SystemContent.IsEmpty())
    {
        TSharedPtr<FJsonObject> SysAsUser = MakeShared<FJsonObject>();
        SysAsUser->SetStringField(TEXT("role"), TEXT("user"));
        SysAsUser->SetStringField(TEXT("content"), SystemContent.TrimEnd());
        Result.Insert(SysAsUser, 0);
    }

    return Result;
}

// ============================================================
// SendChatRequest
// ============================================================

void FLiteRtLmUnrealApi::SendChatRequest(
    void* SessionKey,
    const TArray<TSharedPtr<FJsonObject>>& Messages,
    const FString& ToolsJson,
    FLiteRtLmChunkCallback OnChunk,
    FLiteRtLmDoneCallback OnDone,
    const FLiteRtLmSamplingParams& Params)
{
    ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr;
    if (!Subsystem || !SessionKey)
    {
        if (OnDone.IsBound())
        {
            FLiteRtLmResult ErrResult;
            ErrResult.ErrorMsg = TEXT("LiteRT-LM Subsystem not available.");
            ErrResult.bIsDone = true;
            OnDone.ExecuteIfBound(ErrResult);
        }
        return;
    }

    // 1. Normalize messages (merge system role, flatten tool results)
    TArray<TSharedPtr<FJsonObject>> NormMessages = NormalizeMessages(Messages);
    if (NormMessages.Num() == 0) return;

    // 2. Get or create session (with tool injection)
    void* Session = Subsystem->GetOrCreateSession(SessionKey, ToolsJson);
    if (!Session)
    {
        if (OnDone.IsBound())
        {
            FLiteRtLmResult ErrResult;
            ErrResult.ErrorMsg = TEXT("Failed to create LiteRT-LM session.");
            ErrResult.bIsDone = true;
            OnDone.ExecuteIfBound(ErrResult);
        }
        return;
    }

    // 3. Incremental message sync – only append new messages
    int32 LastSentCount = Subsystem->GetSessionMsgCount(SessionKey);

    // If history shrank (agent was reset), drop session and recreate
    if (LastSentCount > NormMessages.Num())
    {
        Subsystem->ReleaseSession(SessionKey);
        Session = Subsystem->GetOrCreateSession(SessionKey, ToolsJson);
        if (!Session)
        {
            if (OnDone.IsBound())
            {
                FLiteRtLmResult ErrResult;
                ErrResult.ErrorMsg = TEXT("Failed to recreate LiteRT-LM session after reset.");
                ErrResult.bIsDone = true;
                OnDone.ExecuteIfBound(ErrResult);
            }
            return;
        }
        LastSentCount = 0;
    }

    const int32 LastMsgIdx = NormMessages.Num() - 1;

    // Append all history messages except the last one (which triggers inference)
    for (int32 i = LastSentCount; i < LastMsgIdx; ++i)
    {
        FString Role = NormMessages[i]->GetStringField(TEXT("role"));
        FString Content = NormMessages[i]->GetStringField(TEXT("content"));

        TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
        MsgObj->SetStringField(TEXT("role"), Role);
        MsgObj->SetStringField(TEXT("content"), Content);
        FString MsgJson;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgJson);
        FJsonSerializer::Serialize(MsgObj.ToSharedRef(), Writer);

        FLiteRtLmWrapperLoader::AppendMessageJson(Session, TCHAR_TO_UTF8(*MsgJson));
    }

    // 4. Append the last user message to trigger inference
    FString LastRole = NormMessages[LastMsgIdx]->GetStringField(TEXT("role"));
    FString LastContent = NormMessages[LastMsgIdx]->GetStringField(TEXT("content"));

    if (LastRole == TEXT("assistant"))
    {
        // Edge case: last message is assistant – append it and ask for continuation
        FString AsJson;
        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&AsJson);
        TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
        MsgObj->SetStringField(TEXT("role"), TEXT("assistant"));
        MsgObj->SetStringField(TEXT("content"), LastContent);
        FJsonSerializer::Serialize(MsgObj.ToSharedRef(), W);
        FLiteRtLmWrapperLoader::AppendMessageJson(Session, TCHAR_TO_UTF8(*AsJson));

        FLiteRtLmWrapperLoader::AppendUserMessage(Session, TCHAR_TO_UTF8(*FString(TEXT("Please continue."))));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] AppendUserMessage: %s"), *LastContent.Left(100));
        FLiteRtLmWrapperLoader::AppendUserMessage(Session, TCHAR_TO_UTF8(*LastContent));
    }

    // 5. Update message count (will be finalized in done callback)
    const int32 NewSentCount = NormMessages.Num();

    // Wrap the user's OnDone to also update the session message count
    FLiteRtLmDoneCallback WrappedOnDone = FLiteRtLmDoneCallback::CreateLambda(
        [OnDone, Subsystem, SessionKey, NewSentCount](const FLiteRtLmResult& Result)
        {
            UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] OnDone: text_len=%d, tool_calls=%d, err=%s"),
                Result.FullText.Len(), Result.ToolCalls.Num(), *Result.ErrorMsg);
            if (Subsystem)
            {
                Subsystem->SetSessionMsgCount(SessionKey, NewSentCount);
            }
            OnDone.ExecuteIfBound(Result);
        });

    // 6. Run inference on a background thread
    // SendMessageAsync only submits work to the WebGPU queue.
    // WaitUntilDone drives the engine event loop and triggers callbacks.
    void* EngineHandle = Subsystem->GetEngineHandle();
    UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] RunInference: Session=%p, Engine=%p, MaxTokens=%d, Temp=%.2f"),
        Session, EngineHandle, Params.MaxTokens, Params.Temperature);

    FLiteRtLmCallbackContext* CallbackCtx = new FLiteRtLmCallbackContext(OnChunk, WrappedOnDone, Params.ConstraintString);
    LiteRtLm_SamplingParams CParams = MapSamplingParams(Params, CallbackCtx);

    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Session, EngineHandle, CParams, CallbackCtx]()
    {
        const double StartTime = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] AsyncTask started on thread %d"), FPlatformTLS::GetCurrentThreadId());

        if (!FLiteRtLmWrapperLoader::RunInference)
        {
            UE_LOG(LogTemp, Error, TEXT("[LiteRtLm] RunInference function pointer is NULL!"));
            return;
        }

        UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Calling DLL RunInference... (Tokens=%d, Temp=%.2f)"), 
            CParams.max_tokens, CParams.temperature);
            
        FLiteRtLmWrapperLoader::RunInference(Session, CParams, Internal_LiteRtLmCallback, CallbackCtx);
        
        const double AfterRunTime = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] DLL RunInference returned in %.2f ms."), (AfterRunTime - StartTime) * 1000.0);

        if (FLiteRtLmWrapperLoader::WaitUntilDone && EngineHandle)
        {
            UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] Entering WaitUntilDone loop..."));
            int Result = FLiteRtLmWrapperLoader::WaitUntilDone(EngineHandle, 600); // 10 min timeout
            const double EndTime = FPlatformTime::Seconds();
            UE_LOG(LogTemp, Log, TEXT("[LiteRtLm] WaitUntilDone finished in %.2f ms with result: %d"), 
                (EndTime - AfterRunTime) * 1000.0, Result);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[LiteRtLm] WaitUntilDone not available. EngineHandle=%p"), EngineHandle);
        }
    });
}

// ============================================================
// Session Management
// ============================================================

void FLiteRtLmUnrealApi::ReleaseSession(void* SessionKey)
{
    if (ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr)
    {
        Subsystem->ReleaseSession(SessionKey);
    }
}
