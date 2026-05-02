[EN](MODEL_MANAGEMENT.md) | **中文**

# 模型管理指南 (Model Management)

在 **LiteRT-LM-Unreal** 中，模型是驱动智能体验的核心。本插件针对虚幻引擎的生产环境进行了深度优化，确保模型加载高效且运行稳定。

## 1. 模型存储规范

为了保证跨平台打包的兼容性，建议将 `.bin` 或 `.gguf` 模型文件存放在项目的特定目录下：

- **推荐路径**：`Content/LiteRT/Models/`
- **文件格式**：支持 Google LiteRT-LM 专用格式（通常为 `.bin` 或经转换的 `.tflite` 兼容格式）。

> **⚠️ 注意**：虚幻引擎默认不会将非 `.uasset` 文件包含在打包包中。本插件会自动识别该路径，但请确保在 `Project Settings -> Packaging -> Additional Non-Asset Directories to Copy` 中手动添加此文件夹，以确保发布版能够读取。

## 2. 在编辑器中配置模型

您可以直接在虚幻引擎编辑器的全局 Subsystem 设置中进行初步配置，或者通过代码动态加载。

### 配置参数说明

| 参数名称 | 描述 | 推荐值 |
| :--- | :--- | :--- |
| **Model Path** | 模型的磁盘绝对路径或相对路径。 | `D:/Models/gemma-2b.bin` |
| **Backend** | 推理后端选择。`gpu` 适用于现代显卡，`cpu` 适用于低配设备。 | `gpu` |
| **Max Num Tokens** | KV Cache 预分配的大小，直接影响 VRAM 占用。 | `2048` |
| **Optimize Shader** | 是否启用着色器编译优化（Windows/Vulkan 环境必选）。 | `true` |

## 3. 动态加载与切换

通过 `ULiteRtLmSubsystem`，您可以在运行时随时切换模型：

```cpp
FLiteRtLmConfig MyConfig;
MyConfig.ModelPath = TEXT("D:/Models/gemma-2b-it.bin");
MyConfig.Backend = TEXT("gpu");

// 自动根据 VRAM 预算调整配置 (可选)
// MyConfig = FLiteRtLmUnrealApi::GetAutoConfig(4096); 

if (ULiteRtLmSubsystem::Get()->LoadModel(MyConfig))
{
    UE_LOG(LogTemp, Log, TEXT("Model Loaded Successfully!"));
}
```

## 4. VRAM 内存管理

插件内置了基于 **LRU (Least Recently Used)** 的会话清理机制。

- **多 Agent 支持**：您可以同时为多个 NPC 开启会话。
- **自动释放**：当显存不足时，系统会自动销毁最久未使用的 Session 指针。
- **性能监控**：使用 `FLiteRtLmUnrealApi::GetVRAMUsage()` 可以实时获取当前模型占用的显存大小（MB）。

---
*Powered by Winyunq Core Engineering - 提升每一兆显存的推理价值。*
