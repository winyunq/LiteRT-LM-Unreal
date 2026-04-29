// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LiteRtLmUnrealApi.generated.h"

/**
 * High-level configuration for LiteRT-LM engine.
 */
USTRUCT(BlueprintType)
struct LITERTLMCORE_API FLiteRtLmConfig
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
struct LITERTLMCORE_API FLiteRtLmSamplingParams
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
struct LITERTLMCORE_API FLiteRtLmResult
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
};

// Callback delegates
DECLARE_DELEGATE_OneParam(FLiteRtLmChunkCallback, const FString& /*TextChunk*/);
DECLARE_DELEGATE_OneParam(FLiteRtLmDoneCallback, const FLiteRtLmResult& /*Result*/);

/**
 * The Unreal-facing API for LiteRT-LM.
 */
class LITERTLMCORE_API FLiteRtLmUnrealApi
{
public:
    // Lifecycle
    static FLiteRtLmConfig GetAutoConfig(int32 TargetVramMB);
    static bool LoadModel(const FLiteRtLmConfig& Config);
    static void UnloadModel();

    // Inference
    
    /**
     * Stateless single-turn generation.
     */
    static void GenerateOnce(
        const FString& Prompt, 
        FLiteRtLmChunkCallback OnChunk = FLiteRtLmChunkCallback(), 
        FLiteRtLmDoneCallback OnDone = FLiteRtLmDoneCallback(),
        const FLiteRtLmSamplingParams& Params = FLiteRtLmSamplingParams()
    );
    
    /**
     * Agent Startup / Persistent Session.
     * @param Ctx The context pointer (e.g. the Agent object) used to identify the session.
     */
    static void ChatWithPrompt(
        void* Ctx, 
        const FString& UserMsg, 
        const FString& SystemPrompt = TEXT(""),
        FLiteRtLmChunkCallback OnChunk = FLiteRtLmChunkCallback(), 
        FLiteRtLmDoneCallback OnDone = FLiteRtLmDoneCallback(),
        const FLiteRtLmSamplingParams& Params = FLiteRtLmSamplingParams()
    );

    /**
     * Send raw JSON message (for Multi-modal or Tool results).
     */
    static void AppendMessageJson(void* Ctx, const FString& JsonMsg);

    /**
     * Multi-turn Conversation with Cache-Hit.
     */
    static void ChatWithContext(
        void* Ctx, 
        const FString& HistoryJson, 
        FLiteRtLmChunkCallback OnChunk = FLiteRtLmChunkCallback(), 
        FLiteRtLmDoneCallback OnDone = FLiteRtLmDoneCallback(),
        const FLiteRtLmSamplingParams& Params = FLiteRtLmSamplingParams()
    );

    static float GetVRAMUsage();
};
