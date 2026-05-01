// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "LiteRtLmUnrealApi.generated.h"

/**
 * High-level configuration for LiteRT-LM engine.
 */
USTRUCT(BlueprintType)
struct LITERTLMUNREAL_API FLiteRtLmConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    FString ModelPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    FString Backend = TEXT("gpu"); // "cpu", "gpu"

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    int32 MaxNumTokens = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    int32 NumThreads = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    bool bEnableBenchmark = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    bool bOptimizeShader = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    bool bEnableStreaming = true;
};

/**
 * 约束解码类型 (Constrained Decoding Type)
 */
UENUM(BlueprintType)
enum class ELiteRtLmConstraintType : uint8
{
    None = 0,
    Regex = 1,
    Json = 2,
    Lark = 3
};

/**
 * High-level sampling parameters for LiteRT-LM.
 */
USTRUCT(BlueprintType)
struct LITERTLMUNREAL_API FLiteRtLmSamplingParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    float Temperature = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    float TopP = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    int32 TopK = 40;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    int32 MaxTokens = 512;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    ELiteRtLmConstraintType ConstraintType = ELiteRtLmConstraintType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LiteRT-LM")
    FString ConstraintString;
};

/**
 * Result structure for inference completion.
 */
USTRUCT(BlueprintType)
struct LITERTLMUNREAL_API FLiteRtLmResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    FString FullText;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    FString FullJson; // Contains complete MCP/Tool data

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    float TimeMs = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    int32 TokensCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    float TokensPerSec = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    FString ErrorMsg;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM")
    bool bIsDone = false;

    /** Structured tool_calls parsed from full_json_chunk (OpenAI-compatible format). */
    TArray<TSharedPtr<FJsonObject>> ToolCalls;
};

// Callback delegates
DECLARE_DELEGATE_OneParam(FLiteRtLmChunkCallback, const FString& /*TextChunk*/);
DECLARE_DELEGATE_OneParam(FLiteRtLmDoneCallback, const FLiteRtLmResult& /*Result*/);

/**
 * The Unreal-facing API for LiteRT-LM.
 * Acts as an "API provider" – callers only need to call SendChatRequest().
 */
class LITERTLMUNREAL_API FLiteRtLmUnrealApi
{
public:
    // ===== Lifecycle =====

    /**
     * Automatically detect available VRAM and build a config.
     * Default cap: 4GB. Uses DXGI real-time query on Windows.
     */
    static FLiteRtLmConfig GetAutoConfig();
    static bool LoadModel(const FLiteRtLmConfig& Config);
    static void UnloadModel();
    static bool IsModelLoaded();

    // ===== Core API: SendChatRequest =====

    /**
     * Send a chat request – analogous to Google's generateContent.
     * Internally handles session management, incremental message sync,
     * tool injection via json_preface, and structured tool_call parsing.
     *
     * @param SessionKey  Unique key for session (typically the Agent pointer).
     * @param Messages    Complete conversation history (role/content JSON objects).
     * @param ToolsJson   Tool definitions JSON array string (can be empty).
     *                    Format: [{"name":"...", "parameters":{...}}]
     * @param OnChunk     Streaming text callback (game thread).
     * @param OnDone      Completion callback with FullText + ToolCalls (game thread).
     * @param Params      Sampling parameters.
     */
    static void SendChatRequest(
        void* SessionKey,
        const TArray<TSharedPtr<FJsonObject>>& Messages,
        const FString& ToolsJson,
        FLiteRtLmChunkCallback OnChunk = FLiteRtLmChunkCallback(),
        FLiteRtLmDoneCallback OnDone = FLiteRtLmDoneCallback(),
        const FLiteRtLmSamplingParams& Params = FLiteRtLmSamplingParams()
    );

    // ===== Session Management =====

    static void ReleaseSession(void* SessionKey);
};
