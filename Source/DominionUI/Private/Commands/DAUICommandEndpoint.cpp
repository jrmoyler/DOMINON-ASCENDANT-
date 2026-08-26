#include "Commands/DAUICommandEndpoint.h"
#include "Commands/DAUIAuthoritativeService.h"
#include "Commands/DAUIAuthoritativeFeatureSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Accessibility/DAAccessibilitySettings.h"
#include "Cards/DADeckRules.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentRegistrySubsystem.h"
#include "Diplomacy/DADiplomacySystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Founder/DAFounderCharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Narrative/DAFirstHourCampaignCoordinatorSubsystem.h"
#include "Regions/DAWorldStateSubsystem.h"
#include "Save/DASaveService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Time/DASimulationClockSubsystem.h"
#include "Units/DASquadEntity.h"
#include "Command/DACommandSubsystem.h"

namespace
{
    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FString& Out)
    {
        return Object.IsValid() && Object->TryGetStringField(Field, Out) && !Out.IsEmpty();
    }

    void ReadOptionalName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FName& Out)
    {
        FString Value;
        if (ReadString(Object, Field, Value)) Out = FName(*Value);
    }

    void ReadOptionalGuid(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FGuid& Out)
    {
        FString Value;
        if (ReadString(Object, Field, Value)) FGuid::Parse(Value, Out);
    }

    bool RequireName(const FName Value, const TCHAR* Field, FString& Error)
    {
        if (!Value.IsNone()) return true;
        Error = FString::Printf(TEXT("Command payload requires '%s'."), Field);
        return false;
    }

    bool RequireGuid(const FGuid& Value, const TCHAR* Field, FString& Error)
    {
        if (Value.IsValid()) return true;
        Error = FString::Printf(TEXT("Command payload requires valid '%s'."), Field);
        return false;
    }

    bool RequireSlot(const FString& Value, FString& Error)
    {
        if (!Value.IsEmpty()) return true;
        Error = TEXT("Command payload requires 'saveSlotId'.");
        return false;
    }

    bool RequireNumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        double& OutValue, FString& OutError)
    {
        if (Object.IsValid() && Object->TryGetNumberField(Field, OutValue)
            && FMath::IsFinite(OutValue)) return true;
        OutError = FString::Printf(TEXT("Command payload requires finite number '%s'."), Field);
        return false;
    }

    EDADiplomaticMetric MetricFromName(const FName Metric, bool& bOutValid)
    {
        bOutValid = true;
        if (Metric == TEXT("trust")) return EDADiplomaticMetric::Trust;
        if (Metric == TEXT("respect")) return EDADiplomaticMetric::Respect;
        if (Metric == TEXT("fear")) return EDADiplomaticMetric::Fear;
        if (Metric == TEXT("dependence")) return EDADiplomaticMetric::Dependence;
        if (Metric == TEXT("grievance")) return EDADiplomaticMetric::Grievance;
        if (Metric == TEXT("compatibility")) return EDADiplomaticMetric::Compatibility;
        bOutValid = false;
        return EDADiplomaticMetric::Trust;
    }

    EDACommandOrder OrderFromName(const FName Order, bool& bOutValid)
    {
        bOutValid = true;
        if (Order == TEXT("move")) return EDACommandOrder::Move;
        if (Order == TEXT("attack")) return EDACommandOrder::Attack;
        if (Order == TEXT("hold")) return EDACommandOrder::Hold;
        if (Order == TEXT("defend")) return EDACommandOrder::Defend;
        if (Order == TEXT("capture")) return EDACommandOrder::Capture;
        if (Order == TEXT("escort")) return EDACommandOrder::Escort;
        if (Order == TEXT("retreat")) return EDACommandOrder::Retreat;
        if (Order == TEXT("ability")) return EDACommandOrder::Ability;
        bOutValid = false;
        return EDACommandOrder::Hold;
    }

    bool ReadAccessibilitySettings(const FString& Json, FDAAccessibilitySettings& InOut, FString& OutError)
    {
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
        { OutError = TEXT("Accessibility payload must be a valid JSON object."); return false; }
        if (!Root->TryGetNumberField(TEXT("textScale"), InOut.TextScale)
            || !Root->TryGetBoolField(TEXT("subtitles"), InOut.bSubtitles)
            || !Root->TryGetBoolField(TEXT("speakerLabels"), InOut.bSpeakerLabels)
            || !Root->TryGetNumberField(TEXT("subtitleBackgroundOpacity"), InOut.SubtitleBackgroundOpacity)
            || !Root->TryGetBoolField(TEXT("colorIndependentMarkers"), InOut.bColorIndependentMarkers)
            || !Root->TryGetNumberField(TEXT("cameraShake"), InOut.CameraShake)
            || !Root->TryGetNumberField(TEXT("fieldOfView"), InOut.FieldOfView)
            || !Root->TryGetBoolField(TEXT("motionBlur"), InOut.bMotionBlur)
            || !Root->TryGetBoolField(TEXT("reducedMotion"), InOut.bReducedMotion)
            || !Root->TryGetBoolField(TEXT("reducedFlash"), InOut.bReducedFlash)
            || !Root->TryGetBoolField(TEXT("tacticalPause"), InOut.bTacticalPause)
            || !Root->TryGetNumberField(TEXT("aimAssist"), InOut.AimAssist)
            || !Root->TryGetNumberField(TEXT("buildSnapStrength"), InOut.BuildSnapStrength)
            || !Root->TryGetBoolField(TEXT("tutorialRecall"), InOut.bTutorialRecall))
        { OutError = TEXT("Accessibility payload requires every frozen scalar field."); return false; }
        int32 EnumValue = static_cast<int32>(InOut.ColorVisionPreset);
        if (!Root->TryGetNumberField(TEXT("colorVisionPreset"), EnumValue))
        { OutError = TEXT("Accessibility payload requires 'colorVisionPreset'."); return false; }
        InOut.ColorVisionPreset = static_cast<EDAColorVisionPreset>(EnumValue);
        EnumValue = static_cast<int32>(InOut.TooltipMode);
        if (!Root->TryGetNumberField(TEXT("tooltipMode"), EnumValue))
        { OutError = TEXT("Accessibility payload requires 'tooltipMode'."); return false; }
        InOut.TooltipMode = static_cast<EDATooltipMode>(EnumValue);
        const auto ReadBindings = [&Root, &OutError](const TCHAR* Field, TMap<FName, FKey>& OutBindings)
        {
            const TSharedPtr<FJsonObject>* Object = nullptr;
            if (!Root->TryGetObjectField(Field, Object) || Object == nullptr)
            { OutError = FString::Printf(TEXT("Accessibility payload requires '%s'."), Field); return false; }
            OutBindings.Reset();
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Object)->Values)
            {
                FString KeyName;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(KeyName))
                { OutError = FString::Printf(TEXT("'%s' must map action IDs to key names."), Field); return false; }
                OutBindings.Add(FName(*Pair.Key), FKey(FName(*KeyName)));
            }
            return true;
        };
        if (!ReadBindings(TEXT("keyboardBindings"), InOut.KeyboardBindings)
            || !ReadBindings(TEXT("controllerBindings"), InOut.ControllerBindings)) return false;
        const TSharedPtr<FJsonObject>* HoldModes = nullptr;
        if (!Root->TryGetObjectField(TEXT("holdToggleModes"), HoldModes) || HoldModes == nullptr)
        { OutError = TEXT("Accessibility payload requires 'holdToggleModes'."); return false; }
        if (HoldModes != nullptr)
        {
            InOut.HoldToggleModes.Reset();
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*HoldModes)->Values)
            {
                double Value = 0.0;
                if (!Pair.Value.IsValid() || !Pair.Value->TryGetNumber(Value)
                    || Value != FMath::RoundToDouble(Value) || Value < 0.0
                    || Value > static_cast<double>(static_cast<uint8>(EDAHoldToggleMode::Press)))
                { OutError = TEXT("'holdToggleModes' must map known action IDs to enum values."); return false; }
                InOut.HoldToggleModes.Add(FName(*Pair.Key), static_cast<EDAHoldToggleMode>(static_cast<int32>(Value)));
            }
        }
        return InOut.Validate(OutError);
    }

}

