#include "Presentation/DAPresentationContent.h"

#include "Capture/DACaptureComponent.h"
#include "Conflict/DAConflictRecords.h"

bool FDAPresentationRuntimeProjection::BuildConstruction(
    const FDAPresentationContentManifest& Manifest, const FName Faction,
    const EDAConstructionState State, FDAPresentationPlaybackPlan& OutPlan)
{
    OutPlan = FDAPresentationPlaybackPlan();
    const FDAConstructionPresentationGrammar* Grammar =
        Manifest.FindConstructionGrammar(Faction);
    const FDAConstructionPresentationStage* Stage = Grammar == nullptr ? nullptr
        : Grammar->Stages.FindByPredicate([State](const FDAConstructionPresentationStage& Row)
        { return Row.State == State; });
    if (Grammar == nullptr || Stage == nullptr) return false;
    OutPlan.GeometryHook = Stage->GeometryHook;
    OutPlan.VfxId = Stage->VfxId;
    OutPlan.SfxId = Stage->SfxId;
    OutPlan.OrderedStages = {Stage->GeometryHook};
    return true;
}

bool FDAPresentationRuntimeProjection::BuildDamage(
    const FDAPresentationContentManifest& Manifest, const FName Faction,
    const FDAWorldAssetRecord& Asset, FDAPresentationPlaybackPlan& OutPlan)
{
    OutPlan = FDAPresentationPlaybackPlan();
    const FDADamagePresentationLanguage* Language = Manifest.FindDamageLanguage(Faction);
    if (Language == nullptr
        || (Asset.ConstructionState != EDAConstructionState::Damaged
            && Asset.ConstructionState != EDAConstructionState::Disabled
            && Asset.ConstructionState != EDAConstructionState::Ruined)) return false;
    OutPlan.MaterialHook = Language->MaterialHook;
    OutPlan.VfxId = Language->VfxId;
    OutPlan.SfxId = Language->SfxId;
    OutPlan.GeometryHook = Asset.ConstructionState == EDAConstructionState::Ruined
        ? FName(TEXT("authored_ruin_transition")) : FName(TEXT("module_damage_transition"));
    return true;
}

bool FDAPresentationRuntimeProjection::BuildCapture(
    const FDAPresentationContentManifest& Manifest, const FDAWorldAssetRecord& Asset,
    const FDACaptureRecord& Capture, FDAPresentationPlaybackPlan& OutPlan)
{
    OutPlan = FDAPresentationPlaybackPlan();
    if (!Capture.bCaptureCompleted || !Capture.bOutcomeResolved || !Capture.bRewardsGranted
        || Capture.OriginalOwnerCivilizationId.IsNone() || Asset.OwnerCivilizationId.IsNone()
        || Capture.Outcome == EDACaptureOutcome::None
        || Asset.OwnerCivilizationId == Capture.OriginalOwnerCivilizationId
        || Manifest.bAllowsInstantFactionRecolor || Manifest.CaptureStages.Num() != 5)
        return false;
    OutPlan.DisplayOwnerCivilizationId = Capture.OriginalOwnerCivilizationId;
    OutPlan.OriginalOwnerCivilizationId = Capture.OriginalOwnerCivilizationId;
    OutPlan.TargetOwnerCivilizationId = Asset.OwnerCivilizationId;
    OutPlan.OrderedStages = Manifest.CaptureStages;
    for (int32 Index = 0; Index < Manifest.CaptureStages.Num(); ++Index)
    {
        FDACaptureOwnershipPresentationStage& Stage = OutPlan.OwnershipStages.Emplace_GetRef();
        Stage.Stage = Manifest.CaptureStages[Index];
        Stage.OriginalOwnerCivilizationId = Capture.OriginalOwnerCivilizationId;
        Stage.TargetOwnerCivilizationId = Asset.OwnerCivilizationId;
        Stage.DisplayedOwnerCivilizationId = Index < 3
            ? Capture.OriginalOwnerCivilizationId : Asset.OwnerCivilizationId;
        if (Stage.Stage == TEXT("install_new_signage"))
            Stage.SignageOwnerCivilizationId = Asset.OwnerCivilizationId;
    }
    OutPlan.GeometryHook = Manifest.CaptureStages[0];
    OutPlan.VfxId = TEXT("vfx.universal.capture");
    OutPlan.SfxId = Capture.bIntegrationRequired
        ? FName(TEXT("sfx.capture.integration")) : FName(TEXT("sfx.capture.complete"));
    OutPlan.bAllowsInstantFactionRecolor = false;
    return true;
}

