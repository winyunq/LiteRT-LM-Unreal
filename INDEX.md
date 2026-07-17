**EN** | [中文](INDEX_zh.md)

# LiteRT-LM Unreal Documentation

Welcome to the **LiteRT-LM-Unreal** plugin developed by **Winyunq**. This is a high-performance, lightweight local Large Language Model (LLM) integration solution specially built for Unreal Engine.

## 📖 Table of Contents

1.  **[Getting Started](GETTING_STARTED.md)**
    - Installation, model settings, and the first Quick Chat.
2.  **[Single and Multiple Conversations](CONVERSATIONS.md)**
    - UE 5.8-verified Blueprint/C++ steps, lifetime, and memory debugging.
3.  **[Model Management](MODEL_MANAGEMENT.md)**
    - Storage, configuration, canonical history, and VRAM boundaries.
4.  **[API Reference](API_REFERENCE.md)**
    - Subsystem interfaces, sampling parameters, result struct details.
5.  **[GitHub repository](https://github.com/winyunq/LiteRT-LM-Unreal)**
    - The public legacy source baseline and current public documentation.

## 🚀 Why choose LiteRT-LM-Unreal?

- **One object model**: Quick Chat is a compatibility facade over an ordinary default Agent, not a second state implementation.
- **Independent character memory**: use one Agent per NPC, each with complete canonical history.
- **One shared GPU model**: one model and one serial queue, without a model copy per character.
- **C++ first**: Blueprint signatures stay unambiguous while C++ gets convenience overloads and a full native SDK entry.

---
*Strategy by Human, Tactics by AI. LiteRT-LM-Unreal empowers your game to enter the AIGC era.*
