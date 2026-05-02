**EN** | [中文](GETTING_STARTED_zh.md)

# Getting Started Guide

Welcome to **LiteRT-LM-Unreal**. This guide will help you set up the environment and achieve your first local LLM inference within 5 minutes.

## 1. Installation Steps

1.  **Extract Plugin**: Copy the `LiteRT-LM-Unreal` folder into your Unreal project's `Plugins` directory.
2.  **Regenerate Project**: Right-click your `.uproject` file and select `Generate Visual Studio project files`.
3.  **Compile Project**: Click compile in your IDE (VS/Rider) or within the Editor.
4.  **Enable Plugin**: Ensure `LiteRT-LM-Unreal` is checked in the Editor under `Edit -> Plugins` and restart.

## 2. Prepare Model Files

This plugin is based on Google's **LiteRT-LM** engine.
- Go to the [Google AI Edge Model Repository](https://github.com/google-ai-edge/LiteRT-LM) to download a compatible model (e.g., `Gemma-2B-IT`).
- Place the downloaded `.bin` file in a location you remember (Recommended: `D:/Models/gemma.bin`).

## 3. Implement Your First AI Conversation (C++)

In your `Actor` or `Character`, use the following code to wake up the AI:

```cpp
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"

// 1. Load Model
FLiteRtLmConfig Config;
Config.ModelPath = TEXT("D:/Models/gemma.bin");
FLiteRtLmUnrealApi::LoadModel(Config);

// 2. Initiate Conversation
TArray<TSharedPtr<FJsonObject>> Messages;
auto UserMsg = MakeShared<FJsonObject>();
UserMsg->SetStringField("role", "user");
UserMsg->SetStringField("content", "Hello, please introduce yourself.");
Messages.Add(UserMsg);

// Use 'this' as the context pointer for automatic KV Cache session management
FLiteRtLmUnrealApi::SendChatRequest(
    this, 
    Messages,
    TEXT(""), // ToolsJson
    FLiteRtLmChunkCallback::CreateLambda([](const FString& Chunk) {
        // Handle streaming output in real-time (typewriter effect)
        UE_LOG(LogTemp, Warning, TEXT("AI Typing: %s"), *Chunk);
    }),
    FLiteRtLmDoneCallback::CreateLambda([](const FLiteRtLmResult& Result) {
        // Callback after inference completion
        UE_LOG(LogTemp, Display, TEXT("Conversation ended. Full response: %s"), *Result.FullText);
    })
);
```

## 4. Blueprint Support

Since the API involves complex asynchronous callbacks and C++ pointers, we currently strongly recommend using C++ for core integration.
*Note: Blueprint-specific API nodes are under development and expected in v1.1.*

## 5. FAQ

- **Q: Why does the Editor freeze when loading the model?**
  - A: Initial loading requires VRAM allocation and shader compilation (if `OptimizeShader` is enabled). We suggest calling `LoadModel` asynchronously during a Loading Screen.
- **Q: Does it support mobile?**
  - A: Windows (Vulkan/DirectX) is currently supported. Android/iOS support is in internal testing.

---
*Winyunq Strategy: Peak Performance, Within Reach.*
