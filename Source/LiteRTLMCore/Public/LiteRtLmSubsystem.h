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
class LITERTLMCORE_API ULiteRtLmSubsystem : public UEngineSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool LoadModel(const FLiteRtLmConfig& InConfig);
    void UnloadModel();

    // Session Management
    void* GetOrCreateSession(void* Ctx);
    void ReleaseSession(void* Ctx);

    // Getters
    void* GetEngineHandle() const { return EngineHandle; }
    bool IsModelLoaded() const { return EngineHandle != nullptr; }

private:
    void* EngineHandle = nullptr;
    TMap<void*, void*> SessionMap;

    UPROPERTY()
    FLiteRtLmConfig CurrentConfig;

    // LRU logic can be added here
};
