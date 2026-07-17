# LiteRT-LM Unreal

Local, GPU-only LiteRT-LM integration for Unreal Engine 5.8, with Blueprint-friendly conversation objects and a native C++ SDK path.

[中文文档](https://winyunq.github.io/LiteRT-LM-Unreal/CONVERSATIONS_zh.html) · [English guide](https://winyunq.github.io/LiteRT-LM-Unreal/CONVERSATIONS.html) · [Documentation home](https://winyunq.github.io/LiteRT-LM-Unreal/) · [LiteRT-LM binary fork](https://github.com/winyunq/LiteRT-LM/releases)

> The source currently visible on `master` is the legacy public baseline. The current UE 5.8/Fab implementation and its new C++ API are documented publicly, but this documentation update does not publish or synchronize the current plugin source.

## Conversation model

The current plugin uses one state model:

- `ULiteRtLmQuickChat` is the beginner-compatible, default single-conversation facade.
- Every Quick Chat wraps one ordinary `ULiteRtLmAgent`; it is not a global singleton or a second memory implementation.
- `Get Conversation` exposes that same Agent for memory, tools, import/export, and persistence without losing context.
- Multiple NPCs use one `ULiteRtLmAgent` per character, each with an independent canonical history.
- One process loads one shared model and serializes inference through one GPU request queue.

```text
Create Quick Chat -> default Agent -> memory A

Create Conversation (Advanced) -> Agent B -> memory B
                               -> Agent C -> memory C

All Conversations -> one shared model + one serial GPU queue
```

## Blueprint quick start

### One conversation

1. Call `Create Quick Chat` once during initialization.
2. Promote the return value to a `QuickChat` variable.
3. Bind `On Answer` and `On Error`; optionally bind `On Text Chunk`.
4. Call either `Ask Once` or `Ask Streaming` on the same object for every message.
5. Use `Reset Conversation`, `Cancel`, or `Close` for lifecycle control.

The Ask return value is a Request Id. Answers arrive asynchronously through events on the Game Thread.

### Multiple conversations

1. Build one `Agent Config` per NPC/player.
2. Call `Create Conversation (Advanced)` once per character.
3. Store the Agents in an array or map and bind each Agent's events.
4. Ask the Agent whose turn it is.
5. Send public facts to selected Agents with `Append Memory Message to Conversations`; send private facts only to the authorized Agent.
6. End the scene with `Close and Clear Conversations`.

For the complete, UE 5.8-verified wiring guide, see [Single and Multiple Conversations](https://winyunq.github.io/LiteRT-LM-Unreal/CONVERSATIONS.html).

## C++ surface

The current build separates two responsibilities:

- UE scenario API: `ULiteRtLmSubsystem`, `ULiteRtLmQuickChat`, `ULiteRtLmAgent`, `ULiteRtLmMcpGateway`, and `ULiteRtLmComponent`.
- Native SDK access: the official LiteRT-LM v0.14 C API plus no-exception RAII/convenience wrappers behind Stable ABI 1.2 packaging.

Blueprint retains one unambiguous reflected signature per operation. C++ receives convenience overloads for common `CreateAgent`, `Ask`, `AskMessagesJson`, `SubmitToolResult`, `AskStreaming`, and `AskOnce` calls.

## Runtime facts

- Unreal Engine: 5.8
- Targets: Win64 and Android arm64
- Current scenario modality: text
- Runtime policy: strict hardware GPU; no silent CPU fallback
- Model ownership: one process-wide model
- Scheduling: one serial inference queue
- Memory: one independent canonical history per Agent

On the current Win64 GPU runtime, consecutive requests to the same Agent can reuse resident KV. Switching Agents rebuilds the selected Agent from its complete canonical history when required. This preserves memory semantics while cross-Agent switching may add prefill cost; the plugin does not claim that every Agent's physical KV is simultaneously resident.

## 中文摘要

当前 `QuickChat` 是普通 `Agent/Conversation` 的缺省兼容外观，而不是第二套会话实现。每次 `Create Quick Chat` 创建一条独立 Conversation；通过 `Get Conversation` 可无损进入高级接口。多 NPC 应按“一名角色一个 Agent”组织，不能每条消息重建对象，也不能让所有角色共用同一个 Agent。

完整中文步骤：[单对话与多对话指南](https://winyunq.github.io/LiteRT-LM-Unreal/CONVERSATIONS_zh.html)。

## License

The legacy public baseline in this repository remains under the repository's [MIT License](LICENSE). LiteRT-LM and redistributed runtime binaries remain subject to their respective upstream licenses and release notices.
