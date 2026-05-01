# LiteRT-LM Unreal 开发文档

欢迎使用由 **Winyunq** 开发的 **LiteRT-LM-Unreal** 插件。这是一个专为虚幻引擎打造的高性能、轻量级本地大模型（LLM）集成方案。

## 📖 文档目录

1.  **[快速开始 (Getting Started)](GETTING_STARTED.md)**
    - 环境搭建、模型准备、首次推理。
2.  **[模型管理 (Model Management)](MODEL_MANAGEMENT.md)**
    - 存储规范、配置参数、VRAM 优化、LRU 机制。
3.  **[API 参考 (API Reference)](API_REFERENCE.md)**
    - Subsystem 接口、采样参数、结果结构体详述。
4.  **[核心理念 (Strategy & Pillars)](../README.md#🌟-our-logic--strategy)**
    - 了解我们如何通过 Session ID 映射实现极速缓存命中的设计哲学。

## 🚀 为什么选择 LiteRT-LM-Unreal？

相比于市面上动辄 $49 且集成质量良莠不齐的方案，我们坚持：
- **开源精神**：核心代码基于 Google LiteRT-LM，透明且可定制。
- **极致优化**：针对 13900HX/RTX 4060 等现代硬件深度优化，支持 GPU/CPU 后端无缝切换。
- **无感集成**：通过 `void* ctx` 会话管理，NPC 记忆切换耗时 < 1ms。
- **诚意价格**：仅需 $19，提供同等甚至超越竞品的稳定性与功能。

---
*战略由人，战术由 AI。LiteRT-LM-Unreal 助力您的游戏开启 AIGC 时代。*