bool FDAPresentationRuntimeProjection::BuildCapture(
    const FDAPresentationContentManifest& Manifest,
    const FDACapturePresentationSnapshot& Snapshot,
    FDAPresentationPlaybackPlan& OutPlan)
{
    FDAWorldAssetRecord Asset;
    Asset.OwnerCivilizationId = Snapshot.CurrentOwnerCivilizationId;
    FDACaptureRecord Capture;
    Capture.OriginalOwnerCivilizationId = Snapshot.OriginalOwnerCivilizationId;
    Capture.CapturingCivilizationId = Snapshot.CapturingCivilizationId;
    Capture.bCaptureCompleted = Snapshot.bCompleted;
    Capture.bOutcomeResolved = Snapshot.bOutcomeResolved;
    Capture.bRewardsGranted = Snapshot.bRewardsGranted;
    Capture.bIntegrationRequired = Snapshot.bIntegrationRequired;
    Capture.Outcome = Snapshot.Outcome;
    return BuildCapture(Manifest, Asset, Capture, OutPlan);
}

bool FDAPresentationRuntimeProjection::BuildDaxton(
    const FDAPresentationContentManifest& Manifest, const FDADaxtonCampaignState& Daxton,
    FDAPresentationPlaybackPlan& OutPlan)
{
    OutPlan = FDAPresentationPlaybackPlan();
    FName Phase;
    switch (Daxton.Phase)
    {
    case EDADaxtonEncounterPhase::PhaseOne: Phase = TEXT("PhaseOne"); break;
    case EDADaxtonEncounterPhase::PhaseTwo: Phase = TEXT("PhaseTwo"); break;
    case EDADaxtonEncounterPhase::PhaseThree: Phase = TEXT("PhaseThree"); break;
    case EDADaxtonEncounterPhase::Resolved: Phase = TEXT("Resolved"); break;
    default: return false;
    }
    const FDAPresentationStateBinding* Binding = Manifest.DaxtonStates.FindByPredicate(
        [Phase](const FDAPresentationStateBinding& Row) { return Row.State == Phase; });
    if (Binding == nullptr) return false;
    OutPlan.GeometryHook = Binding->GeometryHook;
    OutPlan.VfxId = Binding->VfxId;
    OutPlan.SfxId = Binding->SfxId;
    return true;
}

bool FDAPresentationRuntimeProjection::BuildAscension(
    const FDAPresentationContentManifest& Manifest,
    const FDAAscensionPresentationState& Ascension, FDAPresentationPlaybackPlan& OutPlan)
{
    OutPlan = FDAPresentationPlaybackPlan();
    const TArray<EDAAscensionPresentationBeat> Expected = {
        EDAAscensionPresentationBeat::SystemsHaltAndReact,
        EDAAscensionPresentationBeat::ForgeRelicEmerges,
        EDAAscensionPresentationBeat::WorldTransit,
        EDAAscensionPresentationBeat::FounderHallReceivesRelic,
        EDAAscensionPresentationBeat::Unlocks};
    if (!Ascension.bAscended || !Ascension.bShouldPlayCinematic
        || !Ascension.bCinematicMayBeSkipped || Manifest.bAscensionGameplayGate
        || !Manifest.bAscensionCinematicMayBeSkipped
        || Ascension.CinematicSequenceAsset != Manifest.AscensionCinematicAsset
        || Ascension.OrderedBeats != Expected
        || Manifest.AscensionBeats.Num() != Expected.Num()) return false;
    OutPlan.GeometryHook = TEXT("forgeweave_authority_transfer");
    OutPlan.VfxId = Manifest.AscensionBeats[0].VfxId;
    OutPlan.SfxId = Manifest.AscensionBeats.Last().SfxId;
    for (const FDAPresentationStateBinding& Beat : Manifest.AscensionBeats)
        OutPlan.OrderedStages.Add(Beat.State);
    OutPlan.bMaySkip = true;
    OutPlan.bNonAuthoritative = true;
    return true;
}

void FDAPresentationRuntimeProjection::Skip(FDAPresentationPlaybackPlan& InOutPlan)
{
    if (InOutPlan.bNonAuthoritative && InOutPlan.bMaySkip)
        InOutPlan.bSkipped = true;
}
