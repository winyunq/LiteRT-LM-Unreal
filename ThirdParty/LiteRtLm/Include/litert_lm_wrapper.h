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
        const char* model_path;      // 模型路径
        const char* backend;         // 推理后端 ("cpu", "gpu", "gpu,gpu,cpu" 等)
        int max_num_tokens;          // 最大上下文长度
        int num_threads;             // CPU 线程数
        int bEnableBenchmark;        // 是否开启性能日志
        int bOptimizeShader;         // [UE5 专用] 是否优化着色器
    } LiteRtLm_Config;

    typedef struct {
        float temperature;  // 采样温度
        float top_p;        // Top-P 采样
        int top_k;          // Top-K 采样
        int max_tokens;     // 本次生成最大 Token 数

        /**
         * @brief 强制约束类型 (Constrained Decoding)
         * 0: 无约束, 1: Regex, 2: JSON Schema, 3: Lark Grammar
         */
        int constraint_type;

        /**
         * @brief 约束字符串内容 (如正则表达式、JSON Schema 或 Lark 语法)
         */
        const char* constraint_string;
    } LiteRtLm_SamplingParams;

    typedef struct {
        /**
         * @brief 文本片段指针。
         * @warning 此指针仅在回调函数执行期间有效！
         */
        const char* text_chunk; 

        /**
         * @brief 完整的 JSON 响应片段。
         * 包含模型返回的所有原始信息（如 tool_calls, channels, content 等）。
         */
        const char* full_json_chunk;

        const char* error_msg;
        int bIsDone;
        float tokens_per_sec;   // 实时生成速度
    } LiteRtLm_Result;

    typedef void (*LiteRtLmCallback)(LiteRtLm_Result result, void* user_ptr);

    // --- 0. 硬件与探测接口 ---
    
    // 返回支持的后端列表（如 "cpu,gpu"）
    DLL_EXPORT const char* LiteRtLm_GetAvailableBackends();

    // --- 1. 引擎生命周期 ---
    
    DLL_EXPORT void* LiteRtLm_CreateEngine(LiteRtLm_Config config);
    DLL_EXPORT void LiteRtLm_DestroyEngine(void* engine_ptr);

    // --- 2. 会话状态机接口 ---
    
    // 创建基础会话
    DLL_EXPORT void* LiteRtLm_CreateConversation(void* engine_ptr);

    // 创建带配置的会话 (支持 MCP/Tools 定义, 系统提示词等)
    // json_preface 格式参考 JsonPreface 结构，包含 messages, tools, extra_context
    DLL_EXPORT void* LiteRtLm_CreateConversationWithConfig(
        void* engine_ptr, 
        const char* json_preface,
        int bEnableConstrainedDecoding
    );

    DLL_EXPORT void LiteRtLm_DestroyConversation(void* conv_ptr);

    // 向会话追加用户消息 (纯文本)
    DLL_EXPORT void LiteRtLm_AppendUserMessage(void* conv_ptr, const char* text);

    // 向会话追加多模态或复杂消息 (JSON 格式)
    // 允许发送包含图片、工具结果或特定角色的消息
    // 示例: {"role": "user", "content": [{"type": "text", "text": "..."}, {"type": "image", "image": "base64..."}]}
    DLL_EXPORT void LiteRtLm_AppendMessageJson(void* conv_ptr, const char* json_msg);

    // 向会话追加 AI 消息 (用于同步历史)
    DLL_EXPORT void LiteRtLm_AppendAssistantMessage(void* conv_ptr, const char* text);

    // [核心] 触发增量推理
    DLL_EXPORT void LiteRtLm_RunInference(
        void* conv_ptr, 
        LiteRtLm_SamplingParams params,
        LiteRtLmCallback callback, 
        void* user_ptr
    );

    // 中断推理
    DLL_EXPORT void LiteRtLm_StopMessage(void* conv_ptr);

#ifdef __cplusplus
}
#endif

#endif
