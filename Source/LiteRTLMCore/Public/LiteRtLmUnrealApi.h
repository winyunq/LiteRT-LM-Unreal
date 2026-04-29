// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

/**
 * High-level configuration for LiteRT-LM engine.
 */
struct LITERTLMCORE_API FLiteRtLmConfig
{
    FString ModelPath;
    FString Backend = TEXT("gpu"); // "cpu", "gpu"
    int32 MaxNumTokens = 2048;
    int32 NumThreads = 8;
    bool bEnableBenchmark = false;
    bool bOptimizeShader = true;
    bool bEnableStreaming = true;
};

/**
 * Result structure for inference completion.
 */
struct LITERTLMCORE_API FLiteRtLmResult
{
    FString FullText;
    FString ToolCalls; // JSON format
    float TimeMs = 0.0f;
    int32 TokensCount = 0;
    float TokensPerSec = 0.0f;
    FString ErrorMsg;
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
    static void GenerateOnce(const FString& Prompt, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone);
    
    /**
     * Agent Startup / Persistent Session.
     * @param Ctx The context pointer (e.g. the Agent object) used to identify the session.
     */
    static void ChatWithPrompt(void* Ctx, const FString& SystemPrompt, const FString& UserMsg, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone);

    /**
     * Multi-turn Conversation with Cache-Hit.
     * @param Ctx The context pointer used to identify the session.
     * @param HistoryJson The full history (the API will handle the incremental part via the session).
     */
    static void ChatWithContext(void* Ctx, const FString& HistoryJson, FLiteRtLmChunkCallback OnChunk, FLiteRtLmDoneCallback OnDone);

    static float GetVRAMUsage();
};
