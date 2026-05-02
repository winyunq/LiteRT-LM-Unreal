**EN** | [中文](API_REFERENCE_zh.md)

# API Reference

This document provides technical details of the **LiteRT-LM-Unreal** core classes and structures.

## 1. ULiteRtLmSubsystem (Engine Subsystem)

A global singleton responsible for model lifecycle and session pool management.

### Primary Methods
- `bool LoadModel(const FLiteRtLmConfig& InConfig)`: Loads and initializes the engine.
- `void UnloadModel()`: Releases all VRAM and memory resources.
- `void* GetOrCreateSession(void* Ctx, const FString& JsonPreface = TEXT(""))`: Internal use, retrieves or creates an inference session based on the context pointer.

---

## 2. FLiteRtLmUnrealApi (Static Helper)

... (Static method descriptions omitted) ...

---

## 3. ULiteRtLmComponent (Actor Component)

A C++ component designed specifically for NPCs and interactive objects. It wraps low-level session management into Blueprint-accessible events.

### Primary Properties
- `SystemPrompt`: String, defines the NPC's personality and background.
- `SamplingParams`: Sampling parameters (Temperature, TopP, etc.).

### Primary Methods
- `SendChatMessage(FString Message)`: Sends a message to the NPC, automatically handling KV Cache memory.
- `ResetConversation()`: Clears the conversation history for this NPC.

### Blueprint Events
- `OnTextChunkReceived`: Triggered when a streaming text chunk arrives.
- `OnInferenceCompleted`: Triggered when full response generation ends, containing detailed performance data.

---

## 4. Core Structs

### FLiteRtLmConfig
Controls engine startup parameters.
- `ModelPath`: String, path to the model.
- `Backend`: `gpu` (default) or `cpu`.
- `MaxNumTokens`: Total KV Cache capacity.
- `bOptimizeShader`: Whether to optimize shaders on Windows/Vulkan.

### FLiteRtLmSamplingParams
Controls text generation quality.
- `Temperature`: Randomness (0.0 - 1.0).
- `TopP`: Nucleus sampling threshold.
- `TopK`: Top-K sampling limit.
- `ConstraintType`: Constrained decoding mode (`Regex`, `Json`, `Lark`).
- `ConstraintString`: Corresponding constraint rules (e.g., regex or JSON Schema).

### FLiteRtLmResult
Detailed data packet after an inference task is completed.
- `FullText`: The complete generated text.
- `FullJson`: If the model output JSON or called a tool, this contains structured data.
- `TimeMs`: Total inference time.
- `TokensPerSec`: Tokens generated per second (throughput metric).

---

## 5. Delegates & Callbacks

- `FLiteRtLmChunkCallback`: Triggered when the model outputs a new Token or text chunk.
- `FLiteRtLmDoneCallback`: Triggered when inference fully completes (or fails).

> **💡 Expert Tip**: Callbacks are executed on the Unreal Game Thread, allowing you to modify UI or update Actor states directly.

---
*Powered by Winyunq - The Ultimate Productivity Toolset.*
