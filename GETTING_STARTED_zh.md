[EN](GETTING_STARTED.md) | **中文**

# LiteRT-LM Unreal 快速开始

本页针对当前 **UE 5.8 / LiteRT-LM v0.14 / Stable ABI 1.2** 插件。完整的单对话、多对话和 C++ 示例见[《单对话与多对话分步指南》](CONVERSATIONS_zh.md)。

## 1. 安装

1. 将插件安装到 Engine 或项目的 `Plugins/LiteRT-LM-Unreal`。
2. 在 `Edit > Plugins` 启用 **LiteRT-LM-Unreal** 并重启编辑器。
3. 在 `Project Settings > Plugins > LiteRT-LM` 设置 `Model Path`、上下文长度和缺省采样参数。
4. 模型由第一个 Conversation 自动懒加载，不需要 `Load Model` 蓝图节点。

## 2. 第一次 Blueprint 对话

1. 在 `BeginPlay` 调用 `Create Quick Chat`。
2. 将 Return Value 提升为 `QuickChat` 变量。
3. 绑定 `On Answer` 和 `On Error`；需要流式 UI 时再绑定 `On Text Chunk`。
4. 只调用 `Ask Once` 或 `Ask Streaming` 其中一个。
5. 继续复用同一个 `QuickChat` 变量，它会保留对话历史。

`Ask Once` / `Ask Streaming` 返回 Request Id；答案通过异步事件抵达。不要把返回值当作答案，也不要每条消息创建新 Quick Chat。

## 3. 单对话的兼容语义

Quick Chat 自动创建一个普通、缺省配置的 `ULiteRtLmAgent`。它不是全局单例，也不是另一套 Session 实现。调用 `Get Conversation` 可以获得同一个底层 Agent，并继续使用记忆、工具、导入导出和存档 API。

## 4. 多角色

每个 NPC/玩家创建一次 `Create Conversation (Advanced)`，把返回的 Agent 保存在数组或 Map 中。轮到谁就对谁调用 `Ask`。一局结束时调用 `Close and Clear Conversations`。

多对话完整步骤：[CONVERSATIONS_zh.md](CONVERSATIONS_zh.md)

## 5. 平台

- 当前场景层：文本，严格 GPU。
- 目标平台：Win64、Android arm64。
- 所有 Blueprint 事件在 Game Thread 广播。
- 一个进程共享一个模型和一条串行推理队列。
