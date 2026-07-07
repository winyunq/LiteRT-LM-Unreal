// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "LiteRtLmModelDownloader.generated.h"

class FArchive;
class IHttpRequest;
class IHttpResponse;

USTRUCT(BlueprintType)
struct LITERTLMUNREAL_API FLiteRtLmModelDownloadResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    bool bAlreadyExists = false;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    bool bHashVerified = false;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    FString LocalPath;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    FString ErrorMessage;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    int64 BytesReceived = 0;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    int64 ContentLength = -1;

    UPROPERTY(BlueprintReadOnly, Category = "LiteRT-LM|Model Download")
    int32 HttpStatus = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLiteRtLmModelDownloadProgress, int64, BytesReceived, int64, ContentLength, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLiteRtLmModelDownloadCompleted, const FLiteRtLmModelDownloadResult&, Result);

UCLASS()
class LITERTLMUNREAL_API ULiteRtLmDownloadModelAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FLiteRtLmModelDownloadProgress OnProgress;

    UPROPERTY(BlueprintAssignable)
    FLiteRtLmModelDownloadCompleted OnCompleted;

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Model Download", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Download LiteRT-LM Model", AdvancedDisplay = "ExpectedSha256,bOverwriteExisting,TimeoutSeconds", Keywords = "LiteRT LM download model android e2b gemma"))
    static ULiteRtLmDownloadModelAsyncAction* DownloadLiteRtLmModel(
        UObject* WorldContextObject,
        const FString& Url,
        const FString& ModelFileName = TEXT("gemma-4-E2B-it.litertlm"),
        const FString& ExpectedSha256 = TEXT(""),
        bool bOverwriteExisting = false,
        float TimeoutSeconds = 7200.0f);

    UFUNCTION(BlueprintCallable, Category = "LiteRT-LM|Model Download", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Download Gemma 4 E2B LiteRT-LM Model", AdvancedDisplay = "ExpectedSha256,bOverwriteExisting,TimeoutSeconds", Keywords = "LiteRT LM download gemma 4 e2b android"))
    static ULiteRtLmDownloadModelAsyncAction* DownloadGemma4E2BModel(
        UObject* WorldContextObject,
        const FString& ExpectedSha256 = TEXT(""),
        bool bOverwriteExisting = false,
        float TimeoutSeconds = 7200.0f);

    virtual void Activate() override;

private:
    void HandleHeader(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request, const FString& HeaderName, const FString& HeaderValue);
    void HandleProgress(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request, uint64 BytesSent, uint64 InBytesReceived);
    void HandleComplete(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request, TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, bool bSucceeded);
    void Finish(bool bSuccess, const FString& ErrorMessage, bool bAlreadyExists = false, bool bHashVerified = false);

    FString ResolveTargetPath() const;
    FString NormalizeExpectedSha256() const;
    bool VerifyFileSha256(const FString& FilePath, const FString& ExpectedHash, FString& OutActualHash, FString& OutError) const;

    UPROPERTY()
    TObjectPtr<UObject> WorldContext;

    FString DownloadUrl;
    FString TargetModelFileName;
    FString ExpectedSha256Hash;
    FString TargetPath;
    FString PartialPath;
    int64 ContentLength = -1;
    int64 BytesReceived = 0;
    int32 HttpStatus = 0;
    bool bOverwrite = false;
    float RequestTimeoutSeconds = 7200.0f;

    TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;
    TSharedPtr<FArchive> ReceiveStream;
};
