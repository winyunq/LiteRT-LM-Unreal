// Copyright (c) 2025-2026 Winyunq. All rights reserved.
#include "LiteRtLmModelDownloader.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "LiteRtLmBlueprintLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace LiteRtLmModelDownload
{
    static const TCHAR* Gemma4E2BUrl = TEXT("https://huggingface.co/litert-community/gemma-4-E2B-it-litert-lm/resolve/main/gemma-4-E2B-it.litertlm?download=true");

    static FORCEINLINE uint32 Ror(uint32 Value, uint32 Bits)
    {
        return (Value >> Bits) | (Value << (32 - Bits));
    }

    struct FSha256
    {
        uint8 Data[64] = {};
        uint32 DataLength = 0;
        uint64 BitLength = 0;
        uint32 State[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };

        void Transform(const uint8* Chunk)
        {
            static const uint32 K[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
            };

            uint32 M[64];
            for (int32 i = 0; i < 16; ++i)
            {
                const int32 j = i * 4;
                M[i] = (uint32(Chunk[j]) << 24) | (uint32(Chunk[j + 1]) << 16) | (uint32(Chunk[j + 2]) << 8) | uint32(Chunk[j + 3]);
            }
            for (int32 i = 16; i < 64; ++i)
            {
                const uint32 S0 = Ror(M[i - 15], 7) ^ Ror(M[i - 15], 18) ^ (M[i - 15] >> 3);
                const uint32 S1 = Ror(M[i - 2], 17) ^ Ror(M[i - 2], 19) ^ (M[i - 2] >> 10);
                M[i] = M[i - 16] + S0 + M[i - 7] + S1;
            }

            uint32 A = State[0];
            uint32 B = State[1];
            uint32 C = State[2];
            uint32 D = State[3];
            uint32 E = State[4];
            uint32 F = State[5];
            uint32 G = State[6];
            uint32 H = State[7];

            for (int32 i = 0; i < 64; ++i)
            {
                const uint32 S1 = Ror(E, 6) ^ Ror(E, 11) ^ Ror(E, 25);
                const uint32 Ch = (E & F) ^ ((~E) & G);
                const uint32 Temp1 = H + S1 + Ch + K[i] + M[i];
                const uint32 S0 = Ror(A, 2) ^ Ror(A, 13) ^ Ror(A, 22);
                const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
                const uint32 Temp2 = S0 + Maj;

                H = G;
                G = F;
                F = E;
                E = D + Temp1;
                D = C;
                C = B;
                B = A;
                A = Temp1 + Temp2;
            }

            State[0] += A;
            State[1] += B;
            State[2] += C;
            State[3] += D;
            State[4] += E;
            State[5] += F;
            State[6] += G;
            State[7] += H;
        }

        void Update(const uint8* Bytes, int64 Length)
        {
            for (int64 i = 0; i < Length; ++i)
            {
                Data[DataLength++] = Bytes[i];
                if (DataLength == 64)
                {
                    Transform(Data);
                    BitLength += 512;
                    DataLength = 0;
                }
            }
        }

        FString Final()
        {
            uint32 i = DataLength;
            Data[i++] = 0x80;
            if (i > 56)
            {
                while (i < 64)
                {
                    Data[i++] = 0;
                }
                Transform(Data);
                FMemory::Memzero(Data, 56);
            }
            else
            {
                while (i < 56)
                {
                    Data[i++] = 0;
                }
            }

            BitLength += uint64(DataLength) * 8;
            for (int32 j = 7; j >= 0; --j)
            {
                Data[63 - j] = uint8(BitLength >> (j * 8));
            }
            Transform(Data);

            FString Hex;
            Hex.Reserve(64);
            for (uint32 Word : State)
            {
                Hex += FString::Printf(TEXT("%08x"), Word);
            }
            return Hex.ToLower();
        }
    };
}

ULiteRtLmDownloadModelAsyncAction* ULiteRtLmDownloadModelAsyncAction::DownloadGemma4E2BModel(
    UObject* WorldContextObject,
    const FString& ExpectedSha256,
    bool bOverwriteExisting,
    float TimeoutSeconds)
{
    return DownloadLiteRtLmModel(
        WorldContextObject,
        LiteRtLmModelDownload::Gemma4E2BUrl,
        TEXT("gemma-4-E2B-it.litertlm"),
        ExpectedSha256,
        bOverwriteExisting,
        TimeoutSeconds);
}