bool FDAUIAuthoritativeCommandPayload::ParseAndValidate(
    const FName CommandId, const FString& Json, FDAUIAuthoritativeCommandPayload& Out, FString& OutError)
{
    Out = {};
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
    { OutError = TEXT("Command payload must be a valid JSON object."); return false; }

    ReadOptionalGuid(Root, TEXT("cardInstanceId"), Out.CardInstanceId);
    ReadOptionalGuid(Root, TEXT("replacementCardInstanceId"), Out.ReplacementCardInstanceId);
    ReadOptionalGuid(Root, TEXT("worldAssetId"), Out.WorldAssetId);
    ReadOptionalGuid(Root, TEXT("routeAssetId"), Out.RouteAssetId);
    ReadOptionalGuid(Root, TEXT("facilityWorldAssetId"), Out.FacilityWorldAssetId);
    ReadOptionalName(Root, TEXT("cardDefinitionId"), Out.CardDefinitionId);
    ReadOptionalName(Root, TEXT("blueprintId"), Out.BlueprintId);
    ReadOptionalName(Root, TEXT("cityId"), Out.CityId);
    ReadOptionalName(Root, TEXT("citizenId"), Out.CitizenId);
    ReadOptionalName(Root, TEXT("jobId"), Out.JobId);
    ReadOptionalName(Root, TEXT("squadId"), Out.SquadId);
    ReadOptionalName(Root, TEXT("order"), Out.Order);
    ReadOptionalName(Root, TEXT("targetActorId"), Out.OrderTarget.TargetActorId);
    ReadOptionalName(Root, TEXT("relationshipId"), Out.RelationshipId);
    ReadOptionalName(Root, TEXT("regionId"), Out.RegionId);
    ReadOptionalName(Root, TEXT("questId"), Out.QuestId);
    ReadOptionalName(Root, TEXT("historyRecordId"), Out.HistoryRecordId);
    ReadOptionalName(Root, TEXT("researchId"), Out.ResearchId);
    ReadOptionalName(Root, TEXT("metricId"), Out.MetricId);
    ReadOptionalName(Root, TEXT("leaderResolutionId"), Out.ResolutionId);
    ReadOptionalName(Root, TEXT("rewardId"), Out.RewardId);
    ReadOptionalName(Root, TEXT("campaignPresetId"), Out.CampaignPresetId);
    ReadOptionalName(Root, TEXT("interactionId"), Out.InteractionId);
    ReadOptionalName(Root, TEXT("abilityId"), Out.AbilityId);
    ReadString(Root, TEXT("saveSlotId"), Out.SaveSlotId);
    Root->TryGetNumberField(TEXT("axis"), Out.Axis);
    const TSharedPtr<FJsonObject>* Destination = nullptr;
    if (Root->TryGetObjectField(TEXT("destination"), Destination) && Destination != nullptr)
    {
        (*Destination)->TryGetNumberField(TEXT("x"), Out.OrderTarget.Destination.X);
        (*Destination)->TryGetNumberField(TEXT("y"), Out.OrderTarget.Destination.Y);
        (*Destination)->TryGetNumberField(TEXT("z"), Out.OrderTarget.Destination.Z);
    }
    const TArray<TSharedPtr<FJsonValue>>* Terms = nullptr;
    if (Root->TryGetArrayField(TEXT("treatyTerms"), Terms) && Terms != nullptr)
        for (const TSharedPtr<FJsonValue>& Value : *Terms)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object)
            { OutError = TEXT("Treaty term must be a JSON object."); return false; }
            const TSharedPtr<FJsonObject> TermObject = Value->AsObject();
            FString TermId; FString Metric; double Magnitude = 0.0;
            if (!ReadString(TermObject, TEXT("termId"), TermId)
                || !ReadString(TermObject, TEXT("metric"), Metric)
                || !RequireNumberField(TermObject, TEXT("magnitude"), Magnitude, OutError))
            { if (OutError.IsEmpty()) OutError = TEXT("Every treaty term requires all fields."); return false; }
            FDAUITreatyTermPayload& Term = Out.TreatyTerms.Emplace_GetRef();
            Term.TermId = FName(*TermId); Term.Metric = FName(*Metric);
            Term.Magnitude = static_cast<float>(Magnitude);
        }

    const FString Command = CommandId.ToString();
    if (Command == TEXT("command.city.place_building"))
    {
        if (!RequireGuid(Out.CardInstanceId, TEXT("cardInstanceId"), OutError)
            || !RequireName(Out.CityId, TEXT("cityId"), OutError)) return false;
        double GridX = 0.0; double GridY = 0.0; double FootprintX = 0.0;
        double FootprintY = 0.0; double RotationValue = 0.0;
        if (!RequireNumberField(Root, TEXT("gridX"), GridX, OutError)
            || !RequireNumberField(Root, TEXT("gridY"), GridY, OutError)
            || !RequireNumberField(Root, TEXT("footprintX"), FootprintX, OutError)
            || !RequireNumberField(Root, TEXT("footprintY"), FootprintY, OutError)
            || !RequireNumberField(Root, TEXT("rotation"), RotationValue, OutError)) return false;
        if (FootprintX != FMath::RoundToDouble(FootprintX)
            || FootprintY != FMath::RoundToDouble(FootprintY)
            || RotationValue != FMath::RoundToDouble(RotationValue))
        { OutError = TEXT("Placement footprint and rotation fields must be integers."); return false; }
        if (GridX < MIN_int32 || GridX > MAX_int32 || GridY < MIN_int32 || GridY > MAX_int32
            || FootprintX < 1 || FootprintX > MAX_int32 || FootprintY < 1 || FootprintY > MAX_int32
            || RotationValue < 0 || RotationValue > 3)
        { OutError = TEXT("Placement requires positive footprint and rotation in [0,3]."); return false; }
        Out.GridCoordinate.X = GridX; Out.GridCoordinate.Y = GridY;
        Out.GridCoordinate.FootprintX = static_cast<int32>(FootprintX);
        Out.GridCoordinate.FootprintY = static_cast<int32>(FootprintY);
        Out.GridCoordinate.Rotation = static_cast<uint8>(RotationValue);
    }
    else if (Command == TEXT("command.city.select_card") || Command == TEXT("command.collection.inspect_card")
        || Command == TEXT("command.deck.add_card") || Command == TEXT("command.deck.remove_card")
        || Command == TEXT("command.card.rotate"))
    {
        if (!RequireGuid(Out.CardInstanceId, TEXT("cardInstanceId"), OutError)) return false;
        if ((Command == TEXT("command.deck.add_card") || Command == TEXT("command.deck.remove_card"))
            && !RequireGuid(Out.ReplacementCardInstanceId,
                TEXT("replacementCardInstanceId"), OutError)) return false;
        if (Command == TEXT("command.card.rotate") && (!FMath::IsFinite(Out.Axis) || Out.Axis == 0.f))
        { OutError = TEXT("Card rotation requires a nonzero finite axis."); return false; }
    }
    else if (Command == TEXT("command.city.cancel_build") || Command == TEXT("command.building.track"))
    { if (!RequireGuid(Out.WorldAssetId, TEXT("worldAssetId"), OutError)) return false; }
    else if (Command == TEXT("command.citizen.assign_job"))
    {
        if (!RequireName(Out.CitizenId, TEXT("citizenId"), OutError)
            || !RequireName(Out.JobId, TEXT("jobId"), OutError)
            || !RequireGuid(Out.FacilityWorldAssetId, TEXT("facilityWorldAssetId"), OutError)) return false;
    }
    else if (Command == TEXT("command.command.select_squad"))
    { if (!RequireName(Out.SquadId, TEXT("squadId"), OutError)) return false; }
    else if (Command == TEXT("command.command.issue_order"))
    {
        bool bOrderValid = false; OrderFromName(Out.Order, bOrderValid);
        if (!RequireName(Out.SquadId, TEXT("squadId"), OutError)
            || !RequireName(Out.Order, TEXT("order"), OutError)) return false;
        const TSharedPtr<FJsonObject>* DestinationObject = nullptr;
        double DestinationX = 0.0; double DestinationY = 0.0; double DestinationZ = 0.0;
        if (!Root->TryGetObjectField(TEXT("destination"), DestinationObject)
            || DestinationObject == nullptr
            || !RequireNumberField(*DestinationObject, TEXT("x"), DestinationX, OutError)
            || !RequireNumberField(*DestinationObject, TEXT("y"), DestinationY, OutError)
            || !RequireNumberField(*DestinationObject, TEXT("z"), DestinationZ, OutError)) return false;
        if (!bOrderValid || !FMath::IsFinite(Out.OrderTarget.Destination.X)
            || !FMath::IsFinite(Out.OrderTarget.Destination.Y)
            || !FMath::IsFinite(Out.OrderTarget.Destination.Z))
        { OutError = TEXT("Squad order requires a known order and finite destination."); return false; }
    }
    else if (Command == TEXT("command.crafting.craft"))
    {
        double QuantityValue = 0.0;
        if (!RequireName(Out.BlueprintId, TEXT("blueprintId"), OutError)
            || !RequireNumberField(Root, TEXT("quantity"), QuantityValue, OutError)
            || QuantityValue != FMath::RoundToDouble(QuantityValue)
            || QuantityValue < 1 || QuantityValue > 100)
        { if (OutError.IsEmpty()) OutError = TEXT("Craft quantity must be between 1 and 100."); return false; }
        Out.Quantity = static_cast<int32>(QuantityValue);
    }
    else if (Command == TEXT("command.treaty.commit"))
    {
        if (!RequireName(Out.RelationshipId, TEXT("relationshipId"), OutError)) return false;
        if (Out.TreatyTerms.IsEmpty()) { OutError = TEXT("Treaty commit requires 'treatyTerms'."); return false; }
        for (const FDAUITreatyTermPayload& Term : Out.TreatyTerms)
        {
            bool bMetricValid = false; MetricFromName(Term.Metric, bMetricValid);
            if (Term.TermId.IsNone() || !bMetricValid || !FMath::IsFinite(Term.Magnitude) || Term.Magnitude == 0.f)
            { OutError = TEXT("Every treaty term requires ID, known metric, and nonzero magnitude."); return false; }
        }
    }
    else if (Command == TEXT("command.world.travel"))
    { if (!RequireName(Out.RegionId, TEXT("regionId"), OutError)) return false; }
    else if (Command == TEXT("command.quest.track"))
    { if (!RequireName(Out.QuestId, TEXT("questId"), OutError)) return false; }
    else if (Command == TEXT("command.history.inspect"))
    { if (!RequireName(Out.HistoryRecordId, TEXT("historyRecordId"), OutError)) return false; }
    else if (Command == TEXT("command.research.start"))
    { if (!RequireName(Out.ResearchId, TEXT("researchId"), OutError)) return false; }
    else if (Command == TEXT("command.metrics.inspect"))
    { if (!RequireName(Out.MetricId, TEXT("metricId"), OutError)) return false; }
    else if (Command == TEXT("command.conquest.choose_route"))
    { if (!RequireGuid(Out.RouteAssetId, TEXT("routeAssetId"), OutError)) return false; }
    else if (Command == TEXT("command.leader.confirm_resolution"))
    { if (!RequireName(Out.ResolutionId, TEXT("leaderResolutionId"), OutError)) return false; }
    else if (Command == TEXT("command.ascension.claim"))
    { if (!RequireName(Out.RewardId, TEXT("rewardId"), OutError)) return false; }
    else if (Command == TEXT("command.save.write") || Command == TEXT("command.save.load"))
    { if (!RequireSlot(Out.SaveSlotId, OutError)) return false; }
    else if (Command == TEXT("command.campaign.create"))
    { if (!RequireName(Out.CampaignPresetId, TEXT("campaignPresetId"), OutError)) return false; }
    else if (Command == TEXT("command.founder.interact"))
    { if (!RequireName(Out.InteractionId, TEXT("interactionId"), OutError)) return false; }
    else if (Command == TEXT("command.founder.activate_ability"))
    { if (!RequireName(Out.AbilityId, TEXT("abilityId"), OutError)) return false; }
    OutError.Reset();
    return true;
}

void UDAUICommandEndpointSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UDAWorldStateSubsystem>();
    Collection.InitializeDependency<UDACommandSubsystem>();
    Collection.InitializeDependency<UDAAccessibilitySettingsSubsystem>();
    Collection.InitializeDependency<UDAUIAuthoritativeFeatureRegistrySubsystem>();
    Collection.InitializeDependency<UDAUIAuthoritativeFeatureSubsystem>();
    Collection.InitializeDependency<UDAFirstHourCampaignCoordinatorSubsystem>();
}

bool UDAUICommandEndpointSubsystem::CommitCampaign(
    UDAWorldStateSubsystem& World, const FDACampaignSnapshot& Candidate, FString& OutError) const
{
    const FDACampaignSnapshot& Authority = World.GetPersistentCampaign();
    if (World.TryCommitPersistentCampaign(Candidate, Authority.NarrativeState.MutationRevision,
        Authority.LiveSignals.MutationRevision, Authority.WorldState.CurrentWorldTick))
    { OutError.Reset(); return true; }
    OutError = TEXT("Canonical campaign compare-and-swap transaction was rejected.");
    return false;
}

void UDAUICommandEndpointSubsystem::RecordSuccess(const FDAUICommandRequest& Request)
{
    FDAUICommandAuditRecord& Record = AuditRecords.Emplace_GetRef();
    Record.CommandId = Request.CommandId; Record.SourceScreenId = Request.SourceScreenId;
    Record.PayloadJson = Request.PayloadJson;
    if (const UDAWorldStateSubsystem* World = GetGameInstance()->GetSubsystem<UDAWorldStateSubsystem>())
        Record.WorldTick = World->GetCurrentWorldTick();
}

