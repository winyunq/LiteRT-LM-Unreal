[EN](API_LAYER.md) | **中文**

# LiteRT-LM Unreal: 5. 底层核心原理 (API Layer)

本章节面向 C++ 硬核开发者。我们将深入探讨插件最底层的原始接口 `FLiteRtLmUnrealApi` 以及其背后的核心运行逻辑。

> **提示**：如果您希望直接在蓝图中使用，请跳过此页阅读 **[3. 应用接口层](APP_LAYER_zh.md)**。

---

## 1. 核心静态接口：FLiteRtLmUnrealApi

这是插件与 Google LiteRT-LM 引擎之间的“纯血” C++ 桥梁。所有方法均为静态函数，不持有任何虚幻 UObject 状态，确保了最高的调用效率。

### 1.1 生命周期管理
- **`LoadModel(Config)`**: 将指定路径的模型加载进显存。内部会根据配置初始化后端（Vulkan/VEC/CPU）。
- **`UnloadModel()`**: 暴力释放所有推理相关的资源，确保显存被归还给系统。

### 1.2 原始推理接口
```cpp
static void GenerateOnce(
    const FString& Prompt, 
    FLiteRtLmChunkCallback OnChunk, 
    FLiteRtLmDoneCallback OnDone,
    const FLiteRtLmSamplingParams& Params
);
```
这是无状态的单次推理。它不会保留任何上下文，适用于不需要记忆的临时任务。

---

## 2. 核心原理：极速 KV Cache 映射

LiteRT-LM Unreal 最核心的技术优势在于其对 **Session (会话)** 的管理。

### 2.1 会话上下文句柄 (`void* Ctx`)
在 `ChatWithPrompt` 或 `ChatWithContext` 中，您会发现一个 `void* Ctx` 参数。
- **原理**：我们使用此指针作为唯一的 Hash Key，在内部会话池中查找对应的推理上下文。
- **优势**：当您从“NPC_A”切换到“NPC_B”时，插件只需切换内部的 Ctx 指针，由于 KV Cache 已经保存在内存/显存中，**切换过程几乎是 0 延迟 ( < 1ms )**。

### 2.2 异步推理队列
底层的推理逻辑是在独立的线程池中运行的。
1.  **提交任务**：当您调用 API 时，任务会被推入工作队列。
2.  **分块回调**：引擎产生每一个 Token 时，都会触发 `OnChunk` 委托。
3.  **自动同步**：所有回调都会被分发回虚幻的主线程（Game Thread），因此您可以在回调中安全地修改 UI 或 Actor。

---

## 3. 约束解码 (Constrained Decoding)

底层 API 支持强力的约束解码模式：
- **Regex**：确保模型输出符合特定的正则表达式（如：只要数字）。
- **JSON**：强制模型输出有效的 JSON 结构。
- **Lark**：支持高级逻辑约束语法。

这些约束是在 **Token 生成阶段** 强制执行的，而非生成后再检查，极大地提升了模型在执行“工具调用”时的准确性。

---

## 🧭 进阶之路

理解了底层原理后，您可以：
- **[4. 插件工作流 (SERVICE_LAYER_zh.md)](SERVICE_LAYER_zh.md)** - 了解 Subsystem 如何封装这些底层调用。
- **[3. 应用接口层 (APP_LAYER_zh.md)](APP_LAYER_zh.md)** - 查看我们为应用层提供的开箱即用组件。

---
*Powered by Winyunq - 极致生产力工具集。*