ULiteRtLmDownloadModelAsyncAction* ULiteRtLmDownloadModelAsyncAction::DownloadLiteRtLmModel(
    UObject* WorldContextObject,
    const FString& Url,
    const FString& ModelFileName,
    const FString& ExpectedSha256,
    bool bOverwriteExisting,
    float TimeoutSeconds)
{
    ULiteRtLmDownloadModelAsyncAction* Action = NewObject<ULiteRtLmDownloadModelAsyncAction>();
    Action->WorldContext = WorldContextObject;
    Action->DownloadUrl = Url;
    Action->TargetModelFileName = ModelFileName;
    Action->ExpectedSha256Hash = ExpectedSha256;
    Action->bOverwrite = bOverwriteExisting;
    Action->RequestTimeoutSeconds = TimeoutSeconds;
    Action->RegisterWithGameInstance(WorldContextObject);
    return Action;
}

void ULiteRtLmDownloadModelAsyncAction::Activate()
{
    const FString TrimmedUrl = DownloadUrl.TrimStartAndEnd();
    if (!TrimmedUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) &&
        !TrimmedUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        Finish(false, TEXT("Download URL must start with http:// or https://."));
        return;
    }

    TargetPath = ResolveTargetPath();
    if (TargetPath.IsEmpty())
    {
        Finish(false, TEXT("Target model file name is empty."));
        return;
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    const FString TargetDir = FPaths::GetPath(TargetPath);
    if (!PlatformFile.DirectoryExists(*TargetDir) && !PlatformFile.CreateDirectoryTree(*TargetDir))
    {
        Finish(false, FString::Printf(TEXT("Could not create model directory: %s"), *TargetDir));
        return;
    }

    const FString ExpectedHash = NormalizeExpectedSha256();
    if (PlatformFile.FileExists(*TargetPath) && !bOverwrite)
    {
        bool bHashVerified = false;
        if (!ExpectedHash.IsEmpty())
        {
            FString ActualHash;
            FString HashError;
            if (!VerifyFileSha256(TargetPath, ExpectedHash, ActualHash, HashError))
            {
                bOverwrite = true;
            }
            else
            {
                bHashVerified = true;
            }
        }

        if (!bOverwrite)
        {
            BytesReceived = IFileManager::Get().FileSize(*TargetPath);
            ContentLength = BytesReceived;
            Finish(true, TEXT(""), true, bHashVerified);
            return;
        }
    }

    PartialPath = TargetPath + TEXT(".part");
    IFileManager::Get().Delete(*PartialPath, false, true, true);
    ReceiveStream = MakeShareable(IFileManager::Get().CreateFileWriter(*PartialPath));
    if (!ReceiveStream.IsValid())
    {
        Finish(false, FString::Printf(TEXT("Could not open temporary model file for writing: %s"), *PartialPath));
        return;
    }

    ActiveRequest = FHttpModule::Get().CreateRequest();
    ActiveRequest->SetURL(TrimmedUrl);
    ActiveRequest->SetVerb(TEXT("GET"));
    ActiveRequest->SetHeader(TEXT("User-Agent"), TEXT("LiteRT-LM-Unreal/1.0"));
    if (RequestTimeoutSeconds > 0.0f)
    {
        ActiveRequest->SetTimeout(RequestTimeoutSeconds);
    }
    ActiveRequest->SetActivityTimeout(120.0f);
    ActiveRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);

    if (!ActiveRequest->SetResponseBodyReceiveStream(ReceiveStream.ToSharedRef()))
    {
        ReceiveStream.Reset();
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        Finish(false, TEXT("HTTP backend does not support response streaming to file."));
        return;
    }

    ActiveRequest->OnHeaderReceived().BindUObject(this, &ULiteRtLmDownloadModelAsyncAction::HandleHeader);
    ActiveRequest->OnRequestProgress64().BindUObject(this, &ULiteRtLmDownloadModelAsyncAction::HandleProgress);
    ActiveRequest->OnProcessRequestComplete().BindUObject(this, &ULiteRtLmDownloadModelAsyncAction::HandleComplete);

    if (!ActiveRequest->ProcessRequest())
    {
        ActiveRequest.Reset();
        ReceiveStream.Reset();
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        Finish(false, TEXT("Failed to start HTTP download request."));
    }
}

