// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "Modules/ModuleManager.h"

/**
 * Primary module implementation for LiteRTLMCore.
 */
class FLiteRTLMCoreModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // This code will execute after your module is loaded into memory.
    }

    virtual void ShutdownModule() override
    {
        // This function may be called during shutdown to clean up your module.
    }
};

IMPLEMENT_MODULE(FLiteRTLMCoreModule, LiteRTLMCore)
