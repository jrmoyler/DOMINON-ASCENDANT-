#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

#include "DADamageTypes.generated.h"

UENUM(BlueprintType)
enum class EDADamageChannel : uint8
{
    Kinetic,
    Thermal,
    Arc,
    Bio,
    Cyber,
    Structural
};

USTRUCT(BlueprintType)
struct DOMINIONGAMEPLAY_API FDADamageResolution
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float FinalDamage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float GuardDamage = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Damage")
    float HealthDamage = 0.f;
};

// Native SetByCaller keys keep channel selection explicit and validated by the C++ execution.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_Damage);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_DamageChannel_Kinetic);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_DamageChannel_Thermal);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_DamageChannel_Arc);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_DamageChannel_Bio);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_DamageChannel_Cyber);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_DA_Data_DamageChannel_Structural);

namespace DADamageTags
{
    DOMINIONGAMEPLAY_API const FGameplayTag& Damage();
    DOMINIONGAMEPLAY_API const FGameplayTag& Kinetic();
    DOMINIONGAMEPLAY_API const FGameplayTag& Thermal();
    DOMINIONGAMEPLAY_API const FGameplayTag& Arc();
    DOMINIONGAMEPLAY_API const FGameplayTag& Bio();
    DOMINIONGAMEPLAY_API const FGameplayTag& Cyber();
    DOMINIONGAMEPLAY_API const FGameplayTag& Structural();
}