void ULiteRtLmDownloadModelAsyncAction::HandleHeader(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request, const FString& HeaderName, const FString& HeaderValue)
{
    if (HeaderName.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
    {
        ContentLength = FCString::Strtoi64(*HeaderValue, nullptr, 10);
    }
}

void ULiteRtLmDownloadModelAsyncAction::HandleProgress(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request, uint64 BytesSent, uint64 InBytesReceived)
{
    BytesReceived = static_cast<int64>(InBytesReceived);
    const float Progress = ContentLength > 0 ? static_cast<float>(FMath::Clamp(static_cast<double>(BytesReceived) / static_cast<double>(ContentLength), 0.0, 1.0)) : 0.0f;
    OnProgress.Broadcast(BytesReceived, ContentLength, Progress);
}

void ULiteRtLmDownloadModelAsyncAction::HandleComplete(TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request, TSharedPtr<IHttpResponse, ESPMode::ThreadSafe> Response, bool bSucceeded)
{
    ActiveRequest.Reset();
    ReceiveStream.Reset();

    HttpStatus = Response.IsValid() ? Response->GetResponseCode() : 0;
    const bool bHttpOk = HttpStatus >= 200 && HttpStatus < 300;
    if (!bSucceeded || !Response.IsValid() || !bHttpOk)
    {
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        Finish(false, FString::Printf(TEXT("HTTP download failed. Status=%d"), HttpStatus));
        return;
    }

    const int64 PartialSize = IFileManager::Get().FileSize(*PartialPath);
    BytesReceived = PartialSize;
    if (ContentLength > 0 && PartialSize != ContentLength)
    {
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        Finish(false, FString::Printf(TEXT("Downloaded size mismatch. Expected=%lld Actual=%lld"), ContentLength, PartialSize));
        return;
    }

    const FString ExpectedHash = NormalizeExpectedSha256();
    bool bHashVerified = false;
    if (!ExpectedHash.IsEmpty())
    {
        FString ActualHash;
        FString HashError;
        if (!VerifyFileSha256(PartialPath, ExpectedHash, ActualHash, HashError))
        {
            IFileManager::Get().Delete(*PartialPath, false, true, true);
            Finish(false, HashError.IsEmpty() ? FString::Printf(TEXT("SHA-256 mismatch. Actual=%s"), *ActualHash) : HashError);
            return;
        }
        bHashVerified = true;
    }

    IFileManager::Get().Delete(*TargetPath, false, true, true);
    if (!IFileManager::Get().Move(*TargetPath, *PartialPath, true, true, true, true))
    {
        IFileManager::Get().Delete(*PartialPath, false, true, true);
        Finish(false, FString::Printf(TEXT("Could not move downloaded model to: %s"), *TargetPath));
        return;
    }

    OnProgress.Broadcast(BytesReceived, ContentLength > 0 ? ContentLength : BytesReceived, 1.0f);
    Finish(true, TEXT(""), false, bHashVerified);
}

void ULiteRtLmDownloadModelAsyncAction::Finish(bool bSuccess, const FString& ErrorMessage, bool bAlreadyExists, bool bHashVerified)
{
    FLiteRtLmModelDownloadResult Result;
    Result.bSuccess = bSuccess;
    Result.bAlreadyExists = bAlreadyExists;
    Result.bHashVerified = bHashVerified;
    Result.LocalPath = TargetPath;
    Result.ErrorMessage = ErrorMessage;
    Result.BytesReceived = BytesReceived;
    Result.ContentLength = ContentLength;
    Result.HttpStatus = HttpStatus;
    OnCompleted.Broadcast(Result);
    SetReadyToDestroy();
}

FString ULiteRtLmDownloadModelAsyncAction::ResolveTargetPath() const
{
    return ULiteRtLmBlueprintLibrary::ResolveLiteRtLmDownloadedModelPath(TargetModelFileName);
}

FString ULiteRtLmDownloadModelAsyncAction::NormalizeExpectedSha256() const
{
    FString Hash = ExpectedSha256Hash.TrimStartAndEnd().ToLower();
    Hash.ReplaceInline(TEXT(" "), TEXT(""));
    Hash.ReplaceInline(TEXT(":"), TEXT(""));
    return Hash.Len() == 64 ? Hash : TEXT("");
}

bool ULiteRtLmDownloadModelAsyncAction::VerifyFileSha256(const FString& FilePath, const FString& ExpectedHash, FString& OutActualHash, FString& OutError) const
{
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath));
    if (!Reader.IsValid())
    {
        OutError = FString::Printf(TEXT("Could not open model for SHA-256 verification: %s"), *FilePath);
        return false;
    }

    LiteRtLmModelDownload::FSha256 Sha;
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(1024 * 1024);
    while (!Reader->AtEnd())
    {
        const int64 Remaining = Reader->TotalSize() - Reader->Tell();
        const int64 ToRead = FMath::Min<int64>(Buffer.Num(), Remaining);
        Reader->Serialize(Buffer.GetData(), ToRead);
        if (Reader->GetError())
        {
            OutError = FString::Printf(TEXT("Error while reading model for SHA-256 verification: %s"), *FilePath);
            return false;
        }
        Sha.Update(Buffer.GetData(), ToRead);
    }

    OutActualHash = Sha.Final();
    if (!OutActualHash.Equals(ExpectedHash, ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(TEXT("SHA-256 mismatch. Expected=%s Actual=%s"), *ExpectedHash, *OutActualHash);
        return false;
    }
    return true;
}
