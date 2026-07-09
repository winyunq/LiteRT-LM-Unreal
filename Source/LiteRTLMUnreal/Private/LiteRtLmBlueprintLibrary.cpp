// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmBlueprintLibrary.h"

#include "LiteRtLmSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace LiteRtLmBlueprintLibrary
{
    static FString CleanModelFileName(const FString& ModelFileName)
    {
        return FPaths::GetCleanFilename(ModelFileName.IsEmpty() ? TEXT("gemma-4-E2B-it.litertlm") : ModelFileName);
    }

    static FString ResolveDownloadedModelPath(const FString& ModelFileName)
    {
        const FString CleanFileName = CleanModelFileName(ModelFileName);
        if (CleanFileName.IsEmpty())
        {
            return TEXT("");
        }

        return FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir() / TEXT("LiteRTModels") / CleanFileName);
    }

    static bool CopyFileStreaming(const FString& SourcePath, const FString& TargetPath, FString& OutErrorMessage)
    {
        TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*SourcePath));
        if (!Reader.IsValid())
        {
            OutErrorMessage = FString::Printf(TEXT("Could not open bundled model for read: %s"), *SourcePath);
            return false;
        }

        IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetPath), true);

        const FString PartialPath = TargetPath + TEXT(".part");
        IFileManager::Get().Delete(*PartialPath, false, true, true);

        TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*PartialPath));
        if (!Writer.IsValid())
        {
            OutErrorMessage = FString::Printf(TEXT("Could not create extracted model file: %s"), *PartialPath);
            return false;
        }

        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(1024 * 1024);

        int64 RemainingBytes = Reader->TotalSize();
        while (RemainingBytes > 0)
        {
            const int64 ChunkSize64 = FMath::Min<int64>(RemainingBytes, Buffer.Num());
            const int32 ChunkSize = static_cast<int32>(ChunkSize64);
            Reader->Serialize(Buffer.GetData(), ChunkSize);
            if (Reader->IsError())
            {
                Writer.Reset();
                IFileManager::Get().Delete(*PartialPath, false, true, true);
                OutErrorMessage = FString::Printf(TEXT("Read failed while extracting bundled model: %s"), *SourcePath);
                return false;
            }

            Writer->Serialize(Buffer.GetData(), ChunkSize);
            if (Writer->IsError())
            {
                Writer.Reset();
                IFileManager::Get().Delete(*PartialPath, false, true, true);
                OutErrorMessage = FString::Printf(TEXT("Write failed while extracting bundled model: %s"), *PartialPath);
                return false;
            }

            RemainingBytes -= ChunkSize64;
        }

        Writer.Reset();
        Reader.Reset();

        if (!IFileManager::Get().Move(*TargetPath, *PartialPath, true, true, false, true))
        {
            IFileManager::Get().Delete(*PartialPath, false, true, true);
            OutErrorMessage = FString::Printf(TEXT("Could not move extracted model to: %s"), *TargetPath);
            return false;
        }

        return true;
    }

    static bool PrepareProjectModelFile(const FString& ModelFileName, FString& OutModelPath, FString& OutErrorMessage)
    {
        OutModelPath.Reset();
        OutErrorMessage.Reset();

        FString Normalized = ModelFileName;
        FPaths::NormalizeFilename(Normalized);

        if (Normalized.IsEmpty())
        {
            OutErrorMessage = TEXT("Model file name is empty.");
            return false;
        }

        if (!FPaths::IsRelative(Normalized))
        {
            OutModelPath = FPaths::ConvertRelativePathToFull(Normalized);
            if (IFileManager::Get().FileExists(*OutModelPath))
            {
                return true;
            }

            OutErrorMessage = FString::Printf(TEXT("Model file does not exist: %s"), *OutModelPath);
            return false;
        }

        const FString ContentModelPath = Normalized.StartsWith(TEXT("Content/"))
            ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Normalized)
            : FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Models") / Normalized);

        const FString DownloadedModelPath = ResolveDownloadedModelPath(Normalized);
        const bool bContentExists = IFileManager::Get().FileExists(*ContentModelPath);
        const bool bDownloadedExists = !DownloadedModelPath.IsEmpty() && IFileManager::Get().FileExists(*DownloadedModelPath);

