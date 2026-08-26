#pragma once

#include "Ascension/DAAscensionSystem.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "World/DAWorldAssetRecord.h"

#include "DAPresentationContent.generated.h"

struct FDACaptureRecord;
struct FDACapturePresentationSnapshot;

UENUM(BlueprintType)
enum class EDAPresentationDefinitionKind : uint8
{
    PrimaryAsset,
    CoreVFX,
    Music,
    Ambient,
    SFX
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAPresentationArtifactBinding
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Role;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString AssetClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath Asset;

    bool operator==(const FDAPresentationArtifactBinding& Other) const
    {
        return Role == Other.Role && AssetClass == Other.AssetClass && Asset == Other.Asset;
    }
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAPresentationDefinitionSource
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAPresentationDefinitionKind Kind = EDAPresentationDefinitionKind::PrimaryAsset;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Faction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString AssetPath;
    /** Canonical recipe JSON retained in generated and fallback definitions for runtime consumers. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString RecipePayload;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString RecipeFingerprint;
    /** Loadable production artifacts generated/imported beside this canonical definition. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAPresentationArtifactBinding> Artifacts;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bGeneratedCache = false;
};

UCLASS(BlueprintType)
class DOMINIONWORLD_API UDAPresentationDefinition final : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDAPresentationDefinitionSource Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString SourceFingerprint;
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly) bool bRuntimeManifestFallback = false;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAConstructionPresentationStage
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAConstructionState State = EDAConstructionState::Foundation;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName GeometryHook;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName VfxId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SfxId;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAConstructionPresentationGrammar
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Faction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName GrammarId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAConstructionPresentationStage> Stages;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDADamagePresentationLanguage
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Faction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MaterialHook;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName VfxId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SfxId;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAPresentationStateBinding
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName State;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName GeometryHook;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName VfxId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SfxId;
};

struct DOMINIONWORLD_API FDAPresentationContentManifest
{
    int32 SchemaVersion = 0;
    FName ContentId;
    FString Fingerprint;
    TArray<FDAPresentationDefinitionSource> PrimaryAssets;
    TArray<FDAPresentationDefinitionSource> CoreVfx;
    TArray<FDAPresentationDefinitionSource> Music;
    TArray<FDAPresentationDefinitionSource> Ambient;
    TArray<FDAPresentationDefinitionSource> Sfx;
    FName ConstructionSourceHook;
    FName DamageSourceHook;
    TArray<FDAConstructionPresentationGrammar> ConstructionGrammars;
    TArray<FDADamagePresentationLanguage> DamageLanguages;
    FName CaptureSourceHook;
    bool bAllowsInstantFactionRecolor = true;
    TArray<FName> CaptureStages;
    FName DaxtonSourceAuthority;
    TArray<FDAPresentationStateBinding> DaxtonStates;
    FName AscensionSourceAuthority;
    FSoftObjectPath AscensionCinematicAsset;
    bool bAscensionGameplayGate = true;
    bool bAscensionCinematicMayBeSkipped = false;
    TArray<FDAPresentationStateBinding> AscensionBeats;

    struct FArtifactRequirement
    {
        FName DefinitionId;
        EDAPresentationDefinitionKind DefinitionKind = EDAPresentationDefinitionKind::PrimaryAsset;
        FName Role;
        FString AssetClass;
        FString AssetPath;
        FString SourcePath;
        FString SourceSha1;
        /** Canonical source fragment consumed to generate this definition-specific artifact. */
        FString SourceContentPayload;
        FString SourceContentFingerprint;
        FName GeneratorOwner;
        bool bExternalGenerator = false;
    };
    TArray<FArtifactRequirement> ArtifactRequirements;

    TArray<FDAPresentationDefinitionSource> AllDefinitions() const;
    const FDAConstructionPresentationGrammar* FindConstructionGrammar(FName Faction) const;
    const FDADamagePresentationLanguage* FindDamageLanguage(FName Faction) const;
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDACaptureOwnershipPresentationStage
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Stage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName OriginalOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TargetOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DisplayedOwnerCivilizationId;
    /** Populated only when this stage installs owner-specific signage. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SignageOwnerCivilizationId;
};

class DOMINIONWORLD_API FDAPresentationContentPipeline
{
public:
    static FString GetCanonicalManifestPath();
    static bool LoadCanonical(FDAPresentationContentManifest& OutManifest, TArray<FText>& OutErrors);
    static bool LoadRootFile(const FString& Filename, FDAPresentationContentManifest& OutManifest,
        TArray<FText>& OutErrors);
    static bool Validate(const FDAPresentationContentManifest& Manifest, TArray<FText>& OutErrors);
    static bool Resolve(const FDAPresentationContentManifest& Manifest,
        EDAPresentationDefinitionKind Kind, FName Id, FDAPresentationDefinitionSource& OutDefinition);
    static bool BuildRuntimeFallback(const FDAPresentationContentManifest& Manifest,
        TArray<UDAPresentationDefinition*>& OutDefinitions, TArray<FText>& OutErrors);
    static bool ValidateGeneratedCache(const FDAPresentationContentManifest& Manifest,
        const TArray<UDAPresentationDefinition*>& Definitions, bool& bOutUsedGeneratedCache,
        TArray<FText>& OutErrors);
};

USTRUCT(BlueprintType)
struct DOMINIONWORLD_API FDAPresentationPlaybackPlan
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName GeometryHook;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MaterialHook;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName VfxId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName SfxId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName DisplayOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName OriginalOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName TargetOwnerCivilizationId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> OrderedStages;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDACaptureOwnershipPresentationStage> OwnershipStages;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bAllowsInstantFactionRecolor = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bMaySkip = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bSkipped = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNonAuthoritative = true;
};

class DOMINIONWORLD_API FDAPresentationRuntimeProjection
{
public:
    static bool BuildConstruction(const FDAPresentationContentManifest& Manifest, FName Faction,
        EDAConstructionState State, FDAPresentationPlaybackPlan& OutPlan);
    static bool BuildDamage(const FDAPresentationContentManifest& Manifest, FName Faction,
        const FDAWorldAssetRecord& Asset, FDAPresentationPlaybackPlan& OutPlan);
    static bool BuildCapture(const FDAPresentationContentManifest& Manifest,
        const FDAWorldAssetRecord& Asset, const FDACaptureRecord& Capture,
        FDAPresentationPlaybackPlan& OutPlan);
    static bool BuildCapture(const FDAPresentationContentManifest& Manifest,
        const FDACapturePresentationSnapshot& Snapshot,
        FDAPresentationPlaybackPlan& OutPlan);
    static bool BuildDaxton(const FDAPresentationContentManifest& Manifest,
        const FDADaxtonCampaignState& Daxton, FDAPresentationPlaybackPlan& OutPlan);
    static bool BuildAscension(const FDAPresentationContentManifest& Manifest,
        const FDAAscensionPresentationState& Ascension, FDAPresentationPlaybackPlan& OutPlan);
    static void Skip(FDAPresentationPlaybackPlan& InOutPlan);
};