bool UDAUICommandEndpointSubsystem::ExecuteGameplayUICommand_Implementation(
    const FDAUICommandRequest& Request, FString& OutError)
{
    FDAUIAuthoritativeCommandPayload Payload;
    if (!FDAUIAuthoritativeCommandPayload::ParseAndValidate(
        Request.CommandId, Request.PayloadJson, Payload, OutError)) return false;
    if (!ExecuteCampaignCommand(Request, Payload, OutError)) return false;
    RecordSuccess(Request);
    return true;
}

bool UDAUICommandEndpointSubsystem::ExecuteCampaignCommand(const FDAUICommandRequest& Request,
    const FDAUIAuthoritativeCommandPayload& Payload, FString& OutError)
{
    UGameInstance* GameInstance = GetGameInstance();
    UDAWorldStateSubsystem* World = GameInstance == nullptr ? nullptr
        : GameInstance->GetSubsystem<UDAWorldStateSubsystem>();
    if (GameInstance == nullptr || World == nullptr)
    { OutError = TEXT("Authoritative campaign services are unavailable."); return false; }
    const FString Command = Request.CommandId.ToString();
    if (Command == TEXT("command.campaign.create"))
        return World->InitializeVerticalSliceState(TEXT("region.synara_frontier"), 0)
            ? (OutError.Reset(), true) : (OutError = TEXT("Campaign service rejected new campaign."), false);
    if (Command == TEXT("command.settings.apply") || Command == TEXT("command.accessibility.apply"))
    {
        UDAAccessibilitySettingsSubsystem* Accessibility =
            GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>();
        if (Accessibility == nullptr)
        { OutError = TEXT("Accessibility settings service is unavailable."); return false; }
        FDAAccessibilitySettings Candidate = Accessibility->GetSettings();
        if (!ReadAccessibilitySettings(Request.PayloadJson, Candidate, OutError)) return false;
        return Accessibility->ApplyAndPersist(Candidate, OutError);
    }
    if (Command == TEXT("command.time.tactical_pause"))
    {
        UDASimulationClockSubsystem* Clock = GameInstance->GetSubsystem<UDASimulationClockSubsystem>();
        if (Clock == nullptr) { OutError = TEXT("Simulation clock is unavailable."); return false; }
        Clock->SetPaused(!Clock->IsPaused()); OutError.Reset(); return true;
    }
    if (Command == TEXT("command.city.place_building"))
    {
        const UDAAccessibilitySettingsSubsystem* Accessibility =
            GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>();
        const float SnapStrength = Accessibility == nullptr ? 1.f
            : Accessibility->GetSettings().BuildSnapStrength;
        const FIntPoint GridOrigin = UDAGameplayAccessibilityAdapter::ResolveBuildGridOrigin(
            FVector2D(Payload.GridCoordinate.X, Payload.GridCoordinate.Y), SnapStrength);
        const FCardInstance* SelectedCard =
            World->GetPersistentCampaign().CollectionState.FindInstance(Payload.CardInstanceId);
        const FName SelectedDefinitionId = SelectedCard == nullptr
            ? NAME_None : SelectedCard->DefinitionId;
        FGuid NewWorldAssetId;
        if (!World->TryPlacePlayerWorldAsset(Payload.CardInstanceId, Payload.CityId,
            GridOrigin,
            FIntPoint(Payload.GridCoordinate.FootprintX, Payload.GridCoordinate.FootprintY),
            static_cast<EGridRotation>(Payload.GridCoordinate.Rotation), NewWorldAssetId, OutError))
        {
            return false;
        }
        if (SelectedDefinitionId == TEXT("synara.adaptive_habitat"))
        {
            if (UDAFirstHourCampaignCoordinatorSubsystem* FirstHour =
                GameInstance->GetSubsystem<UDAFirstHourCampaignCoordinatorSubsystem>())
            {
                // The placement transaction is already canonical. Submit the authored player
                // action through the coordinator so quest progression observes that same owner.
                FirstHour->SubmitPlayerAction(TEXT("quest.a_place_to_stay"),
                    EDAFirstHourPlayerAction::PlaceAdaptiveHabitat, NAME_None,
                    FGuid(), FGuid());
            }
        }
        OutError.Reset();
        return true;
    }
    if (Command == TEXT("command.city.cancel_build"))
    {
        return World->TryCancelPlayerWorldAsset(Payload.WorldAssetId, OutError);
    }
    if (Command == TEXT("command.founder.activate_ability"))
    {
        APlayerController* Controller = GameInstance->GetFirstLocalPlayerController();
        APawn* Pawn = Controller == nullptr ? nullptr : Controller->GetPawn();
        UAbilitySystemComponent* ASC = Pawn == nullptr ? nullptr : Pawn->FindComponentByClass<UAbilitySystemComponent>();
        if (Controller != nullptr && Pawn != nullptr)
            if (const UDAAccessibilitySettingsSubsystem* Accessibility =
                GameInstance->GetSubsystem<UDAAccessibilitySettingsSubsystem>())
            {
                const FVector Assisted = FMath::Lerp(Controller->GetControlRotation().Vector(),
                    Pawn->GetActorForwardVector(), Accessibility->GetSettings().AimAssist).GetSafeNormal();
                if (!Assisted.IsNearlyZero()) Controller->SetControlRotation(Assisted.Rotation());
            }
        const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(Payload.AbilityId, false);
        FGameplayTagContainer Tags;
        if (Tag.IsValid()) Tags.AddTag(Tag);
        if (ASC == nullptr || !Tag.IsValid() || !ASC->TryActivateAbilitiesByTag(Tags))
        { OutError = TEXT("Gameplay Ability System rejected the requested founder ability."); return false; }
        OutError.Reset(); return true;
    }
    if (Command == TEXT("command.command.select_squad") || Command == TEXT("command.command.issue_order"))
    {
        UDACommandSubsystem* Commands = GameInstance->GetSubsystem<UDACommandSubsystem>();
        const TObjectPtr<UDASquadEntity>* FoundSquad = Commands == nullptr ? nullptr
            : Commands->GetRegisteredSquads().FindByPredicate(
                [&Payload](const TObjectPtr<UDASquadEntity>& Row)
                { return Row != nullptr && Row->GetFName() == Payload.SquadId; });
        UDASquadEntity* Squad = FoundSquad == nullptr ? nullptr : FoundSquad->Get();
        if (Commands == nullptr || Squad == nullptr)
        { OutError = TEXT("Authoritative squad target is not registered."); return false; }
        if (Command == TEXT("command.command.select_squad"))
        {
            if (Squad->IsDirectlyControlled())
            { OutError.Reset(); return true; }
            return Commands->TrySelectDirectly(Squad) ? (OutError.Reset(), true)
                : (OutError = TEXT("Command capacity rejected squad selection."), false);
        }
        bool bValidOrder = false; const EDACommandOrder Order = OrderFromName(Payload.Order, bValidOrder);
        if (!bValidOrder || !Payload.OrderTarget.TargetActorId.IsNone())
        {
            TArray<AActor*> Actors;
            UGameplayStatics::GetAllActorsWithTag(GetWorld(), Payload.OrderTarget.TargetActorId, Actors);
            Squad->SetTarget(Actors.IsEmpty() ? nullptr : Actors[0]);
            if (!Payload.OrderTarget.TargetActorId.IsNone() && Actors.IsEmpty())
            { OutError = TEXT("Squad order target actor does not resolve."); return false; }
        }
        return Commands->IssueOrder(Squad, Order, Payload.OrderTarget.Destination)
            ? (OutError.Reset(), true) : (OutError = TEXT("Command service rejected squad order."), false);
    }
    if (Command == TEXT("command.crafting.craft"))
    {
        TArray<FGuid> CraftedInstanceIds;
        return World->CraftUnlockedBlueprint(Payload.BlueprintId, Payload.Quantity,
            CraftedInstanceIds, OutError);
    }
    if (Command.StartsWith(TEXT("command.deck.")))
    {
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        const FGuid Outgoing = Command == TEXT("command.deck.add_card")
            ? Payload.ReplacementCardInstanceId : Payload.CardInstanceId;
        const FGuid Incoming = Command == TEXT("command.deck.add_card")
            ? Payload.CardInstanceId : Payload.ReplacementCardInstanceId;
        if (Command == TEXT("command.deck.add_card"))
        {
            if (!Candidate.DeckState.TrySwapInstance(Outgoing, Incoming, OutError)) return false;
        }
        else if (Command == TEXT("command.deck.remove_card"))
        {
            if (!Candidate.DeckState.TrySwapInstance(Outgoing, Incoming, OutError)) return false;
        }
        else
        {
            OutError = TEXT("ServiceUnavailable: no authoritative deck-draft service is registered; "
                "the endpoint will not report a no-op save.");
            return false;
        }
        return CommitCampaign(*World, Candidate, OutError);
    }
    if (Command == TEXT("command.citizen.assign_job"))
    {
        FDACampaignJobAssignmentSignal Assignment;
        Assignment.CitizenId = Payload.CitizenId;
        Assignment.JobId = Payload.JobId;
        Assignment.FacilityWorldAssetId = Payload.FacilityWorldAssetId;
        if (!World->SubmitJobAssignmentSignal(Assignment))
        {
            OutError = TEXT("Canonical city authority rejected the job assignment.");
            return false;
        }
        OutError.Reset();
        return true;
    }
    if (Command == TEXT("command.treaty.commit"))
    {
        FDACampaignSnapshot Candidate = World->GetPersistentCampaign();
        UDADiplomacySystem* Diplomacy = NewObject<UDADiplomacySystem>(this);
        const FString TreatyAction = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        for (const FDAUITreatyTermPayload& Term : Payload.TreatyTerms)
        {
            bool bMetricValid = false; const EDADiplomaticMetric Metric = MetricFromName(Term.Metric, bMetricValid);
            if (!bMetricValid || !Diplomacy->ApplyReason(Candidate, Payload.RelationshipId, Metric,
                Term.TermId, Term.Magnitude, Candidate.WorldState.CurrentWorldTick,
                FName(*(TEXT("ui.treaty.") + TreatyAction + TEXT(".") + Term.TermId.ToString()))))
            { OutError = TEXT("Diplomacy service rejected a treaty term."); return false; }
        }
        return CommitCampaign(*World, Candidate, OutError);
    }
    if (Command == TEXT("command.world.travel"))
    {
        const FDATravelHandoffState& ExistingHandoff = World->GetPersistentState().TravelHandoff;
        const FName RequestId = ExistingHandoff.Stage != EDATravelHandoffStage::None
            && ExistingHandoff.Stage != EDATravelHandoffStage::Completed
            && ExistingHandoff.DestinationRegionId == Payload.RegionId
            ? ExistingHandoff.RequestId
            : FName(*FString::Printf(TEXT("ui.travel.%s.%lld"), *Payload.RegionId.ToString(),
                static_cast<long long>(World->GetCurrentWorldTick())));
        const EDATravelResult Result = World->TravelUsingRegisteredRuntime(
            RequestId, Payload.RegionId, 1, OutError);
        if (Result == EDATravelResult::Completed || Result == EDATravelResult::AlreadyCompleted)
        {
            if (Payload.RegionId == TEXT("region.eden_basin"))
            {
                if (UDAFirstHourCampaignCoordinatorSubsystem* FirstHour =
                    GameInstance->GetSubsystem<UDAFirstHourCampaignCoordinatorSubsystem>())
                {
                    FirstHour->SubmitPlayerAction(TEXT("quest.basin_speaks"),
                        EDAFirstHourPlayerAction::ReachEdenBasin, NAME_None,
                        FGuid(), FGuid());
                }
            }
            OutError.Reset();
            return true;
        }
        if (OutError.IsEmpty()) OutError = TEXT("World travel authority rejected the destination or runtime handoff.");
        return false;
    }
    if (Command == TEXT("command.save.write") || Command == TEXT("command.save.load"))
    {
        FDASaveService Saves;
        if (Command == TEXT("command.save.write"))
        {
            const FDASaveResult Result = Saves.SaveCampaign(World->GetPersistentCampaign(), Payload.SaveSlotId);
            if (Result.IsSuccess()) { OutError.Reset(); return true; }
            OutError = Result.Error.Message; return false;
        }
        const TResult<FDACampaignSnapshot, FDASaveError> Result = Saves.LoadCampaign(Payload.SaveSlotId);
        if (Result.HasValue() && World->RestorePersistentCampaign(Result.GetValue()))
        { OutError.Reset(); return true; }
        OutError = Result.HasValue() ? TEXT("Loaded campaign could not be restored.") : Result.GetError().Message;
        return false;
    }
    if (Command == TEXT("command.research.start")
        || Command == TEXT("command.conquest.choose_route")
        || Command == TEXT("command.leader.confirm_resolution")
        || Command == TEXT("command.ascension.claim")
        || Command == TEXT("command.history.inspect")
        || Command == TEXT("command.metrics.inspect")
        || Command == TEXT("command.founder.interact"))
    {
        UDAUIAuthoritativeFeatureRegistrySubsystem* Features =
            GameInstance->GetSubsystem<UDAUIAuthoritativeFeatureRegistrySubsystem>();
        if (Features == nullptr)
        { OutError = TEXT("ServiceUnavailable: feature authority registry is unavailable."); return false; }
        return Features->ExecuteRegisteredCommand(Request.CommandId, Request.SourceScreenId,
            Request.PayloadJson, OutError);
    }
    OutError = TEXT("Production endpoint has no authoritative route for the frozen command ID.");
    return false;
}