#if PLATFORM_ANDROID
        if (bContentExists)
        {
            const int64 SourceSize = IFileManager::Get().FileSize(*ContentModelPath);
            const int64 TargetSize = IFileManager::Get().FileSize(*DownloadedModelPath);
            if (bDownloadedExists && SourceSize > 0 && SourceSize == TargetSize)
            {
                OutModelPath = DownloadedModelPath;
                return true;
            }

            UE_LOG(LogLiteRtLm, Log, TEXT("Extracting bundled LiteRT-LM model to persistent storage: %s"), *DownloadedModelPath);
            if (CopyFileStreaming(ContentModelPath, DownloadedModelPath, OutErrorMessage))
            {
                OutModelPath = DownloadedModelPath;
                return true;
            }

            UE_LOG(LogLiteRtLm, Warning, TEXT("%s"), *OutErrorMessage);
        }
#else
        if (bContentExists)
        {
            OutModelPath = ContentModelPath;
            return true;
        }
#endif

        if (bDownloadedExists)
        {
            OutModelPath = DownloadedModelPath;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Model file was not found in Content/Models or persistent storage: %s"), *Normalized);
        return false;
    }

    static FLiteRtLmConfig BuildConfig(
        const FString& ModelPath,
        bool bUseAutoConfig,
        const FString& Backend,
        int32 MaxNumTokens,
        int32 NumThreads,
        bool bEnableBenchmark,
        bool bOptimizeShader,
        bool bEnableVision,
        bool bEnableAudio,
        bool bEnableStreaming)
    {
        FLiteRtLmConfig Config = bUseAutoConfig ? FLiteRtLmUnrealApi::GetAutoConfig() : FLiteRtLmConfig();

        Config.ModelPath = ModelPath;
        if (!bUseAutoConfig)
        {
            Config.Backend = Backend;
            Config.MaxNumTokens = MaxNumTokens;
            Config.NumThreads = NumThreads;
        }

        Config.bEnableBenchmark = bEnableBenchmark;
        Config.bOptimizeShader = bOptimizeShader;
        Config.bEnableVision = bEnableVision;
        Config.bEnableAudio = bEnableAudio;
        Config.bEnableStreaming = bEnableStreaming;
        return Config;
    }

    static UObject* ResolveSessionOwner(UObject* WorldContextObject, UObject* SessionOwner)
    {
        if (IsValid(SessionOwner))
        {
            return SessionOwner;
        }

        if (IsValid(WorldContextObject))
        {
            return WorldContextObject;
        }

        return GetTransientPackage();
    }

    static void ExecuteError(const FLiteRtLmBlueprintDoneDelegate& OnDone, const FString& ErrorMessage)
    {
        if (!OnDone.IsBound())
        {
            return;
        }

        FLiteRtLmResult Result;
        Result.ErrorMsg = ErrorMessage;
        Result.bIsDone = true;
        OnDone.Execute(Result);
    }

    static TSharedPtr<FJsonObject> MakeTextMessage(const FString& Role, const FString& Content)
    {
        TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
        Message->SetStringField(TEXT("role"), Role);
        Message->SetStringField(TEXT("content"), Content);
        return Message;
    }

    static TSharedPtr<FJsonObject> MakeUserMessage(
        const FString& UserMessage,
        const FString& ImagePath,
        const FString& AudioPath)
    {
        TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
        Message->SetStringField(TEXT("role"), TEXT("user"));

        const bool bHasImage = !ImagePath.IsEmpty();
        const bool bHasAudio = !AudioPath.IsEmpty();
        if (!bHasImage && !bHasAudio)
        {
            Message->SetStringField(TEXT("content"), UserMessage);
            return Message;
        }

        TArray<TSharedPtr<FJsonValue>> ContentArray;

        if (bHasImage)
        {
            FString StandardPath = ImagePath;
            FPaths::NormalizeFilename(StandardPath);

            TSharedPtr<FJsonObject> ImageObject = MakeShared<FJsonObject>();
            ImageObject->SetStringField(TEXT("type"), TEXT("image"));
            ImageObject->SetStringField(TEXT("path"), StandardPath);
            ContentArray.Add(MakeShared<FJsonValueObject>(ImageObject));
        }

        if (bHasAudio)
        {
            FString StandardPath = AudioPath;
            FPaths::NormalizeFilename(StandardPath);

            TSharedPtr<FJsonObject> AudioObject = MakeShared<FJsonObject>();
            AudioObject->SetStringField(TEXT("type"), TEXT("audio"));
            AudioObject->SetStringField(TEXT("path"), StandardPath);
            ContentArray.Add(MakeShared<FJsonValueObject>(AudioObject));
        }

        FString TextToSend = UserMessage;
        if (bHasImage && !TextToSend.Contains(TEXT("<IMAGE>")))
        {
            TextToSend = TEXT("<IMAGE>\n") + TextToSend;
        }
        if (bHasAudio && !TextToSend.Contains(TEXT("<AUDIO>")))
        {
            TextToSend = TEXT("<AUDIO>\n") + TextToSend;
        }

        TSharedPtr<FJsonObject> TextObject = MakeShared<FJsonObject>();
        TextObject->SetStringField(TEXT("type"), TEXT("text"));
        TextObject->SetStringField(TEXT("text"), TextToSend);
        ContentArray.Add(MakeShared<FJsonValueObject>(TextObject));

        Message->SetArrayField(TEXT("content"), ContentArray);
        return Message;
    }

    static void SendMessages(
        UObject* WorldContextObject,
        UObject* SessionOwner,
        const TArray<TSharedPtr<FJsonObject>>& Messages,
        const FString& ToolsJson,
        FLiteRtLmBlueprintChunkDelegate OnChunk,
        FLiteRtLmBlueprintDoneDelegate OnDone,
        const FLiteRtLmSamplingParams& SamplingParams)
    {
        UObject* ResolvedSessionOwner = ResolveSessionOwner(WorldContextObject, SessionOwner);
        if (!ResolvedSessionOwner)
        {
            ExecuteError(OnDone, TEXT("LiteRT-LM session owner is not available."));
            return;
        }

        if (Messages.Num() == 0)
        {
            ExecuteError(OnDone, TEXT("LiteRT-LM request has no messages."));
            return;
        }

        FLiteRtLmChunkCallback NativeChunk;
        if (OnChunk.IsBound())
        {
            NativeChunk = FLiteRtLmChunkCallback::CreateLambda(
                [OnChunk](const FString& TextChunk) mutable
                {
                    OnChunk.ExecuteIfBound(TextChunk);
                });
        }

        FLiteRtLmDoneCallback NativeDone;
        if (OnDone.IsBound())
        {
            NativeDone = FLiteRtLmDoneCallback::CreateLambda(
                [OnDone](const FLiteRtLmResult& Result) mutable
                {
                    OnDone.ExecuteIfBound(Result);
                });
        }

        TArray<TSharedPtr<FJsonObject>> NormalizedMessages = FLiteRtLmUnrealApi::NormalizeMessages(Messages);
        if (NormalizedMessages.Num() == 0)
        {
            ExecuteError(OnDone, TEXT("LiteRT-LM request has no valid messages."));
            return;
        }

        if (ULiteRtLmSubsystem* Subsystem = GEngine ? GEngine->GetEngineSubsystem<ULiteRtLmSubsystem>() : nullptr)
        {
            Subsystem->PrepareActiveAgent(ResolvedSessionOwner, ToolsJson);
        }

        TArray<TSharedPtr<FJsonObject>> HistoryMessages;
        for (int32 Index = 0; Index < NormalizedMessages.Num() - 1; ++Index)
        {
            HistoryMessages.Add(NormalizedMessages[Index]);
        }

        FLiteRtLmUnrealApi::RestoreHistory(HistoryMessages);
        FLiteRtLmUnrealApi::SendChatRequest(
            ResolvedSessionOwner,
            NormalizedMessages.Last(),
            NativeChunk,
            NativeDone,
            SamplingParams);
    }

    static bool ParseMessagesJson(
        const FString& MessagesJson,
        TArray<TSharedPtr<FJsonObject>>& OutMessages,
        FString& OutError)
    {
        const FString Trimmed = MessagesJson.TrimStartAndEnd();
        if (Trimmed.IsEmpty())
        {
            OutError = TEXT("MessagesJson is empty.");
            return false;
        }

        if (Trimmed.StartsWith(TEXT("[")))
        {
            TArray<TSharedPtr<FJsonValue>> Values;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
            if (!FJsonSerializer::Deserialize(Reader, Values))
            {
                OutError = TEXT("MessagesJson array could not be parsed.");
                return false;
            }

            for (const TSharedPtr<FJsonValue>& Value : Values)
            {
                TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
                if (!Object.IsValid())
                {
                    OutError = TEXT("MessagesJson array must contain only JSON objects.");
                    return false;
                }
                OutMessages.Add(Object);
            }
        }
        else
        {
            TSharedPtr<FJsonObject> Object;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
            if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
            {
                OutError = TEXT("MessagesJson object could not be parsed.");
                return false;
            }
            OutMessages.Add(Object);
        }

        if (OutMessages.Num() == 0)
        {
            OutError = TEXT("MessagesJson did not contain any messages.");
            return false;
        }

        return true;
    }
}

