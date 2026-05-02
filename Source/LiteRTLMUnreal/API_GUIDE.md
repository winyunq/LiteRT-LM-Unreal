# LiteRT-LM Unreal API Implementation Guide

This document provides a detailed explanation of the implementation and functionality of the LiteRT-LM Unreal plugin, as required for Fab/Marketplace technical review.

## 1. Architectural Overview

The plugin follows a **Bridge Pattern** to integrate Google's LiteRT-LM (TensorFlow Lite) into Unreal Engine 5.

### 1.1 The C-Style "Firewall" DLL
LiteRT-LM carries heavy dependencies (Abseil, Protobuf, Flatbuffers). To prevent these from polluting the Unreal Engine global namespace and causing version conflicts, we encapsulate the entire inference engine inside a standalone C++ DLL (`litert_lm_wrapper.dll`).

- **Interface**: The DLL exports a minimalist `extern "C"` interface.
- **Loader**: `FLiteRtLmWrapperLoader` dynamically loads this DLL at runtime using `FPlatformProcess::GetDllHandle`, ensuring the plugin remains "Zero-Link" and easy to redistribute.

## 2. Core Functional Modules

### 2.1 ULiteRtLmSubsystem (Resource Manager)
A `UEngineSubsystem` that manages the global lifecycle of the LiteRT engine.
- **Singleton Pattern**: Ensures only one instance of the heavy GPU engine exists.
- **VRAM Management**: Implements `QueryAvailableVramMB` via DXGI to automatically suggest optimal configurations (e.g., capping at 4GB by default).

### 2.2 FLiteRtLmUnrealApi (High-Level Interface)
The primary entry point for developers. It wraps the low-level DLL calls into idiomatic Unreal C++ (using `FString`, `TSharedPtr<FJsonObject>`, and Delegates).

#### Key Functions:
- `SendChatRequest`: The core inference function. It handles:
  1. **Message Normalization**: Merges 'system' prompts into the next 'user' message (as Gemma models lack a native system role).
  2. **Incremental Sync**: Only sends new messages to the DLL to utilize the KV-cache effectively.
  3. **Thread Safety**: Offloads inference to `ENamedThreads::AnyBackgroundThreadNormalTask` to keep the Game Thread responsive.

### 2.3 Session & KV-Cache Strategy
One of the plugin's strongest features is its **Context Mapping**.
- **Strategy**: It uses a `TMap<void*, void*>` to map any Unreal Object (usually an Agent Component or Actor) to a specific LiteRT Session.
- **Benefit**: This allows multiple AI Agents to exist in the same level. Switching between them is nearly instantaneous (<1ms) because the GPU-side KV-cache is preserved per-pointer.

## 3. Inference Flow

1. **User Call**: Developer calls `SendChatRequest` with a unique pointer (SessionKey).
2. **Synchronization**: The plugin checks the message count for that key and appends only the delta to the session.
3. **Async Trigger**: `LiteRtLm_RunInference` is called on a background thread.
4. **Event Driving**: The background thread enters a `WaitUntilDone` loop. This is critical because the underlying WebGPU backend is asynchronous; this loop drives the message pump to trigger streaming callbacks.
5. **Callback Marshalling**: Streaming text and final results are marshalled back to the **Game Thread** via `AsyncTask(ENamedThreads::GameThread, ...)` before being delivered to the user's delegates.

## 4. Best Practices for Developers

- **Model Placement**: Place `.litertlm` models in `Content/Models/`.
- **Streaming**: Always bind to the `FLiteRtLmChunkCallback` for a smooth "typewriter" UI effect.
- **Memory**: The plugin handles session cleanup automatically, but developers can manually call `ReleaseSession` if an Agent is destroyed.

---
*Developed by Winyunq. For technical support, visit github.com/winyunq/LiteRT-LM-Unreal*
