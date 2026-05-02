**EN** | [中文](INDEX_zh.md)

# LiteRT-LM Unreal Documentation

Welcome to the **LiteRT-LM-Unreal** plugin developed by **Winyunq**. This is a high-performance, lightweight local Large Language Model (LLM) integration solution specially built for Unreal Engine.

## 📖 Table of Contents

1.  **[Getting Started](GETTING_STARTED.md)**
    - Environment setup, model preparation, first inference.
2.  **[Model Management](MODEL_MANAGEMENT.md)**
    - Storage standards, configuration parameters, VRAM optimization, LRU mechanism.
3.  **[API Reference](API_REFERENCE.md)**
    - Subsystem interfaces, sampling parameters, result struct details.
4.  **[Core Strategy & Pillars](../README.md#🌟-our-logic--strategy)**
    - Learn about our design philosophy of achieving lightning-fast cache hits through Session ID mapping.

## 🚀 Why choose LiteRT-LM-Unreal?

Compared to solutions on the market that often cost $49 with inconsistent integration quality, we insist on:
- **Open Source Spirit**: Core code is based on Google LiteRT-LM, transparent and customizable.
- **Extreme Optimization**: Deeply optimized for modern hardware such as 13900HX/RTX 4060, supporting seamless GPU/CPU backend switching.
- **Zero-Friction Integration**: Through `void* ctx` session management, NPC memory switching takes < 1ms.
- **Sincere Pricing**: Only $19, providing stability and functionality equal to or even exceeding competitors.

---
*Strategy by Human, Tactics by AI. LiteRT-LM-Unreal empowers your game to enter the AIGC era.*