FLiteRtLmConfig ULiteRtLmBlueprintLibrary::GetLiteRtLmAutoConfig()
{
    return FLiteRtLmUnrealApi::GetAutoConfig();
}

FLiteRtLmConfig ULiteRtLmBlueprintLibrary::MakeLiteRtLmConfig(
    const FString& ModelPath,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    return LiteRtLmBlueprintLibrary::BuildConfig(
        ModelPath,
        false,
        Backend,
        MaxNumTokens,
        NumThreads,
        bEnableBenchmark,
        bOptimizeShader,
        bEnableVision,
        bEnableAudio,
        bEnableStreaming);
}

FLiteRtLmSamplingParams ULiteRtLmBlueprintLibrary::MakeLiteRtLmSamplingParams(
    float Temperature,
    float TopP,
    int32 TopK,
    int32 MaxTokens,
    ELiteRtLmConstraintType ConstraintType,
    const FString& ConstraintString)
{
    FLiteRtLmSamplingParams Params;
    Params.Temperature = Temperature;
    Params.TopP = TopP;
    Params.TopK = TopK;
    Params.MaxTokens = MaxTokens;
    Params.ConstraintType = ConstraintType;
    Params.ConstraintString = ConstraintString;
    return Params;
}

FString ULiteRtLmBlueprintLibrary::ResolveLiteRtLmProjectModelPath(const FString& ModelFileName)
{
    FString Normalized = ModelFileName;
    FPaths::NormalizeFilename(Normalized);

    if (Normalized.IsEmpty())
    {
        return TEXT("");
    }

    if (!FPaths::IsRelative(Normalized))
    {
        return FPaths::ConvertRelativePathToFull(Normalized);
    }

    if (Normalized.StartsWith(TEXT("Content/")))
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Normalized);
    }

    const FString ContentModelPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / TEXT("Models") / Normalized);
    if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ContentModelPath))
    {
        return ContentModelPath;
    }

    const FString DownloadedModelPath = ResolveLiteRtLmDownloadedModelPath(Normalized);
    if (!DownloadedModelPath.IsEmpty() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*DownloadedModelPath))
    {
        return DownloadedModelPath;
    }

    return ContentModelPath;
}

