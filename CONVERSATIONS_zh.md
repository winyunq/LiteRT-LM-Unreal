# LiteRT-LM Unreal：单对话与多对话分步指南

[English](CONVERSATIONS.md) | **中文**

本文针对当前 **Unreal Engine 5.8 / LiteRT-LM v0.14 / Stable ABI 1.2** 插件。节点名称和引脚已在 UE5.8 编辑器中核对。

## 先理解一个原则

`ULiteRtLmQuickChat` 不是第二套会话系统。它只是一个普通 `ULiteRtLmAgent`（Conversation）的缺省配置兼容外观：

```text
Create Quick Chat
  -> 创建一个普通 Agent / Conversation
  -> 自动采用缺省配置
  -> 代为转发流式文本、完成、错误和状态事件
```

因此：

- 单对话和多对话共用同一套记忆、队列、取消和生命周期实现。
- 每次调用 `Create Quick Chat` 都会创建一个普通的独立 Conversation；它不是进程级单例。
- `Get Conversation` 可取出底层 `ULiteRtLmAgent`，继续使用记忆、工具和存档 API，不会丢失已有上下文。
- 一个进程只加载一份共享模型；多个 Conversation 各自保留规范历史，请求由 Subsystem 串行送入 GPU。

## 0. 项目准备

1. 在 `Edit > Plugins` 启用 **LiteRT-LM-Unreal**，重启编辑器。
2. 打开 `Project Settings > Plugins > LiteRT-LM`。
3. 设置 `Model Path`，以及上下文长度和缺省采样参数。
4. 不要寻找 `Load Model` 节点。创建第一个 Quick Chat 或 Conversation 时会自动触发懒加载。

## 1. 快速单对话（Blueprint）

适用场景：聊天窗口、单个助手、教程 NPC，以及任何只需要一条连续历史的功能。

### 第一步：创建并保存对象

在拥有合适生命周期的 Blueprint（例如 Player Controller、Game Instance 子系统或聊天 Widget 的长期 owner）中：

1. 从 `BeginPlay` 或初始化事件调用 `Create Quick Chat`。
2. 在 `System Prompt` 填写稳定的角色说明；可以留空。
3. 将 `Return Value` **提升为变量**，命名为 `QuickChat`。

不要在每次发送消息时重新调用 `Create Quick Chat`。重新创建对象就等于开始另一条对话。

### 第二步：绑定结果事件

从 `QuickChat` 变量绑定：

- `On Answer`：每个被接受的请求最终都会在这里给出完整 `FLiteRtLmResult`。
- `On Error`：显示或记录结构化错误。
- `On Text Chunk`：仅在需要打字机效果时绑定。
- `On State Changed`：可用于禁用发送按钮或显示加载/生成状态。

事件会在 Game Thread 触发，可以直接更新 UMG。

### 第三步：选择一种提问方式

发送按钮只调用其中一个节点：

- `Ask Once(Message, Options)`：不转发文本块，只通过 `On Answer` 给出完整答案。
- `Ask Streaming(Message, Options)`：通过 `On Text Chunk` 逐块输出，结束时仍会触发一次 `On Answer`。

两个节点的 `Return Value` 都是 **Request Id**，不是答案。非空表示请求已被接受；答案是异步事件。

### 第四步：继续同一上下文

下一次提问继续对同一个 `QuickChat` 变量调用 `Ask Once` 或 `Ask Streaming`。底层 Agent 会保留之前的用户和 assistant 消息。

### 第五步：重置、取消和关闭

- 新话题：`Reset Conversation(true)`，清除历史但保留 System Prompt。
- 停止当前生成：`Cancel`。
- owner 永久退出：`Close`，并清空你的变量。
- 需要高级 API：`Get Conversation`，取得同一个底层 Agent。

### UE5.8 核对过的单对话节点

| 节点 | 主要输入 | 输出/事件 |
| --- | --- | --- |
| `Create Quick Chat` | `System Prompt` | `Lite Rt Lm Quick Chat` |
| `Ask Once` | `Message`, `Options` | `Request Id`; 最终结果走 `On Answer` |
| `Ask Streaming` | `Message`, `Options` | `Request Id`; 文本走 `On Text Chunk`，最终结果走 `On Answer` |
| `Get Conversation` | Quick Chat | 同一 `Lite Rt Lm Agent` |
| `Reset Conversation` | `Keep System Prompt` | 是否成功 |

## 2. 快速单对话（C++）

在模块的 `Build.cs` 中依赖 `LiteRTLMUnreal`。用 `UPROPERTY` 持有对象：

```cpp
#include "LiteRtLmBlueprintLibrary.h"
#include "LiteRtLmQuickChat.h"

UPROPERTY(Transient)
TObjectPtr<ULiteRtLmQuickChat> QuickChat;

UFUNCTION()
void HandleAnswer(const FLiteRtLmResult& Result);

UFUNCTION()
void HandleChunk(const FString& RequestId, const FString& TextChunk);

UFUNCTION()
void HandleChatError(const FLiteRtLmError& Error);
```

在初始化阶段创建一次：

```cpp
QuickChat = ULiteRtLmBlueprintLibrary::CreateQuickChat(
    this,
    TEXT("Answer clearly and remember the ongoing conversation."));

if (QuickChat)
{
    QuickChat->OnAnswer.AddDynamic(this, &ThisClass::HandleAnswer);
    QuickChat->OnTextChunk.AddDynamic(this, &ThisClass::HandleChunk);
    QuickChat->OnError.AddDynamic(this, &ThisClass::HandleChatError);
}
```

发送时使用 C++ 缺省参数重载：

```cpp
const FString RequestId = QuickChat
    ? QuickChat->AskStreaming(TEXT("Remember that my codename is Orion."))
    : FString();
```

