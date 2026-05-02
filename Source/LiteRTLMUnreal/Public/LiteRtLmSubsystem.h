// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "LiteRtLmUnrealApi.h"
#include "LiteRtLmSubsystem.generated.h"

/**
 * Global Subsystem to manage LiteRT-LM resources.
 */
UCLASS()
class LITERTLMUNREAL_API ULiteRtLmSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool LoadModel(const FLiteRtLmConfig& InConfig);
    void UnloadModel();

    // Session Management
    void* GetOrCreateSession(void* Ctx, const FString& ToolsJson = TEXT(""));
    void ReleaseSession(void* Ctx);

    // Per-session message tracking (for incremental message sync)
    int32 GetSessionMsgCount(void* Ctx) const;
    void SetSessionMsgCount(void* Ctx, int32 Count);

    // Getters
    void* GetEngineHandle() const { return EngineHandle; }
    bool IsModelLoaded() const { return EngineHandle != nullptr; }

    /**
     * Query available GPU VRAM via DXGI (Windows only).
     * Returns available VRAM in MB, or DefaultMB if query fails.
     */
    static int32 QueryAvailableVramMB(int32 DefaultMB = 4096);

private:
    void* EngineHandle = nullptr;
    TMap<void*, void*> SessionMap;
    TMap<void*, int32> SessionMsgCountMap;

    /** Tracks which ToolsJson was used to create each session (for invalidation). */
    TMap<void*, FString> SessionToolsMap;

    UPROPERTY()
    FLiteRtLmConfig CurrentConfig;

    // LRU logic can be added here
};
