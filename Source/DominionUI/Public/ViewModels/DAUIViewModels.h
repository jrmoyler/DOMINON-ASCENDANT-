#pragma once

#include "CoreMinimal.h"
#include "Accessibility/DAAccessibilitySettings.h"
#include "Camera/DACameraModeController.h"
#include "Economy/DAEconomyTypes.h"
#include "Save/DACampaignSaveGame.h"
#include "Manifest/DAUIManifest.h"
#include "Commands/DAUIAuthoritativeService.h"

#include "DAUIViewModels.generated.h"

struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAWalletViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) double Capital = 0.0;
    UPROPERTY(BlueprintReadOnly) double Insight = 0.0;
    UPROPERTY(BlueprintReadOnly) double Influence = 0.0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACityHUDViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAWalletViewModel Wallets;
    UPROPERTY(BlueprintReadOnly) int32 Population = 0;
    UPROPERTY(BlueprintReadOnly) double Dependency = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FGuid> Hand;
    UPROPERTY(BlueprintReadOnly) FName ObjectiveId;
    UPROPERTY(BlueprintReadOnly) TArray<FDASynaraValueReason> DependencyReasons;
    UPROPERTY(BlueprintReadOnly) int64 CurrentDevelopmentCycle = 0;
    UPROPERTY(BlueprintReadOnly) int64 CurrentWorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAFounderHUDSource
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) float Health = 0.f;
    UPROPERTY(BlueprintReadOnly) float Guard = 0.f;
    UPROPERTY(BlueprintReadOnly) float Stamina = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FName> AbilityIds;
    UPROPERTY(BlueprintReadOnly) FName InteractionId;
    UPROPERTY(BlueprintReadOnly) FName PlayMode;
    UPROPERTY(BlueprintReadOnly) int64 CurrentDevelopmentCycle = 0;
    UPROPERTY(BlueprintReadOnly) int64 CurrentWorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAFounderHUDViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAFounderHUDSource Founder;
    UPROPERTY(BlueprintReadOnly) FName ObjectiveId;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACommandSquadViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName SquadId;
    UPROPERTY(BlueprintReadOnly) float Morale = 0.f;
    UPROPERTY(BlueprintReadOnly) float Supply = 0.f;
    UPROPERTY(BlueprintReadOnly) bool bSelected = false;
    UPROPERTY(BlueprintReadOnly) FVector SuggestedOrderDestination = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACommandHUDSource
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDACommandSquadViewModel> Squads;
    UPROPERTY(BlueprintReadOnly) TArray<FName> ControlZoneIds;
    UPROPERTY(BlueprintReadOnly) TArray<FName> KnownEnemyIds;
    UPROPERTY(BlueprintReadOnly) int32 CommandPoints = 0;
    UPROPERTY(BlueprintReadOnly) float Supply = 0.f;
    UPROPERTY(BlueprintReadOnly) float Sovereignty = 0.f;
    UPROPERTY(BlueprintReadOnly) FName FounderStatus;
    UPROPERTY(BlueprintReadOnly) FName TacticalAlertId;
    UPROPERTY(BlueprintReadOnly) FName PlayMode;
    UPROPERTY(BlueprintReadOnly) int64 CurrentDevelopmentCycle = 0;
    UPROPERTY(BlueprintReadOnly) int64 CurrentWorldTick = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDAFounderHUDLiveSource : public UInterface
{
    GENERATED_BODY()
};

class DOMINIONUI_API IDAFounderHUDLiveSource
{
    GENERATED_BODY()
public:
    virtual bool CaptureFounderHUDState(FDAFounderHUDSource& OutState, FString& OutError) const = 0;
};

UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class DOMINIONUI_API UDACommandHUDLiveSource : public UInterface
{
    GENERATED_BODY()
};

