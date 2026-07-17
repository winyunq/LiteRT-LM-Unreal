[EN](INDEX.md) | **中文**

# LiteRT-LM Unreal 开发文档

欢迎使用由 **Winyunq** 开发的 **LiteRT-LM-Unreal** 插件。这是一个专为虚幻引擎打造的高性能、轻量级本地大模型（LLM）集成方案。

## 📖 文档目录

1.  **[快速开始 (Getting Started)](GETTING_STARTED_zh.md)**
    - 安装、模型设置和第一次 Quick Chat。
2.  **[单对话与多对话](CONVERSATIONS_zh.md)**
    - UE5.8 已核对的 Blueprint/C++ 步骤、生命周期与记忆排错。
3.  **[模型管理 (Model Management)](MODEL_MANAGEMENT_zh.md)**
    - 存储规范、配置参数、规范历史与显存边界。
4.  **[API 参考 (API Reference)](API_REFERENCE_zh.md)**
    - Subsystem 接口、采样参数、结果结构体详述。
5.  **[GitHub 仓库](https://github.com/winyunq/LiteRT-LM-Unreal)**
    - 公开的旧版源码基线与当前公开文档。

## 🚀 为什么选择 LiteRT-LM-Unreal？

- **统一对象模型**：Quick Chat 是普通缺省 Agent 的兼容外观，不维护第二份状态。
- **独立角色记忆**：多 NPC 每个角色一个 Agent，各自保留完整规范历史。
- **共享 GPU 模型**：一个模型、一条串行队列，不为每个角色复制模型。
- **C++ 优先**：Blueprint 签名保持唯一；C++ 提供常用重载与完整 native SDK 入口。

---
*战略由人，战术由 AI。LiteRT-LM-Unreal 助力您的游戏开启 AIGC 时代。*
