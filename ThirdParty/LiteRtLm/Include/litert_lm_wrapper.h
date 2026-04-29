// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#ifndef LITERT_LM_WRAPPER_H
#define LITERT_LM_WRAPPER_H

#ifdef _WIN32
  #define DLL_EXPORT __declspec(dllexport)
#else
  #define DLL_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    // --- 数据结构 ---

    typedef struct {
        float temperature;  // 采样温度
        float top_p;        // Top-P 采样
        int top_k;          // Top-K 采样
        int max_tokens;     // 本次生成最大 Token 数
    } LiteRtLm_SamplingParams;

    typedef struct {
        const char* text_chunk; // 仅回调期间有效
        const char* error_msg;
        int bIsDone;
        float tokens_per_sec;   // 实时生成速度
    } LiteRtLm_Result;

    typedef void (*LiteRtLmCallback)(LiteRtLm_Result result, void* user_ptr);

    // --- 0. 硬件与探测接口 ---
    
    // 返回支持的后端列表（如 "cpu,gpu"），由 UE5 侧决定启动策略
    DLL_EXPORT const char* LiteRtLm_GetAvailableBackends();

    // 获取当前引擎的显存占用（MB）
    DLL_EXPORT float LiteRtLm_GetVRAMUsage(void* engine_ptr);

    // --- 1. 引擎生命周期 ---
    
    DLL_EXPORT void* LiteRtLm_CreateEngine(const char* model_path, const char* backend);
    DLL_EXPORT void LiteRtLm_DestroyEngine(void* engine_ptr);

    // --- 2. 会话状态机接口 (核心增量逻辑) ---
    
    DLL_EXPORT void* LiteRtLm_CreateConversation(void* engine_ptr);
    DLL_EXPORT void LiteRtLm_DestroyConversation(void* conv_ptr);

    // 向会话显式追加用户消息 (仅处理 Tokenization，不触发计算)
    DLL_EXPORT void LiteRtLm_AppendUserMessage(void* conv_ptr, const char* text);

    // 向会话显式追加 AI 消息 (同步外部历史或 MCP 结果)
    DLL_EXPORT void LiteRtLm_AppendAssistantMessage(void* conv_ptr, const char* text);

    // [核心] 触发增量推理
    // 该接口仅针对 Append 后的增量内容进行计算。
    DLL_EXPORT void LiteRtLm_RunInference(
        void* conv_ptr, 
        LiteRtLm_SamplingParams params,
        LiteRtLmCallback callback, 
        void* user_ptr
    );

    // 中断当前会话的推理
    DLL_EXPORT void LiteRtLm_StopMessage(void* conv_ptr);

#ifdef __cplusplus
}
#endif

#endif
