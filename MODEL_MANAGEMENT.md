**EN** | [中文](MODEL_MANAGEMENT_zh.md)

# Model Management Guide

In **LiteRT-LM-Unreal**, the model is the core driving the intelligent experience. This plugin is deeply optimized for Unreal Engine's production environment, ensuring efficient loading and stable operation.

## 1. Model Storage Specification

To ensure compatibility across platforms after packaging, it is recommended to store `.bin` or `.gguf` model files in a specific directory within your project:

- **Recommended Path**: `Content/LiteRT/Models/`
- **File Format**: Supports Google LiteRT-LM specific formats (usually `.bin` or converted `.tflite` compatible formats).

> **⚠️ Note**: Unreal Engine does not include non-`.uasset` files in the packaged build by default. While this plugin automatically identifies the path, please ensure you manually add this folder in `Project Settings -> Packaging -> Additional Non-Asset Directories to Copy` to ensure the release version can read it.

## 2. Configure Models in the Editor

You can perform initial configuration directly in the Global Subsystem settings within the Unreal Editor or load models dynamically via code.

### Configuration Parameter Description

| Parameter Name | Description | Recommended Value |
| :--- | :--- | :--- |
| **Model Path** | Absolute or relative disk path to the model. | `D:/Models/gemma-2b.bin` |
| **Backend** | Inference backend selection. `gpu` for modern graphics cards, `cpu` for lower-end devices. | `gpu` |
| **Max Num Tokens** | Size of the KV Cache pre-allocation, directly affecting VRAM usage. | `2048` |
| **Optimize Shader** | Whether to enable shader compilation optimization (Mandatory for Windows/Vulkan). | `true` |

## 3. Dynamic Loading & Switching

Via `ULiteRtLmSubsystem`, you can switch models at any time during runtime:

```cpp
FLiteRtLmConfig MyConfig;
MyConfig.ModelPath = TEXT("D:/Models/gemma-2b-it.bin");
MyConfig.Backend = TEXT("gpu");

// Automatically adjust configuration based on VRAM budget (Optional)
// MyConfig = FLiteRtLmUnrealApi::GetAutoConfig(4096); 

if (ULiteRtLmSubsystem::Get()->LoadModel(MyConfig))
{
    UE_LOG(LogTemp, Log, TEXT("Model Loaded Successfully!"));
}
```

## 4. VRAM Memory Management

The plugin has a built-in session cleanup mechanism based on **LRU (Least Recently Used)**.

- **Multi-Agent Support**: You can open sessions for multiple NPCs simultaneously.
- **Automatic Release**: When VRAM is insufficient, the system automatically destroys the least recently used Session pointer.
- **Performance Monitoring**: Use `FLiteRtLmUnrealApi::GetVRAMUsage()` to get the current model's VRAM usage (MB) in real-time.

---
*Powered by Winyunq Core Engineering - Maximizing inference value for every MB of VRAM.*
