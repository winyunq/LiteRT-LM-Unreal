# LiteRT-LM Unreal (v2.1)

High-performance, local LLM inference integration for Unreal Engine 5. This plugin provides a clean C++ API to run LiteRT-LM models with ultra-low latency and efficient KV-cache management.

## 🚀 Key Features

- **Strategic KV Cache**: Direct control over session persistence for instant "context-aware" responses.
- **Zero-Dependency ABI**: Clean C-style wrapper to avoid version conflicts with Unreal's internal libraries (Abseil, Protobuf, etc.).
- **Async & Thread-Safe**: Non-blocking inference with easy-to-use GameThread callbacks.
- **MCP Tool Support**: Built-in support for tool-calling patterns.

---

## 🛠️ API Specification

### 1. Configuration & Lifecycle

#### `LiteRtLm_Config`
`cpp
struct LiteRtLm_Config {
    const char* model_path;      // Path to the .gguf/model file
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
| **`LiteRtLm_GetAutoConfig`** | Generates the best `LiteRtLm_Config` based on VRAM budget. |
| **`LiteRtLm_LoadModel`** | Initializes the engine and pre-allocates VRAM pools. |
| **`LiteRtLm_UnloadModel`** | Releases all GPU and memory resources. |

---

### 2. Inference & Session Management

| Function | Scenario & Strategy |
| :--- | :--- |
| **`LiteRtLm_GenerateOnce`** | **One-Shot Task**. Temporary cache is destroyed immediately after generation. |
| **`LiteRtLm_ChatWithPrompt`** | **Agent Startup**. Creates the first persistent cache for a given `void* ctx`. |
| **`LiteRtLm_ChatWithContext`** | **Conversational/MCP**. Achieves Cache-Hit via `ctx` pointer for incremental prefill. |

---

### 3. Usage Examples (UE5 C++ Style)

#### Example 1: Multi-turn Conversation (Cache-Hit)
`cpp
// 1. Setup
LiteRtLm_Config Config = LiteRtLm_GetAutoConfig(4096); // Reserve 4GB VRAM
LiteRtLm_LoadModel(Config);

// 2. First Turn (Agent A)
// Passing 'this' as context; the system creates a dedicated GPU session for this object.
LiteRtLm_ChatWithPrompt(this, "You are a guide.", "Hello!", OnChunk, OnDone, nullptr);

// 3. Subsequent Turn (Agent A)
// Using the same 'this' pointer triggers a cache-hit. 
// Only the new message is processed, resulting in near-instant response.
LiteRtLm_ChatWithContext(this, HistoryJson, OnChunk, OnDone, nullptr);
`

#### Example 2: MCP Tool Use
`cpp
// 1. Send request with tool definitions in HistoryJson
LiteRtLm_ChatWithContext(this, HistoryWithTools, nullptr, [](LiteRtLm_Result res, void* p){
    if (res.tool_calls) {
        // 2. AI requested a tool. Execute locally.
        FString Result = ExecuteLocalTool(res.tool_calls);
        
        // 3. Send result back. KV Cache ensures fast prefill.
        LiteRtLm_ChatWithContext(this, HistoryWithToolResult, OnChunk, OnDone, nullptr);
    }
}, nullptr);
`

---

## ⚡ Performance Guidelines

1. **Cache Hit Policy**: The plugin uses a `TMap<void*, Session*>` to track sessions. As long as the `void* ctx` remains consistent, the GPU state is reused.
2. **Auto-VRAM Cleanup**: When VRAM is low, the system automatically releases the least recently used (LRU) sessions.
3. **Low Latency Switching**: Switching between multiple active Agents takes <1ms on an RTX 4060.

---
*Open Sourced under MIT License. Part of the Winyunq Core Ecosystem.*
