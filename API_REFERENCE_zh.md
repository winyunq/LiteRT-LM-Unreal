[EN](API_REFERENCE.md) | **中文**

# API 详细参考 (API Reference)

本文档提供了 **LiteRT-LM-Unreal** 核心类和结构体的技术细节。

## 1. ULiteRtLmSubsystem (Engine Subsystem)

全局单例，负责模型生命周期和会话池管理。

### 主要方法
- `bool LoadModel(const FLiteRtLmConfig& InConfig)`：加载并初始化引擎。
- `void UnloadModel()`：释放所有显存和内存资源。
- `void* GetOrCreateSession(void* Ctx, const FString& JsonPreface = TEXT(""))`：内部使用，根据上下文指针获取或创建推理会话。

---

## 2. FLiteRtLmUnrealApi (Static Helper)

... (省略静态方法描述) ...

---

## 3. ULiteRtLmComponent (Actor Component)

这是专为 NPC 和交互对象设计的 C++ 组件。它将底层的会话管理封装为蓝图可用的事件。

### 主要属性
- `SystemPrompt`: 字符串，定义该 NPC 的性格和背景。
- `SamplingParams`: 采样参数（Temperature, TopP 等）。

### 主要方法
- `SendChatMessage(FString Message)`：向该 NPC 发送消息，自动处理 KV Cache 记忆。
- `ResetConversation()`：清空该 NPC 的对话历史。

### 蓝图事件
- `OnTextChunkReceived`: 当流式文本块到达时触发。
- `OnInferenceCompleted`: 当完整回复生成结束时触发，包含详细性能数据。

---

## 4. 核心结构体 (Core Structs)

### FLiteRtLmConfig
控制引擎启动参数。
- `ModelPath`: 字符串，模型路径。
- `Backend`: `gpu` (默认) 或 `cpu`。
- `MaxNumTokens`: KV Cache 总容量。
- `bOptimizeShader`: 是否在 Windows/Vulkan 上优化 Shader。

### FLiteRtLmSamplingParams
控制文本生成质量。
- `Temperature`: 随机性 (0.0 - 1.0)。
- `TopP`: 核采样阈值。
- `TopK`: Top-K 采样限制。
- `ConstraintType`: 约束解码模式（`Regex`, `Json`, `Lark`）。
- `ConstraintString`: 对应的约束规则（如正则表达式或 JSON Schema）。

### FLiteRtLmResult
推理任务完成后的详细数据包。
- `FullText`: 生成的完整文本。
- `FullJson`: 如果模型输出了 JSON 或调用了工具，此处包含结构化数据。
- `TimeMs`: 总推理耗时。
- `TokensPerSec`: 每秒生成的 Token 数（吞吐量指标）。

---

## 5. 委托回调 (Delegates)

- `FLiteRtLmChunkCallback`：当模型输出一个新的 Token 或文本块时触发。
- `FLiteRtLmDoneCallback`：当推理完全结束（或出错）时触发。

> **💡 专家提示**：回调函数是在虚幻的主线程（Game Thread）中执行的，您可以直接在其中修改 UI 或更新 Actor 状态。

---
*Powered by Winyunq - 极致生产力工具集。*
