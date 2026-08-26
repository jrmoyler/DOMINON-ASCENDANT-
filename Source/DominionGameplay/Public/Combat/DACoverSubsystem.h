#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "DACoverSubsystem.generated.h"

UENUM(BlueprintType)
enum class EDACoverType : uint8
{
    Partial,
    Full,
    Hardened,
    Destructible
};

UENUM(BlueprintType)
enum class EDACoverSource : uint8
{
    Authored,
    Ruin,
    Deployable
};

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDACoverSocket
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName Id = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EDACoverType CoverType = EDACoverType::Partial;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EDACoverSource Source = EDACoverSource::Authored;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;

    bool Matches(const FDACoverSocket& Other) const;
};

/** World-owned registry of deliberately authored, ruin, and deployable cover. */
UCLASS()
class DOMINIONGAMEPLAY_API UDACoverSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    bool RegisterAuthoredCoverSocket(FName Id, EDACoverType CoverType, const FVector& Location);
    bool RegisterRuinCover(FName Id, EDACoverType CoverType, const FVector& Location);
    bool RegisterDeployableCover(FName Id, EDACoverType CoverType, const FVector& Location);
    bool UnregisterAuthoredCoverSocket(FName Id);
    bool UnregisterRuinCover(FName Id);
    bool UnregisterDeployableCover(FName Id);

    int32 GetRegisteredCoverCount() const { return CoverSockets.Num(); }
    const FDACoverSocket* FindCoverSocket(FName Id) const;

private:
    bool RegisterCover(FName Id, EDACoverType CoverType, EDACoverSource Source, const FVector& Location);
    bool UnregisterCover(FName Id, EDACoverSource Source);

    UPROPERTY(Transient)
    TArray<FDACoverSocket> CoverSockets;
};