FString ULiteRtLmBlueprintLibrary::ResolveLiteRtLmDownloadedModelPath(const FString& ModelFileName)
{
    return LiteRtLmBlueprintLibrary::ResolveDownloadedModelPath(ModelFileName);
}

bool ULiteRtLmBlueprintLibrary::DoesLiteRtLmDownloadedModelExist(const FString& ModelFileName)
{
    const FString ModelPath = ResolveLiteRtLmDownloadedModelPath(ModelFileName);
    return !ModelPath.IsEmpty() && FPlatformFileManager::Get().GetPlatformFile().FileExists(*ModelPath);
}

bool ULiteRtLmBlueprintLibrary::PrepareLiteRtLmProjectModelFile(
    const FString& ModelFileName,
    FString& OutModelPath,
    FString& OutErrorMessage)
{
    return LiteRtLmBlueprintLibrary::PrepareProjectModelFile(ModelFileName, OutModelPath, OutErrorMessage);
}

int32 ULiteRtLmBlueprintLibrary::QueryLiteRtLmAvailableVramMB(int32 DefaultMB)
{
    return ULiteRtLmSubsystem::QueryAvailableVramMB(DefaultMB);
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmModel(const FLiteRtLmConfig& Config)
{
    return FLiteRtLmUnrealApi::LoadModel(Config);
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmModelFromPath(
    const FString& ModelPath,
    bool bUseAutoConfig,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    return FLiteRtLmUnrealApi::LoadModel(
        LiteRtLmBlueprintLibrary::BuildConfig(
            ModelPath,
            bUseAutoConfig,
            Backend,
            MaxNumTokens,
            NumThreads,
            bEnableBenchmark,
            bOptimizeShader,
            bEnableVision,
            bEnableAudio,
            bEnableStreaming));
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmProjectModel(
    const FString& ModelFileName,
    bool bUseAutoConfig,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    FString PreparedModelPath;
    FString PrepareError;
    if (!PrepareLiteRtLmProjectModelFile(ModelFileName, PreparedModelPath, PrepareError))
    {
        UE_LOG(LogLiteRtLm, Warning, TEXT("PrepareLiteRtLmProjectModelFile failed: %s"), *PrepareError);
        PreparedModelPath = ResolveLiteRtLmProjectModelPath(ModelFileName);
    }

    return LoadLiteRtLmModelFromPath(
        PreparedModelPath,
        bUseAutoConfig,
        Backend,
        MaxNumTokens,
        NumThreads,
        bEnableBenchmark,
        bOptimizeShader,
        bEnableVision,
        bEnableAudio,
        bEnableStreaming);
}

bool ULiteRtLmBlueprintLibrary::LoadLiteRtLmDownloadedModel(
    const FString& ModelFileName,
    bool bUseAutoConfig,
    const FString& Backend,
    int32 MaxNumTokens,
    int32 NumThreads,
    bool bEnableBenchmark,
    bool bOptimizeShader,
    bool bEnableVision,
    bool bEnableAudio,
    bool bEnableStreaming)
{
    return LoadLiteRtLmModelFromPath(
        ResolveLiteRtLmDownloadedModelPath(ModelFileName),
        bUseAutoConfig,
        Backend,
        MaxNumTokens,
        NumThreads,
        bEnableBenchmark,
        bOptimizeShader,
        bEnableVision,
        bEnableAudio,
        bEnableStreaming);
}

void ULiteRtLmBlueprintLibrary::UnloadLiteRtLmModel()
{
    FLiteRtLmUnrealApi::UnloadModel();
}

void ULiteRtLmBlueprintLibrary::StopLiteRtLmInference()
{
    FLiteRtLmUnrealApi::StopInference();
}

bool ULiteRtLmBlueprintLibrary::IsLiteRtLmModelLoaded()
{
    return FLiteRtLmUnrealApi::IsModelLoaded();
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmTextChat(
    UObject* WorldContextObject,
    const FString& UserMessage,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    Messages.Add(LiteRtLmBlueprintLibrary::MakeUserMessage(UserMessage, TEXT(""), TEXT("")));

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        TEXT(""),
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmTextChatWithSystemPrompt(
    UObject* WorldContextObject,
    const FString& UserMessage,
    const FString& SystemPrompt,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    if (!SystemPrompt.IsEmpty())
    {
        Messages.Add(LiteRtLmBlueprintLibrary::MakeTextMessage(TEXT("system"), SystemPrompt));
    }
    Messages.Add(LiteRtLmBlueprintLibrary::MakeUserMessage(UserMessage, TEXT(""), TEXT("")));

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        TEXT(""),
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmMultimodalChat(
    UObject* WorldContextObject,
    const FString& UserMessage,
    const FString& ImagePath,
    const FString& AudioPath,
    const FString& SystemPrompt,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    if (!SystemPrompt.IsEmpty())
    {
        Messages.Add(LiteRtLmBlueprintLibrary::MakeTextMessage(TEXT("system"), SystemPrompt));
    }
    Messages.Add(LiteRtLmBlueprintLibrary::MakeUserMessage(UserMessage, ImagePath, AudioPath));

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        TEXT(""),
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::SendLiteRtLmJsonChat(
    UObject* WorldContextObject,
    const FString& MessagesJson,
    const FString& ToolsJson,
    UObject* SessionOwner,
    FLiteRtLmBlueprintChunkDelegate OnChunk,
    FLiteRtLmBlueprintDoneDelegate OnDone,
    FLiteRtLmSamplingParams SamplingParams)
{
    TArray<TSharedPtr<FJsonObject>> Messages;
    FString ParseError;
    if (!LiteRtLmBlueprintLibrary::ParseMessagesJson(MessagesJson, Messages, ParseError))
    {
        LiteRtLmBlueprintLibrary::ExecuteError(OnDone, ParseError);
        return;
    }

    LiteRtLmBlueprintLibrary::SendMessages(
        WorldContextObject,
        SessionOwner,
        Messages,
        ToolsJson,
        OnChunk,
        OnDone,
        SamplingParams);
}

void ULiteRtLmBlueprintLibrary::ReleaseLiteRtLmSession(UObject* WorldContextObject, UObject* SessionOwner)
{
    if (UObject* ResolvedSessionOwner = LiteRtLmBlueprintLibrary::ResolveSessionOwner(WorldContextObject, SessionOwner))
    {
        FLiteRtLmUnrealApi::ReleaseSession(ResolvedSessionOwner);
    }
}