class DOMINIONUI_API IDACommandHUDLiveSource
{
    GENERATED_BODY()
public:
    virtual bool CaptureCommandHUDState(FDACommandHUDSource& OutState, FString& OutError) const = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACommandHUDViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDACommandHUDSource Command;
    UPROPERTY(BlueprintReadOnly) FName ObjectiveId;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAFacilityTooltipViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid WorldAssetId;
    UPROPERTY(BlueprintReadOnly) FDAWalletValues GrossOutput;
    UPROPERTY(BlueprintReadOnly) FDAWalletValues NetOutput;
    UPROPERTY(BlueprintReadOnly) float MaintenanceCapital = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FDAEconomyContribution> Contributions;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDADiplomacyRelationshipViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName RelationshipId;
    UPROPERTY(BlueprintReadOnly) float Trust = 0.f;
    UPROPERTY(BlueprintReadOnly) float Respect = 0.f;
    UPROPERTY(BlueprintReadOnly) float Fear = 0.f;
    UPROPERTY(BlueprintReadOnly) float Dependence = 0.f;
    UPROPERTY(BlueprintReadOnly) float Grievance = 0.f;
    UPROPERTY(BlueprintReadOnly) float Compatibility = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FDADiplomaticReason> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDADiplomacyPanelViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDADiplomacyRelationshipViewModel> Relationships;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAWorldMapViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDARegionState> Regions;
    UPROPERTY(BlueprintReadOnly) TArray<FDATradeRouteState> TradeCorridors;
    UPROPERTY(BlueprintReadOnly) TArray<FDAWorldMapAuthorityRecord> AuthorityReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAConquestRouteMeterViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RouteAssetId;
    UPROPERTY(BlueprintReadOnly) float Progress = 0.f;
    UPROPERTY(BlueprintReadOnly) float CapitalReward = 0.f;
    UPROPERTY(BlueprintReadOnly) float InsightReward = 0.f;
    UPROPERTY(BlueprintReadOnly) TArray<FName> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAConquestDashboardViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDACaptureRecord> Captures;
    UPROPERTY(BlueprintReadOnly) TArray<FDASurrenderRecord> Surrenders;
    UPROPERTY(BlueprintReadOnly) TArray<FName> OutcomeReasons;
    UPROPERTY(BlueprintReadOnly) TArray<FDAConquestRouteMeterViewModel> RouteMeters;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACollectionViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FCardInstance> Instances;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDADeckViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FGuid> Deck;
    UPROPERTY(BlueprintReadOnly) TArray<FGuid> Hand;
    UPROPERTY(BlueprintReadOnly) TArray<FGuid> Reserve;
    UPROPERTY(BlueprintReadOnly) bool bRulesValid = false;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RuleResults;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDABuildingInspectViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAWorldAssetRecord Asset;
    UPROPERTY(BlueprintReadOnly) bool bHasEconomyTrace = false;
    UPROPERTY(BlueprintReadOnly) FDAFacilityTooltipViewModel EconomyTrace;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACitizenInspectViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDACampaignCitizenSignal Citizen;
    UPROPERTY(BlueprintReadOnly) FName StoryState;
    UPROPERTY(BlueprintReadOnly) TArray<FDASynaraValueReason> RelationshipReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAFactionSupportViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName FactionId;
    UPROPERTY(BlueprintReadOnly) double Support = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FDASynaraValueReason> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAFactionPanelViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDAFactionSupportViewModel> Factions;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAResearchViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDAQuestContentUnlockRecord> AuthoredUnlocks;
    UPROPERTY(BlueprintReadOnly) TArray<FDAAuditEligibilitySourceRecord> EligibilityReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDATreatyBuilderViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName RelationshipId;
    UPROPERTY(BlueprintReadOnly) TArray<FDADiplomaticReason> TermReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAMainMenuViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bCampaignAuthorityAvailable = false;
    UPROPERTY(BlueprintReadOnly) FName CurrentRegionId;
    UPROPERTY(BlueprintReadOnly) int64 CurrentWorldTick = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDANewCampaignViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FName> PresetIds;
    UPROPERTY(BlueprintReadOnly) FName SelectedPresetId;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ValidationReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDASettingsScreenViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAAccessibilitySettings PersistedSettings;
    UPROPERTY(BlueprintReadOnly) TArray<FName> RemappableActionIds;
    UPROPERTY(BlueprintReadOnly) TArray<FString> BindingConflicts;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAAccessibilityScreenViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAAccessibilitySettings Draft;
    UPROPERTY(BlueprintReadOnly) TArray<FName> OptionIds;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ValidationReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAPauseScreenViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SaveSlotId;
    UPROPERTY(BlueprintReadOnly) bool bCampaignValid = false;
    UPROPERTY(BlueprintReadOnly) int64 WorldTick = 0;
    UPROPERTY(BlueprintReadOnly) int64 SignalRevision = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ValidationReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACardInspectViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FCardInstance Instance;
    UPROPERTY(BlueprintReadOnly) TArray<FString> SimulationTrace;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACraftingBlueprintViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName BlueprintId;
    UPROPERTY(BlueprintReadOnly) FName SourceQuestId;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FGuid> GrantedInstanceIds;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RequirementReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDABlueprintCraftingViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAWalletViewModel Wallets;
    UPROPERTY(BlueprintReadOnly) TArray<FDACraftingBlueprintViewModel> Blueprints;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAQuestJournalViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDAQuestSaveState> Quests;
    UPROPERTY(BlueprintReadOnly) TArray<FDAQuestObjectiveAssetBindingRecord> ObjectiveBindings;
    UPROPERTY(BlueprintReadOnly) FName TrackedObjectiveId;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAHistoryTimelineViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FName> HistoryTags;
    UPROPERTY(BlueprintReadOnly) TArray<FDANarrativeActionRecord> Actions;
    UPROPERTY(BlueprintReadOnly) TArray<FDAPromiseRecord> PromiseReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDACityMetricsViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FDAWalletViewModel Wallets;
    UPROPERTY(BlueprintReadOnly) int32 Population = 0;
    UPROPERTY(BlueprintReadOnly) double Dependency = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FDASynaraValueReason> DependencyReasons;
    UPROPERTY(BlueprintReadOnly) TArray<FDAFacilityTooltipViewModel> FacilityOutputs;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDALeaderResolutionViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDAUILeaderResolutionAuthorityRecord> Records;
    UPROPERTY(BlueprintReadOnly) TArray<FDASurrenderRecord> Surrenders;
    UPROPERTY(BlueprintReadOnly) TArray<FName> ResolutionReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAAscensionRewardViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDAUIAscensionRewardAuthorityRecord> AuthorityRewards;
    UPROPERTY(BlueprintReadOnly) TArray<FName> HistoryProofTags;
    UPROPERTY(BlueprintReadOnly) TArray<FDAQuestContentUnlockRecord> Rewards;
    UPROPERTY(BlueprintReadOnly) TArray<FDAAuditEligibilitySourceRecord> EligibilityReasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDASaveSlotViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString SlotId;
    UPROPERTY(BlueprintReadOnly) bool bExists = false;
    UPROPERTY(BlueprintReadOnly) bool bLoadable = false;
    UPROPERTY(BlueprintReadOnly) FString ValidationReason;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDASaveLoadViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FDASaveSlotViewModel> Slots;
    UPROPERTY(BlueprintReadOnly) int64 CurrentWorldTick = 0;
    UPROPERTY(BlueprintReadOnly) int64 SignalRevision = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAReturningPlayerRecapViewModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName CurrentObjectiveId;
    UPROPERTY(BlueprintReadOnly) TArray<FName> HistoryTags;
    UPROPERTY(BlueprintReadOnly) TArray<FDANarrativeActionRecord> RecentActions;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIIndexedActionPayloads
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TArray<FString> Values;
    /** Stable current-record payload used only by the controller activation binding. */
    UPROPERTY(BlueprintReadOnly) FString ControllerValue;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIScreenProjection
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName ScreenId;
    UPROPERTY(BlueprintReadOnly) bool bAuthoritative = false;
    UPROPERTY(BlueprintReadOnly) TArray<FName> DataChannels;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RenderedValues;
    UPROPERTY(BlueprintReadOnly) TMap<FName, FString> ActionPayloads;
    UPROPERTY(BlueprintReadOnly) TMap<FName, FDAUIIndexedActionPayloads> IndexedActionPayloads;
    UPROPERTY(BlueprintReadOnly) FDACityHUDViewModel CityHUD;
    UPROPERTY(BlueprintReadOnly) FDAFounderHUDViewModel FounderHUD;
    UPROPERTY(BlueprintReadOnly) FDACommandHUDViewModel CommandHUD;
    UPROPERTY(BlueprintReadOnly) FDACollectionViewModel Collection;
    UPROPERTY(BlueprintReadOnly) FDADeckViewModel Deck;
    UPROPERTY(BlueprintReadOnly) FDABuildingInspectViewModel Building;
    UPROPERTY(BlueprintReadOnly) FDACitizenInspectViewModel Citizen;
    UPROPERTY(BlueprintReadOnly) FDADiplomacyPanelViewModel Diplomacy;
    UPROPERTY(BlueprintReadOnly) FDATreatyBuilderViewModel Treaty;
    UPROPERTY(BlueprintReadOnly) FDAWorldMapViewModel WorldMap;
    UPROPERTY(BlueprintReadOnly) FDAConquestDashboardViewModel Conquest;
    UPROPERTY(BlueprintReadOnly) FDAFactionPanelViewModel Factions;
    UPROPERTY(BlueprintReadOnly) FDAResearchViewModel Research;
    UPROPERTY(BlueprintReadOnly) FDAAccessibilitySettings Accessibility;
    UPROPERTY(BlueprintReadOnly) FDAMainMenuViewModel MainMenu;
    UPROPERTY(BlueprintReadOnly) FDANewCampaignViewModel NewCampaign;
    UPROPERTY(BlueprintReadOnly) FDASettingsScreenViewModel Settings;
    UPROPERTY(BlueprintReadOnly) FDAAccessibilityScreenViewModel AccessibilityScreen;
    UPROPERTY(BlueprintReadOnly) FDAPauseScreenViewModel Pause;
    UPROPERTY(BlueprintReadOnly) FDACardInspectViewModel CardInspect;
    UPROPERTY(BlueprintReadOnly) FDABlueprintCraftingViewModel Crafting;
    UPROPERTY(BlueprintReadOnly) FDAQuestJournalViewModel QuestJournal;
    UPROPERTY(BlueprintReadOnly) FDAHistoryTimelineViewModel HistoryTimeline;
    UPROPERTY(BlueprintReadOnly) FDACityMetricsViewModel CityMetrics;
    UPROPERTY(BlueprintReadOnly) FDALeaderResolutionViewModel LeaderResolution;
    UPROPERTY(BlueprintReadOnly) FDAAscensionRewardViewModel AscensionReward;
    UPROPERTY(BlueprintReadOnly) FDASaveLoadViewModel SaveLoad;
    UPROPERTY(BlueprintReadOnly) FDAReturningPlayerRecapViewModel ReturningPlayerRecap;
    UPROPERTY(BlueprintReadOnly) FDAUIAuthoritativeFeatureSnapshot LateFeatureAuthority;
};

