**EN** | [中文](GETTING_STARTED_zh.md)

# LiteRT-LM Unreal Quick Start

This page targets the current **UE 5.8 / LiteRT-LM v0.14 / Stable ABI 1.2** plugin. For complete single-conversation, multiple-conversation, and C++ examples, read [Single and Multiple Conversations](CONVERSATIONS.md).

## 1. Install

1. Install the plugin under the Engine or project at `Plugins/LiteRT-LM-Unreal`.
2. Enable **LiteRT-LM-Unreal** under `Edit > Plugins`, then restart the Editor.
3. Set `Model Path`, context size, and default sampling under `Project Settings > Plugins > LiteRT-LM`.
4. The first Conversation starts lazy model loading automatically. There is no Blueprint `Load Model` node.

## 2. First Blueprint conversation

1. Call `Create Quick Chat` from `BeginPlay`.
2. Promote Return Value to a `QuickChat` variable.
3. Bind `On Answer` and `On Error`; bind `On Text Chunk` only for streaming UI.
4. Call either `Ask Once` or `Ask Streaming`, not both.
5. Keep using the same `QuickChat` variable so it retains conversation history.

`Ask Once` / `Ask Streaming` returns a Request Id. The answer arrives through asynchronous events. Do not treat the return value as the answer, and do not create a new Quick Chat for each message.

## 3. Single-conversation compatibility semantics

Quick Chat automatically creates an ordinary, default-configured `ULiteRtLmAgent`. It is neither a global singleton nor a separate Session implementation. `Get Conversation` returns that same Agent so you can adopt memory, tools, import/export, and persistence APIs.

## 4. Multiple characters

Create one `Create Conversation (Advanced)` object per NPC/player and retain each Agent in an array or map. Ask the Agent whose turn it is. At scene or match shutdown, call `Close and Clear Conversations`.

Complete multiple-conversation steps: [CONVERSATIONS.md](CONVERSATIONS.md)

## 5. Platforms

- Current scenario layer: text, strict GPU.
- Targets: Win64 and Android arm64.
- Blueprint events are delivered on the Game Thread.
- One process shares one model and one serial inference queue.
