#pragma once

#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "Manifest/DAUIManifest.h"

#include "DAUIGeneratedMetadata.generated.h"

class UInputAction;
class UInputMappingContext;
enum class EInputActionValueType : uint8;

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIInputBindingDescriptor
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName BindingId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FKey Key;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PayloadIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bController = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNegate = false;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIGeneratedAssetRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftClassPath SourceClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SemanticHash;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIGeneratedActionRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ActionId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) uint8 ValueType = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FKey Key;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 PayloadIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bController = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNegate = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SemanticHash;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIGeneratedInputRecord
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName ContextId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FString> MappingSignatures;
};

UCLASS(BlueprintType)
class DOMINIONUI_API UDAUIGeneratedManifestMetadata final : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ManifestContentHash;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIGeneratedAssetRecord> Screens;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIGeneratedAssetRecord> Overlays;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIGeneratedInputRecord> InputContexts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIGeneratedActionRecord> InputActions;
};

struct DOMINIONUI_API FDAUIGeneratedCache
{
    static const TCHAR* MetadataObjectPath();
    static FString InputActionObjectPath(FName ActionId);
    static TArray<FDAUIInputBindingDescriptor> GetBindingDescriptors(
        const FDAUICampaignActionDescriptor& Action, FString& OutError);
    static FString ScreenSemanticHash(const FDAUIScreenDescriptor& Screen);
    static FString OverlaySemanticHash(const FDAUIOverlayDescriptor& Overlay);
    static FString ActionSemanticHash(const FDAUICampaignActionDescriptor& Action);
    static FString BindingSemanticHash(const FDAUICampaignActionDescriptor& Action,
        const FDAUIInputBindingDescriptor& Binding);
    static bool ValidateMetadata(const FDAUIManifest& Manifest,
        const UDAUIGeneratedManifestMetadata& Metadata, FString& OutError);
    static bool ValidateInstalledCache(const FDAUIManifest& Manifest, FString& OutError);
    static bool CanUseGeneratedScreen(const FDAUIManifest& Manifest, const FDAUIScreenDescriptor& Screen);
    static bool GetExpectedInputKeys(const FDAUICampaignActionDescriptor& Action,
        TArray<FKey>& OutKeyboard, TArray<FKey>& OutController, FString& OutError);
    static EInputActionValueType GetExpectedValueType(const FDAUICampaignActionDescriptor& Action);
    static bool MapFrozenAction(UInputMappingContext& Context, UInputAction& InputAction,
        const FDAUICampaignActionDescriptor& Action, FString& OutError);
    static bool MapFrozenBinding(UInputMappingContext& Context, UInputAction& InputAction,
        const FDAUICampaignActionDescriptor& Action, const FDAUIInputBindingDescriptor& Binding,
        FString& OutError);
    /** Validates the complete per-screen graph; only one such context is active at runtime. */
    static bool ValidateActiveScreenInputGraph(const FDAUIManifest& Manifest, FString& OutError);
};
