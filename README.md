# LiteRT-LM Unreal (v1.0)

High-performance, local LLM inference integration for Unreal Engine 5, powered by **Google's LiteRT (formerly MediaPipe LLM Inference)**. 

This plugin provides a clean, zero-dependency C++ API to run Google's LiteRT-LM models with ultra-low latency and strategic KV-cache management, optimized for real-time applications and game agents.

## 🌟 About LiteRT-LM
**LiteRT** is Google's high-performance library for on-device machine learning. The LLM Inference API allows you to run large language models completely on-device, offering:
- **Privacy & Speed**: No data leaves the device; inference happens locally on CPU or GPU (Vulkan/WebGPU).
- **Cross-Platform**: Built on top of the same technology that powers Gemini Nano.
- **Efficiency**: Optimized for mobile and desktop hardware with advanced quantization support.

---

## 🚀 Key Features

- **Strategic KV Cache**: Direct control over session persistence for instant "context-aware" responses.
- **Zero-Dependency ABI**: Clean C-style wrapper to avoid version conflicts with Unreal's internal libraries (Abseil, Protobuf, etc.).
- **Async & Thread-Safe**: Non-blocking inference with easy-to-use GameThread delegates.
- **Multi-Agent Isolation**: Manage multiple conversation states (KV Caches) simultaneously without memory cross-talk.

---

## 🛠️ API Specification

### 1. Configuration & Lifecycle

#### LiteRtLm_Config
`cpp
struct LiteRtLm_Config {
    const char* model_path;      // Path to the .bin/model file
    const char* backend;         // "cpu", "gpu" (vulkan/webgpu)
    int max_num_tokens;          // Context window size (KV Cache pre-allocation)
    int num_threads;             // CPU threads (for CPU backend)
    bool enable_benchmark;       // Print performance logs
    bool optimize_shader;        // Enable shader optimization (Windows recommended)
    bool enable_streaming;       // Enable streaming chunk callbacks
};
`

| Function | Description |
| :--- | :--- |
| **LiteRtLm_GetAutoConfig** | Generates the best LiteRtLm_Config based on VRAM budget. |
| **LiteRtLm_LoadModel** | Initializes the engine and pre-allocates VRAM pools. |
| **LiteRtLm_UnloadModel** | Releases all GPU and memory resources. |

---

### 2. Inference & Session Management

| Function | Scenario & Strategy |
| :--- | :--- |
| **LiteRtLm_GenerateOnce** | **One-Shot Task**. Temporary cache is destroyed immediately after generation. |
| **LiteRtLm_ChatWithPrompt** | **Agent Startup**. Creates the first persistent cache for a given oid* ctx. |
| **LiteRtLm_ChatWithContext** | **Conversational/MCP**. Achieves Cache-Hit via ctx pointer for incremental prefill. |

---

### 3. Usage Examples (UE5 C++ Style)

#### Example: Multi-turn Conversation (Cache-Hit)
`cpp
// 1. Setup
FLiteRtLmConfig Config;
Config.ModelPath = TEXT("D:/Models/gemma-2b-it.bin");
ULiteRtLmSubsystem::Get()->LoadModel(Config);

// 2. First Turn (Agent A)
// Passing 'this' as context; the system creates a dedicated GPU session for this object.
FLiteRtLmUnrealApi::ChatWithPrompt(this, "You are a guide.", "Hello!", OnChunk, OnDone);

// 3. Subsequent Turn (Agent A)
// Using the same 'this' pointer triggers a cache-hit. 
// Only the new message is processed, resulting in near-instant response.
FLiteRtLmUnrealApi::ChatWithContext(this, HistoryJson, OnChunk, OnDone);
`

---

## ⚡ Performance Guidelines

1. **Cache Hit Policy**: The plugin uses a TMap<void*, Session*> to track sessions. As long as the oid* ctx remains consistent, the GPU state is reused.
2. **Auto-VRAM Cleanup**: When VRAM is low, the system automatically releases the least recently used (LRU) sessions.
3. **Low Latency Switching**: Switching between multiple active Agents takes <1ms on an RTX 4060.

---
*Open Sourced under MIT License. Developed by Winyunq Core Engineering.*