UENUM(BlueprintType)
enum class EDAUIOverlayKind : uint8
{
    Power, Water, Data, Employment, Housing, Happiness, Dependency, Adjacency
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIEmploymentOverlayRow
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName CitizenId;
    UPROPERTY(BlueprintReadOnly) FName JobId;
    UPROPERTY(BlueprintReadOnly) FGuid FacilityWorldAssetId;
    UPROPERTY(BlueprintReadOnly) int32 OpenPositions = 0;
    UPROPERTY(BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIHousingOverlayRow
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName CitizenId;
    UPROPERTY(BlueprintReadOnly) FGuid HomeWorldAssetId;
    UPROPERTY(BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUICitizenValueOverlayRow
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName CitizenId;
    UPROPERTY(BlueprintReadOnly) double Value = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FDASynaraValueReason> Reasons;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIAdjacencyOverlayEdge
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid FromWorldAssetId;
    UPROPERTY(BlueprintReadOnly) FGuid ToWorldAssetId;
    UPROPERTY(BlueprintReadOnly) FIntPoint FromGrid;
    UPROPERTY(BlueprintReadOnly) FIntPoint ToGrid;
    UPROPERTY(BlueprintReadOnly) bool bAdjacent = false;
    UPROPERTY(BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIOverlayReadModel
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName OverlayId;
    UPROPERTY(BlueprintReadOnly) EDAUIOverlayKind Kind = EDAUIOverlayKind::Power;
    UPROPERTY(BlueprintReadOnly) bool bAuthoritative = false;
    UPROPERTY(BlueprintReadOnly) bool bColorIndependentMarkers = true;
    UPROPERTY(BlueprintReadOnly) FName MarkerShape;
    UPROPERTY(BlueprintReadOnly) TArray<FDACampaignUtilitySignal> UtilitySignals;
    UPROPERTY(BlueprintReadOnly) TArray<FDACampaignCitizenSignal> Citizens;
    UPROPERTY(BlueprintReadOnly) TArray<FDAWorldAssetRecord> WorldAssets;
    UPROPERTY(BlueprintReadOnly) TArray<FDASynaraValueReason> Reasons;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUIEmploymentOverlayRow> EmploymentRows;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUIHousingOverlayRow> HousingRows;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUICitizenValueOverlayRow> CitizenValueRows;
    UPROPERTY(BlueprintReadOnly) double DependencyValue = 0.0;
    UPROPERTY(BlueprintReadOnly) TArray<FDAUIAdjacencyOverlayEdge> AdjacencyEdges;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RenderedValues;
};

/** Snapshot-backed typed provider; callers refresh it only from the authoritative campaign owner. */
DECLARE_MULTICAST_DELEGATE(FDAUIProjectionInvalidated);
UCLASS(BlueprintType)
class DOMINIONUI_API UDAUIViewModelProvider : public UObject
{
    GENERATED_BODY()
public:
    FDAUIProjectionInvalidated OnProjectionInvalidated;
    void SetAuthoritativeSnapshot(const FDACampaignSnapshot& InCampaign);
    void SetFounderHUDSource(const FDAFounderHUDSource& InSource)
    { FounderSource = InSource; OnProjectionInvalidated.Broadcast(); }
    void SetCommandHUDSource(const FDACommandHUDSource& InSource)
    { CommandSource = InSource; OnProjectionInvalidated.Broadcast(); }
    void SetAccessibilitySettings(const FDAAccessibilitySettings& InSettings)
    { AccessibilitySettings = InSettings; OnProjectionInvalidated.Broadcast(); }
    void SetFacilityOutputs(const TArray<FDAFacilityOutput>& InOutputs)
    { FacilityOutputs = InOutputs; OnProjectionInvalidated.Broadcast(); }
    void SetStableSelection(FName ScreenId, FName SelectionId)
    { StableSelections.Add(ScreenId, SelectionId); OnProjectionInvalidated.Broadcast(); }
    bool BuildScreen(const FDAUIScreenDescriptor& Descriptor, FDAUIScreenProjection& Out) const;
    bool BuildOverlay(const FDAUIOverlayDescriptor& Descriptor, bool bColorIndependentMarkers,
        FDAUIOverlayReadModel& Out) const;
    bool BindAuthoritativeWorld(class UDAWorldStateSubsystem* InWorld);
    void BindRuntimePlayer(class APlayerController* PlayerController);
    void RefreshRuntimeSources();
    bool RegisterFounderHUDLiveSource(UObject* Source, FString& OutError);
    bool RegisterCommandHUDLiveSource(UObject* Source, FString& OutError);
    void UnregisterFounderHUDLiveSource(UObject* Source);
    void UnregisterCommandHUDLiveSource(UObject* Source);
    bool ApplySelectionCommand(FName CommandId, FName SourceScreenId,
        const FString& PayloadJson, FString& OutError);
    bool CycleStableSelection(FName ScreenId, int32 Direction, FString& OutError);
    virtual void BeginDestroy() override;
private:
    void HandleCampaignCommitted(TSharedRef<const FDACampaignSnapshot> Snapshot);
    void HandleAccessibilityChanged(const FDAAccessibilityRuntimePolicy& Policy);
    void HandleCombatAttributeChanged(const FOnAttributeChangeData& ChangeData);
    UFUNCTION() void HandleCameraModeChanged(EDAPlayMode PreviousMode, EDAPlayMode NewMode);
    UPROPERTY(Transient) FDACampaignSnapshot Campaign;
    UPROPERTY(Transient) FDAFounderHUDSource FounderSource;
    UPROPERTY(Transient) FDACommandHUDSource CommandSource;
    UPROPERTY(Transient) FDAAccessibilitySettings AccessibilitySettings = FDAAccessibilitySettings::MakeDefaults();
    UPROPERTY(Transient) TMap<FName, FName> StableSelections;
    UPROPERTY(Transient) TMap<FName, float> StableAxes;
    UPROPERTY(Transient) TArray<FDAFacilityOutput> FacilityOutputs;
    UPROPERTY(Transient) bool bDeckRulesValid = false;
    UPROPERTY(Transient) TArray<FString> DeckRuleResults;
    bool bHasAuthority = false;
    bool bHasFounderLiveAuthority = false;
    bool bHasCommandLiveAuthority = false;
    TWeakObjectPtr<class UDAWorldStateSubsystem> WorldAuthority;
    TWeakObjectPtr<class APlayerController> RuntimePlayerController;
    TWeakObjectPtr<class UDAAccessibilitySettingsSubsystem> AccessibilityAuthority;
    TWeakObjectPtr<class UAbilitySystemComponent> AbilitySystemAuthority;
    TWeakObjectPtr<class UDACameraModeController> CameraModeAuthority;
    TWeakObjectPtr<class UDAContentRegistrySubsystem> ContentAuthority;
    TWeakObjectPtr<class UDAUIAuthoritativeFeatureRegistrySubsystem> FeatureAuthority;
    TWeakObjectPtr<UObject> FounderHUDLiveAuthority;
    TWeakObjectPtr<UObject> CommandHUDLiveAuthority;
    FDelegateHandle CampaignCommittedHandle;
    FDelegateHandle AccessibilityChangedHandle;
    FDelegateHandle HealthChangedHandle;
    FDelegateHandle GuardChangedHandle;
    FDelegateHandle StaminaChangedHandle;
};

struct DOMINIONUI_API FDAUIViewModelFactory
{
    static FString BuildAccessibilityPayload(FName ScreenId, FName ActionId,
        const FDAAccessibilitySettings& Settings);
    static FDACityHUDViewModel BuildCityHUD(const FDACampaignSnapshot& Campaign);
    static FDAFounderHUDViewModel BuildFounderHUD(const FDACampaignSnapshot& Campaign, const FDAFounderHUDSource& Source);
    static FDACommandHUDViewModel BuildCommandHUD(const FDACampaignSnapshot& Campaign, const FDACommandHUDSource& Source);
    static FDAFacilityTooltipViewModel BuildFacilityTooltip(const FDAFacilityOutput& AuthoritativeOutput);
    static FDADiplomacyPanelViewModel BuildDiplomacy(const FDACampaignSnapshot& Campaign);
    static FDAWorldMapViewModel BuildWorldMap(const FDACampaignSnapshot& Campaign);
    static FDAConquestDashboardViewModel BuildConquest(const FDACampaignSnapshot& Campaign);
    static FDACollectionViewModel BuildCollection(const FDACampaignSnapshot& Campaign);
    static FDADeckViewModel BuildDeck(const FDACampaignSnapshot& Campaign);
    static FDABuildingInspectViewModel BuildBuildingInspect(const FDACampaignSnapshot& Campaign,
        FGuid WorldAssetId, const FDAFacilityOutput* AuthoritativeOutput);
    static FDACitizenInspectViewModel BuildCitizenInspect(const FDACampaignSnapshot& Campaign, FName CitizenId);
    static FDAFactionPanelViewModel BuildFactionPanel(const FDACampaignSnapshot& Campaign);
    static FDAResearchViewModel BuildResearch(const FDACampaignSnapshot& Campaign);
    static FDATreatyBuilderViewModel BuildTreaty(const FDACampaignSnapshot& Campaign, FName RelationshipId);
};