Blueprint 只有一个无歧义反射签名；C++ 额外获得省略 `FLiteRtLmAskOptions` 的便利重载。

## 3. 多对话（Blueprint）

适用场景：多个 NPC、狼人杀、队伍成员、多个聊天标签页，或需要相互隔离记忆的工作流。

### 第一步：准备容器

创建一个 `Lite Rt Lm Agent Object Reference` 数组，例如 `Conversations`。如果角色有稳定 Id，推荐再维护 `Role Id -> Agent` 的 Map。

### 第二步：每个角色创建一次 Conversation

在开局/场景初始化时，对每个角色执行：

1. `Make Lite Rt Lm Agent Config`。
2. 填写 `Display Name`、该角色独有的 `System Prompt`，以及可选的 `Tool Declarations Json`。
3. 调用 `Create Conversation (Advanced)`。
4. 将返回的 Agent 加入 `Conversations` 数组。

狼人杀应当是“一名 AI 玩家一个 Agent”，不是所有玩家共用一个 Agent，也不是每回合创建一个 Agent。

### 第三步：为每个 Agent 绑定事件

绑定 `On Text Chunk`、`On Completed`、`On Error` 和可选的 `On State Changed`。共享处理函数时，应通过 owner、Agent Id 或你保存的请求映射识别是哪名角色完成了请求。

### 第四步：轮到谁就询问谁

从数组或 Map 取出当前角色的 Agent，调用 `Ask(Message, Options)`。同一个 Agent 忙碌时不要再次提交；不同 Agent 的请求也会由共享 GPU 队列串行执行。

### 第五步：只同步角色确实知道的事实

- 公共事件可调用 `Append Memory Message to Conversations` 广播给所有合法接收者。
- 私密事件只调用目标 Agent 的 `Append Memory Message`。
- 记录事件本身即可，例如 `3号玩家发言：“我是狼人。”`。不要额外拼接“该信息已公开，以后必须……”之类提示词；谁能收到该事件由游戏规则层决定。

### 第六步：处理工具调用和规则拒绝

游戏动作仍由规则层校验：

1. 从 `On Completed` 的 `Result.ToolCalls` 读取结构化动作。
2. 校验座位、阶段、身份和目标是否合法。
3. 合法且无需模型续写：`Append Tool Result to Memory`。
4. 合法且需要模型继续解释：`Submit Tool Result`。
5. 回答非法且准备重新询问：`Reject Last Response`，再发送纠错请求，避免错误 assistant 消息留在记忆里成为错误范例。

### 第七步：结束场景

对数组调用 `Close and Clear Conversations`。该节点会关闭唯一有效 Agent，并清空调用方数组，避免上一局的回调进入下一局。

### UE5.8 核对过的多对话节点

| 节点 | 用途 |
| --- | --- |
| `Create Conversation (Advanced)` | 根据 `Agent Config` 创建一个独立 Conversation |
| `Ask` | 对指定 Agent 提交请求，返回 Request Id |
| `Append Memory Message to Conversations` | 向一组 Agent 写入同一条真实事件 |
| `Close and Clear Conversations` | 批量关闭并清空数组 |
| `Export/Import/Save/Load Memory` | 会话检查、迁移和存档 |
| `Reject Last Response` | 删除最后一条终态 assistant 回答并保留输入 |

## 4. 多对话（C++）

```cpp
#include "LiteRtLmAgent.h"
#include "LiteRtLmSubsystem.h"

UPROPERTY(Transient)
TArray<TObjectPtr<ULiteRtLmAgent>> Conversations;

ULiteRtLmSubsystem* Runtime =
    GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>();

ULiteRtLmAgent* Seat3 = Runtime
    ? Runtime->CreateAgent(
        TEXT("Seat 3"),
        TEXT("You are seat 3 in a social deduction game. Follow your role and the game rules."))
    : nullptr;

if (Seat3)
{
    Conversations.Add(Seat3);
    Seat3->OnCompleted.AddDynamic(this, &ThisClass::HandleAgentCompleted);
    Seat3->OnError.AddDynamic(this, &ThisClass::HandleAgentError);
    Seat3->Ask(TEXT("Give your current analysis."));
}
```

`CreateAgent` 和 `Ask` 的这些短签名是 C++ 便利重载；它们与 Blueprint 使用的是同一个 Agent 实现。

## 5. 记忆正确性检查

调试“AI 像没看历史”时，按顺序检查：

1. 是否在每条消息前重新创建 Quick Chat/Agent。
2. 多角色是否错误共用了一个 Agent。
3. 角色可见的真实事件是否写入了对应 Agent，而不是只显示在 UI。
4. `Ask` 是否返回非空 Request Id；空 Id 代表请求没有被接受。
5. 是否在 Agent 忙碌时修改记忆或再次询问。
6. 使用 `Export Conversation Json` / `Export Memory Json` 检查下一次推理前的规范历史。
7. 规则层拒绝错误工具动作后，是否调用了 `Reject Last Response`。

物理 KV 不是公开状态。当前 Win64 GPU 运行时下，同一 Agent 的连续请求可复用当前常驻 KV；切换 Agent 时会从该 Agent 的完整规范历史重建上下文。这样保证语义不丢失，代价是跨 Agent 切换可能产生额外 prefill。

## 6. 选择建议

| 需求 | 入口 |
| --- | --- |
| 一个聊天窗口，尽快得到答案 | `Create Quick Chat` |
| 单对话但需要存档、工具或精细记忆操作 | Quick Chat + `Get Conversation` |
| 多 NPC / 多玩家 / 多标签页 | 每个角色一个 `Create Conversation (Advanced)` |
| Actor 自己拥有长期 AI | `LiteRtLmComponent` |
| 固定工具 schema、外部 MCP 路由 | `Create MCP Gateway` |
