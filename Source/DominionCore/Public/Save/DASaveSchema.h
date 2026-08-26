#pragma once

#include "CoreMinimal.h"

struct DOMINIONCORE_API FDASaveSchema
{
    static constexpr int32 InitialSchemaVersion = 1;
    static constexpr int32 CurrentSchemaVersion = 19;
    static constexpr int32 CurrentContentVersion = 1;
    static constexpr int32 CurrentBuildVersion = 1;
};

enum class EDASaveErrorCode : uint8
{
    None,
    InvalidSlot,
    DirectoryCreationFailed,
    SerializationFailed,
    TemporaryWriteFailed,
    ChecksumMismatch,
    BackupRotationFailed,
    AtomicReplaceFailed,
    SlotNotFound,
    ReadFailed,
    InvalidDocument,
    UnsupportedSchema,
    MigrationFailed
};

struct DOMINIONCORE_API FDASaveError
{
    EDASaveErrorCode Code = EDASaveErrorCode::None;
    FString Message;
};

struct DOMINIONCORE_API FDASaveResult
{
    static FDASaveResult Success()
    {
        return {};
    }

    static FDASaveResult Failure(const EDASaveErrorCode Code, FString Message)
    {
        FDASaveResult Result;
        Result.Error.Code = Code;
        Result.Error.Message = MoveTemp(Message);
        return Result;
    }

    bool IsSuccess() const
    {
        return Error.Code == EDASaveErrorCode::None;
    }

    FDASaveError Error;
};
