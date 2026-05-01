# 快速开始指南 (Getting Started)

欢迎使用 **LiteRT-LM-Unreal**。本指南将帮助您在 5 分钟内完成环境搭建并实现第一次本地大模型推理。

## 1. 安装步骤

1.  **解压插件**：将 `LiteRT-LM-Unreal` 文件夹拷贝到您虚幻项目的 `Plugins` 目录下。
2.  **重新生成项目**：右键点击您的 `.uproject` 文件，选择 `Generate Visual Studio project files`。
3.  **编译项目**：在 IDE（VS/Rider）或编辑器中点击编译。
4.  **启用插件**：在编辑器 `Edit -> Plugins` 中确保 `LiteRT-LM-Unreal` 已勾选并重启。

## 2. 准备模型文件

本插件基于 Google 的 **LiteRT-LM** 引擎。
- 前往 [Google AI Edge 模型库](https://github.com/google-ai-edge/LiteRT-LM) 下载适配的模型（如 `Gemma-2B-IT`）。
- 将下载好的 `.bin` 文件放在您记得住的位置（推荐：`D:/Models/gemma.bin`）。

## 3. 实现第一次 AI 对话 (C++)

在您的 `Actor` 或 `Character` 中，使用以下代码唤醒 AI：

```cpp
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.h"

// 1. 加载模型
FLiteRtLmConfig Config;
Config.ModelPath = TEXT("D:/Models/gemma.bin");
FLiteRtLmUnrealApi::LoadModel(Config);

// 2. 发起对话
// 使用 'this' 作为上下文指针，自动管理 KV Cache 会话
FLiteRtLmUnrealApi::ChatWithPrompt(
    this, 
    TEXT("你好，请介绍一下你自己。"),
    TEXT("你是一个乐于助人的游戏向导。"),
    FLiteRtLmChunkCallback::CreateLambda([](const FString& Chunk) {
        // 实时处理流式输出（打字机效果）
        UE_LOG(LogTemp, Warning, TEXT("AI 正在输入: %s"), *Chunk);
    }),
    FLiteRtLmDoneCallback::CreateLambda([](const FLiteRtLmResult& Result) {
        // 推理完成后的回调
        UE_LOG(LogTemp, Display, TEXT("对话结束，完整回复: %s"), *Result.FullText);
    })
);
```

## 4. 蓝图支持 (Blueprints)

由于 API 涉及复杂的异步回调和 C++ 指针，我们目前强烈推荐使用 C++ 进行核心集成。
*注：蓝图专用的 API 节点正在开发中，预计在 v1.1 版本上线。*

## 5. 常见问题 (FAQ)

- **Q: 为什么加载模型时编辑器卡顿？**
  - A: 首次加载模型需要分配 VRAM 并编译着色器（如开启了 `OptimizeShader`）。建议在 Loading Screen 期间异步调用 `LoadModel`。
- **Q: 是否支持移动端？**
  - A: 目前已支持 Windows (Vulkan/DirectX)。Android/iOS 支持正在内部测试中。

---
*Winyunq Strategy: 极致性能，触手可及。*
