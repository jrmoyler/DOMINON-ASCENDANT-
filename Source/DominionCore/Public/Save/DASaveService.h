#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Save/DACampaignSaveGame.h"
#include "Save/DASaveSchema.h"

template <typename TValue, typename TError>
class TResult
{
public:
    static TResult Success(TValue InValue)
    {
        TResult Result;
        Result.Value.Emplace(MoveTemp(InValue));
        return Result;
    }

    static TResult Failure(TError InError)
    {
        TResult Result;
        Result.Error.Emplace(MoveTemp(InError));
        return Result;
    }

    bool HasValue() const
    {
        return Value.IsSet();
    }

    const TValue& GetValue() const
    {
        check(Value.IsSet());
        return Value.GetValue();
    }

    const TError& GetError() const
    {
        check(Error.IsSet());
        return Error.GetValue();
    }

private:
    TOptional<TValue> Value;
    TOptional<TError> Error;
};

// Platform transaction boundary. Production uses Windows same-volume replacement primitives;
// tests may inject a controlled failure and assert the resulting files and loaded campaign.
class DOMINIONCORE_API IDASaveTransactionPlatform
{
public:
    virtual ~IDASaveTransactionPlatform() = default;

    virtual bool Commit(
        const FString& ActivePath,
        const FString& TemporaryPath,
        const FString& BackupPath,
        bool bActiveExists,
        FString& OutError) = 0;

    virtual bool RecoverActive(
        const FString& ActivePath,
        const FString& BackupPath,
        const FString& RecoveryPath,
        FString& OutError) = 0;
};

class DOMINIONCORE_API FDASaveService
{
public:
    static constexpr int32 CurrentSchemaVersion = FDASaveSchema::CurrentSchemaVersion;
    static constexpr int32 CurrentContentVersion = FDASaveSchema::CurrentContentVersion;
    static constexpr int32 CurrentBuildVersion = FDASaveSchema::CurrentBuildVersion;

    FDASaveService();
    explicit FDASaveService(FString InSaveDirectory);
    FDASaveService(FString InSaveDirectory, TSharedRef<IDASaveTransactionPlatform> InTransactionPlatform);

    FDASaveResult SaveCampaign(const FDACampaignSnapshot& Snapshot, FString Slot) const;
    TResult<FDACampaignSnapshot, FDASaveError> LoadCampaign(FString Slot) const;

private:
    FString SaveDirectory;
    TSharedRef<IDASaveTransactionPlatform> TransactionPlatform;
};
