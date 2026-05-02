**EN** | [中文](API_LAYER_zh.md)

# LiteRT-LM Unreal: 5. Core Principles (API Layer)

This chapter is for C++ hardcore developers. We will delve into the plugin's lowest-level raw interface `FLiteRtLmUnrealApi` and the core operational logic behind it.

> **Tip**: If you wish to use it directly in Blueprints, please skip this page and read **[3. App Interface Layer](APP_LAYER.md)**.

---

## 1. Core Static Interface: FLiteRtLmUnrealApi

This is the "pure" C++ bridge between the plugin and the Google LiteRT-LM engine. All methods are static functions and do not hold any Unreal UObject state, ensuring the highest calling efficiency.

### 1.1 Lifecycle Management
- **`LoadModel(Config)`**: Loads the model at the specified path into VRAM. Internally, it initializes the backend (Vulkan/VEC/CPU) based on the configuration.
- **`UnloadModel()`**: Forcibly releases all inference-related resources, ensuring VRAM is returned to the system.

### 1.2 Raw Inference Interface
```cpp
static void GenerateOnce(
    const FString& Prompt, 
    FLiteRtLmChunkCallback OnChunk, 
    FLiteRtLmDoneCallback OnDone,
    const FLiteRtLmSamplingParams& Params
);
```
This is stateless single-pass inference. It does not retain any context and is suitable for temporary tasks that do not require memory.

---

## 2. Core Principle: Instant KV Cache Mapping

The most core technical advantage of LiteRT-LM Unreal lies in its management of **Sessions**.

### 2.1 Session Context Handle (`void* Ctx`)
In `ChatWithPrompt` or `ChatWithContext`, you will find a `void* Ctx` parameter.
- **Principle**: We use this pointer as a unique Hash Key to look up the corresponding inference context in the internal session pool.
- **Advantage**: When you switch from "NPC_A" to "NPC_B", the plugin only needs to switch the internal Ctx pointer. Since the KV Cache is already preserved in memory/VRAM, **the switching process has almost zero latency (< 1ms)**.

### 2.2 Async Inference Queue
The underlying inference logic runs in an independent thread pool.
1.  **Submit Task**: When you call the API, the task is pushed into a work queue.
2.  **Chunk Callback**: As the engine produces each token, the `OnChunk` delegate is triggered.
3.  **Auto Sync**: All callbacks are dispatched back to the Unreal Game Thread, so you can safely modify the UI or Actors within the callback.

---

## 3. Constrained Decoding

The low-level API supports powerful constrained decoding modes:
- **Regex**: Ensures the model output matches a specific regular expression (e.g., numbers only).
- **JSON**: Forces the model to output a valid JSON structure.
- **Lark**: Supports advanced logic constraint syntax.

These constraints are enforced during the **Token Generation Stage**, rather than checking after generation, greatly improving the accuracy of the model when performing "Tool Calls."

---

## 🧭 Roadmap

After understanding the low-level principles, you can:
- **[4. Plugin Workflow (SERVICE_LAYER.md)](SERVICE_LAYER.md)** - Learn how the Subsystem encapsulates these low-level calls.
- **[3. App Interface Layer (APP_LAYER.md)](APP_LAYER.md)** - See the out-of-the-box components we provide for the application layer.

---
*Powered by Winyunq - Ultimate Productivity Toolset.*
