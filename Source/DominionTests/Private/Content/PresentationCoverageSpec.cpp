#include "Misc/AutomationTest.h"

#include "Ascension/DAAscensionSystem.h"
#include "Cards/DACardInstance.h"
#include "Campaign/DADaxtonCampaignState.h"
#include "Capture/DACaptureComponent.h"
#include "Construction/DAConstructionComponent.h"
#include "Conflict/DAConflictRecords.h"
#include "Content/DACardDefinition.h"
#include "Content/DAContentManifest.h"
#include "Damage/DAStructuralDamageComponent.h"
#include "Misc/PackageName.h"
#include "Presentation/DAPresentationContent.h"
#include "Presentation/DAPresentationEventAdapterComponent.h"
#include "Presentation/DAPresentationBindingWorldSubsystem.h"
#include "Presentation/DAPresentationRegistrySubsystem.h"
#include "Save/DACampaignSaveGame.h"
#include "World/DAWorldAssetRecord.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
    struct FDAPresentationWorldFixture
    {
        UWorld* World = nullptr;
        UGameInstance* GameInstance = nullptr;

        FDAPresentationWorldFixture()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None,
                GetTransientPackage());
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            GameInstance = NewObject<UGameInstance>(GetTransientPackage());
            GameInstance->AddToRoot();
            GameInstance->Init();
            World->SetGameInstance(GameInstance);
            World->InitializeNewWorld(UWorld::InitializationValues()
                .AllowAudioPlayback(false)
                .CreatePhysicsScene(false)
                .CreateNavigation(false)
                .CreateAISystem(false)
                .ShouldSimulatePhysics(false)
                .SetTransactional(false));
        }

        ~FDAPresentationWorldFixture()
        {
            if (World != nullptr)
            {
                World->DestroyWorld(false);
                if (GEngine != nullptr) GEngine->DestroyWorldContext(World);
            }
            if (GameInstance != nullptr)
            {
                GameInstance->Shutdown();
                GameInstance->RemoveFromRoot();
            }
        }

        template <typename TComponent>
        TComponent* AddComponent(AActor& Owner)
        {
            TComponent* Component = NewObject<TComponent>(&Owner);
            Owner.AddInstanceComponent(Component);
            Component->RegisterComponent();
            return Component;
        }
    };
}

BEGIN_DEFINE_SPEC(FDAPresentationCoverageSpec, "Dominion.Content.PresentationCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAPresentationCoverageSpec)

void FDAPresentationCoverageSpec::Define()
{
    Describe("canonical coverage", [this]()
    {
        It("resolves the exact frozen primary VFX music and ambient scope plus at least sixty distinct SFX", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses", FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            TestEqual("Primary assets", Manifest.PrimaryAssets.Num(), 50);
            TestEqual("Core VFX", Manifest.CoreVfx.Num(), 25);
            TestEqual("Music", Manifest.Music.Num(), 9);
            TestEqual("Ambient", Manifest.Ambient.Num(), 12);
            TestTrue("At least sixty distinct authored SFX", Manifest.Sfx.Num() >= 60);
            for (const FDAPresentationDefinitionSource& Source : Manifest.AllDefinitions())
            {
                FDAPresentationDefinitionSource Resolved;
                TestTrue(*FString::Printf(TEXT("%s resolves"), *Source.Id.ToString()),
                    FDAPresentationContentPipeline::Resolve(Manifest, Source.Kind, Source.Id, Resolved));
                TestTrue("Generated package is explicit", Resolved.AssetPath.StartsWith(TEXT("/Game/")));
                TestTrue("Definition binds loadable artifact paths", !Resolved.Artifacts.IsEmpty());
                for (const FDAPresentationArtifactBinding& Binding : Resolved.Artifacts)
                {
                    TestTrue("Artifact object path is explicit", Binding.Asset.IsValid());
                    TestTrue("Artifact class is explicit", !Binding.AssetClass.IsEmpty());
                }
                TestFalse("Canonical source is not generated cache", Resolved.bGeneratedCache);
            }
        });

        It("rejects partial generated coverage instead of treating canonical fallback as Asset Registry proof", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses", FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            TArray<UDAPresentationDefinition*> Generated;
            TestTrue("Transient canonical fallback builds", FDAPresentationContentPipeline::BuildRuntimeFallback(Manifest, Generated, Errors));
            TestEqual("Complete fallback row count is derived", Generated.Num(),
                Manifest.AllDefinitions().Num());
            bool bUsedGeneratedCache = true;
            Generated.Pop();
            TestFalse("Partial generated cache is rejected",
                FDAPresentationContentPipeline::ValidateGeneratedCache(Manifest, Generated, bUsedGeneratedCache, Errors));
            TestFalse("Rejected cache never reports generated provenance", bUsedGeneratedCache);
        });

        It("resolves definition-specific palette and Niagara generation content from checked-in sources", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses",
                FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            const auto* SynaraMaterial = Manifest.ArtifactRequirements.FindByPredicate(
                [](const FDAPresentationContentManifest::FArtifactRequirement& Row)
                {
                    return Row.DefinitionId == TEXT("primary.01.adaptive_habitat")
                        && Row.Role == TEXT("material");
                });
            const auto* ForgeweaveMaterial = Manifest.ArtifactRequirements.FindByPredicate(
                [](const FDAPresentationContentManifest::FArtifactRequirement& Row)
                {
                    return Row.DefinitionId == TEXT("primary.17.worker_arcology")
                        && Row.Role == TEXT("material");
                });
            const auto* NiagaraSystem = Manifest.ArtifactRequirements.FindByPredicate(
                [](const FDAPresentationContentManifest::FArtifactRequirement& Row)
                {
                    return Row.DefinitionId == TEXT("vfx.synara.placement")
                        && Row.Role == TEXT("system");
                });
            TestNotNull("Synara material carries source content", SynaraMaterial);
            TestNotNull("Forgeweave material carries source content", ForgeweaveMaterial);
            TestNotNull("Niagara system carries source content", NiagaraSystem);
            if (SynaraMaterial != nullptr && ForgeweaveMaterial != nullptr)
            {
                TestTrue("Palette payloads are content-addressed",
                    SynaraMaterial->SourceContentFingerprint.Len() == 40
                    && ForgeweaveMaterial->SourceContentFingerprint.Len() == 40);
                TestNotEqual("Faction material generation inputs are distinct",
                    SynaraMaterial->SourceContentPayload,
                    ForgeweaveMaterial->SourceContentPayload);
            }
            if (NiagaraSystem != nullptr)
            {
                TestTrue("Niagara payload is content-addressed",
                    NiagaraSystem->SourceContentFingerprint.Len() == 40);
                TestTrue("Niagara payload selects the real sprite-burst template",
                    NiagaraSystem->SourceContentPayload.Contains(TEXT("NS_SimpleSpriteBurst")));
                TestTrue("Niagara payload carries the definition gameplay radius",
                    NiagaraSystem->SourceContentPayload.Contains(
                        TEXT("\"gameplayRadiusMeters\":8")));
            }
        });

        It("keeps all definitions runtime-resolvable through canonical fallback when generated cache is absent", [this]()
        {
            UDAPresentationRegistrySubsystem* Registry =
                NewObject<UDAPresentationRegistrySubsystem>();
            TArray<FText> Errors;
            TestTrue("Runtime registry rebuilds from packaged source",
                Registry->RebuildFromCanonicalAndGenerated({}, Errors));
            TestTrue("Runtime registry is ready", Registry->IsReady());
            TestFalse("Canonical fallback is not generated-cache evidence",
                Registry->UsedGeneratedCache());
            FDAPresentationDefinitionSource Resolved;
            TestTrue("A production primary asset resolves",
                Registry->Resolve(EDAPresentationDefinitionKind::PrimaryAsset,
                    TEXT("primary.50.autonomous_factory"), Resolved));
            TestEqual("Fallback preserves canonical package intent", Resolved.AssetPath,
                FString(TEXT("/Game/Buildings/Fusion/Primary50/DA_Presentation_AutonomousFactory")));
            TestFalse("Resolved fallback row never claims generated provenance",
                Resolved.bGeneratedCache);
        });

        It("surfaces rejected generated-cache diagnostics while remaining ready on canonical fallback", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses",
                FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            TArray<UDAPresentationDefinition*> Partial;
            TestTrue("Transient canonical rows build",
                FDAPresentationContentPipeline::BuildRuntimeFallback(Manifest, Partial, Errors));
            Partial.Pop();

            UDAPresentationRegistrySubsystem* Registry = NewObject<UDAPresentationRegistrySubsystem>();
            TArray<FText> Diagnostics;
            TestTrue("Registry degrades to canonical fallback",
                Registry->RebuildFromCanonicalAndGenerated(Partial, Diagnostics));
            TestFalse("Partial cache never becomes registry evidence", Registry->UsedGeneratedCache());
            TestTrue("Cache rejection is returned to the caller", !Diagnostics.IsEmpty());
            TestTrue("Registry retains a degraded-cache diagnostic",
                Registry->HasDegradedCacheWarning());
        });

        It("derives fallback and cache totals for compliant SFX coverage above sixty", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses",
                FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            FDAPresentationDefinitionSource Extra = Manifest.Sfx.Last();
            Extra.Id = TEXT("sfx.validation.sixty_one");
            Extra.AssetPath = TEXT("/Game/Audio/SFX/Validation/DA_Presentation_SixtyOne");
            for (FDAPresentationArtifactBinding& Binding : Extra.Artifacts)
            {
                const FString PackagePath = Extra.AssetPath
                    + (Binding.Role == TEXT("wave") ? TEXT("_Wave") : TEXT("_Cue"));
                Binding.Asset = FSoftObjectPath(PackagePath + TEXT(".")
                    + FPackageName::GetLongPackageAssetName(PackagePath));
            }
            Manifest.Sfx.Add(Extra);
            const TArray<FDAPresentationContentManifest::FArtifactRequirement> CanonicalArtifacts =
                Manifest.ArtifactRequirements;
            for (const FDAPresentationContentManifest::FArtifactRequirement& Existing
                : CanonicalArtifacts)
            {
                if (Existing.DefinitionId == Manifest.Sfx[Manifest.Sfx.Num() - 2].Id)
                {
                    FDAPresentationContentManifest::FArtifactRequirement Copy = Existing;
                    Copy.DefinitionId = Extra.Id;
                    Copy.AssetPath = Extra.AssetPath
                        + (Copy.Role == TEXT("wave") ? TEXT("_Wave") : TEXT("_Cue"));
                    Manifest.ArtifactRequirements.Add(Copy);
                }
            }
            TestTrue("Sixty-one unique SFX remain valid",
                FDAPresentationContentPipeline::Validate(Manifest, Errors));
            TArray<UDAPresentationDefinition*> Fallback;
            TestTrue("Dynamic fallback builds",
                FDAPresentationContentPipeline::BuildRuntimeFallback(
                    Manifest, Fallback, Errors));
            TestEqual("Dynamic fallback contains 157 definitions", Fallback.Num(), 157);
        });
    });

    Describe("authoritative event projections", [this]()
    {
        It("projects five behaviorally distinct construction stages for each frozen civilization", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses", FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            const TArray<FName> Factions = {TEXT("Synara"), TEXT("Forgeweave"), TEXT("EdenCircuit")};
            TSet<FName> GrammarIds;
            TSet<FName> FrameGeometryHooks;
            for (const FName Faction : Factions)
            {
                const FDAConstructionPresentationGrammar* Grammar = Manifest.FindConstructionGrammar(Faction);
                TestNotNull(*Faction.ToString(), Grammar);
                if (Grammar == nullptr) continue;
                TestEqual("Five authored stages", Grammar->Stages.Num(), 5);
                GrammarIds.Add(Grammar->GrammarId);
                FrameGeometryHooks.Add(Grammar->Stages[1].GeometryHook);
                for (const FDAConstructionPresentationStage& Stage : Grammar->Stages)
                {
                    FDAPresentationPlaybackPlan Plan;
                    TestTrue("Construction state resolves through real OnStageChanged values",
                        FDAPresentationRuntimeProjection::BuildConstruction(Manifest, Faction, Stage.State, Plan));
                    TestEqual("Projection preserves grammar action", Plan.GeometryHook, Stage.GeometryHook);
                }
            }
            TestEqual("Three distinct grammars", GrammarIds.Num(), 3);
            TestEqual("Three distinct frame behaviors", FrameGeometryHooks.Num(), 3);
        });

        It("derives faction damage and staged capture signage from committed world and capture records", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses", FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            FDAWorldAssetRecord Asset;
            Asset.OwnerCivilizationId = TEXT("Synara");
            Asset.ConstructionState = EDAConstructionState::Damaged;
            FDAPresentationPlaybackPlan Damage;
            TestTrue("Committed damage state resolves", FDAPresentationRuntimeProjection::BuildDamage(
                Manifest, TEXT("Synara"), Asset, Damage));
            TestEqual("Synara damage is material-specific", Damage.MaterialHook,
                FName(TEXT("fractured_ceramic_exposed_lattice")));

            FDACaptureRecord Capture;
            Capture.OriginalOwnerCivilizationId = TEXT("Forgeweave");
            Capture.CapturingCivilizationId = TEXT("Synara");
            Capture.Outcome = EDACaptureOutcome::Preserve;
            Capture.bCaptureCompleted = true;
            Capture.bOutcomeResolved = true;
            Capture.bRewardsGranted = true;
            Capture.bIntegrationRequired = true;
            Asset.OwnerCivilizationId = TEXT("Synara");
            FDAPresentationPlaybackPlan Ownership;
            TestTrue("Committed capture resolves", FDAPresentationRuntimeProjection::BuildCapture(
                Manifest, Asset, Capture, Ownership));
            TestEqual("Presentation begins with original owner", Ownership.DisplayOwnerCivilizationId,
                FName(TEXT("Forgeweave")));
            TestEqual("Plan retains original owner", Ownership.OriginalOwnerCivilizationId,
                FName(TEXT("Forgeweave")));
            TestEqual("Plan retains committed target owner", Ownership.TargetOwnerCivilizationId,
                FName(TEXT("Synara")));
            TestEqual("Five integration/signage stages", Ownership.OrderedStages.Num(), 5);
            TestEqual("Five ownership payload stages", Ownership.OwnershipStages.Num(), 5);
            if (Ownership.OrderedStages.Num() >= 4 && Ownership.OwnershipStages.Num() >= 5)
            {
                TestEqual("New signage is installed before integrated operation", Ownership.OrderedStages[3],
                    FName(TEXT("install_new_signage")));
                TestEqual("Install stage carries the new signage owner",
                    Ownership.OwnershipStages[3].SignageOwnerCivilizationId,
                    FName(TEXT("Synara")));
                TestEqual("Original architecture retains old displayed owner",
                    Ownership.OwnershipStages[0].DisplayedOwnerCivilizationId,
                    FName(TEXT("Forgeweave")));
                TestEqual("Integrated operation displays committed owner",
                    Ownership.OwnershipStages[4].DisplayedOwnerCivilizationId,
                    FName(TEXT("Synara")));
            }
            TestFalse("Instant recolor is prohibited", Ownership.bAllowsInstantFactionRecolor);
        });

        It("binds real construction damage and capture delegate broadcasts to presentation plans", [this]()
        {
            UDAPresentationRegistrySubsystem* Registry = NewObject<UDAPresentationRegistrySubsystem>();
            TArray<FText> Errors;
            TestTrue("Presentation registry is ready",
                Registry->RebuildFromCanonicalAndGenerated({}, Errors));
            UDAPresentationEventAdapterComponent* Adapter =
                NewObject<UDAPresentationEventAdapterComponent>();
            TestTrue("Adapter accepts the ready read-only registry", Adapter->Initialize(Registry));

            FDAVerticalSliceContentManifest ContentManifest;
            FDABuiltManifestContent Built;
            TestTrue("Gameplay content parses",
                FDAContentManifestPipeline::LoadCanonical(ContentManifest, Errors));
            TestTrue("Gameplay content builds",
                FDAContentManifestPipeline::BuildRuntimeContent(ContentManifest, Built, Errors));
            UDA_CardDefinition* Definition =
                Built.FindDefinition(TEXT("synara.guardian_drone_cohort"));
            TestNotNull("Construction fixture definition exists", Definition);
            if (Definition == nullptr) return;
            FCardInstance Card;
            Card.InstanceId = FGuid(1, 2, 3, 4);
            Card.DefinitionId = Definition->DefinitionId;
            FDAWorldAssetRecord ConstructionRecord;
            ConstructionRecord.WorldAssetId = FGuid(5, 6, 7, 8);
            ConstructionRecord.CardInstanceId = Card.InstanceId;
            ConstructionRecord.CardDefinitionId = Card.DefinitionId;
            ConstructionRecord.OwnerCivilizationId = TEXT("Synara");
            ConstructionRecord.ConstructionState = EDAConstructionState::Foundation;
            ConstructionRecord.StructuralIntegrity = 100.f;
            Card.WorldAssetId = ConstructionRecord.WorldAssetId;
            UDAConstructionComponent* Construction = NewObject<UDAConstructionComponent>();
            TestTrue("Construction record binds",
                Construction->InitializeFromRecord(ConstructionRecord, &Card, *Definition));
            TestTrue("Adapter binds OnStageChanged",
                Adapter->BindConstruction(Construction, TEXT("Synara")));
            TestTrue("Authoritative construction mutation broadcasts", Construction->AdvanceCycle());
            TestEqual("Delegate broadcast produces the committed construction plan",
                Adapter->GetLastConstructionPlan().GeometryHook,
                FName(TEXT("smart_panels_unfold")));

            UDACampaignSaveGame* DamageCampaign = NewObject<UDACampaignSaveGame>();
            FDAWorldAssetRecord DamageAsset;
            DamageAsset.WorldAssetId = FGuid(9, 10, 11, 12);
            DamageAsset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
            DamageAsset.OwnerCivilizationId = TEXT("Synara");
            DamageAsset.ConstructionState = EDAConstructionState::Operational;
            DamageAsset.StructuralIntegrity = 100.f;
            DamageCampaign->Snapshot.WorldAssets.Add(DamageAsset);
            FDAStructuralDamageRecord DamageRecord;
            DamageRecord.WorldAssetId = DamageAsset.WorldAssetId;
            DamageRecord.CardDefinitionId = DamageAsset.CardDefinitionId;
            DamageRecord.Modules = {
                FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true)};
            DamageCampaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(DamageRecord);
            UDAStructuralDamageComponent* Damage = NewObject<UDAStructuralDamageComponent>();
            TestTrue("Damage authority binds",
                Damage->InitializeFromCampaign(*DamageCampaign, DamageAsset.WorldAssetId));
            TestTrue("Adapter binds OnDamageStateChanged",
                Adapter->BindDamage(Damage, TEXT("Synara")));
            TestTrue("Authoritative structural mutation broadcasts", Damage->ApplyStructuralDamage(50.f));
            TestEqual("Delegate broadcast produces faction damage language",
                Adapter->GetLastDamagePlan().MaterialHook,
                FName(TEXT("fractured_ceramic_exposed_lattice")));

            UDACampaignSaveGame* CaptureCampaign = NewObject<UDACampaignSaveGame>();
            FDAWorldAssetRecord CaptureAsset;
            CaptureAsset.WorldAssetId = FGuid(13, 14, 15, 16);
            CaptureAsset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
            CaptureAsset.OwnerCivilizationId = TEXT("Forgeweave");
            CaptureAsset.ConstructionState = EDAConstructionState::Disabled;
            CaptureAsset.StructuralIntegrity = 10.f;
            CaptureCampaign->Snapshot.WorldAssets.Add(CaptureAsset);
            FDACaptureRecord CaptureRecord;
            CaptureRecord.WorldAssetId = CaptureAsset.WorldAssetId;
            CaptureCampaign->Snapshot.OperationConflict.CaptureRecords.Add(CaptureRecord);
            UDACaptureComponent* Capture = NewObject<UDACaptureComponent>();
            TestTrue("Capture authority binds",
                Capture->InitializeFromCampaign(*CaptureCampaign, CaptureAsset.WorldAssetId));
            TestTrue("Adapter binds OnCaptureStateChanged", Adapter->BindCapture(Capture));
            FDACaptureInteractionContext Context;
            Context.InteractionId = FGuid(17, 18, 19, 20);
            Context.CaptureActorId = FGuid(21, 22, 23, 24);
            Context.AgentRole = EDACaptureAgentRole::Founder;
            Context.bActorPresent = true;
            TestTrue("Capture begins", Capture->BeginCapture(Context, TEXT("Synara")));
            TestTrue("Capture commits", Capture->AdvanceCapture(
                UDACaptureComponent::BaseCaptureTimeSeconds, Context));
            TestTrue("Preserve outcome commits",
                Capture->ResolveOutcome(EDACaptureOutcome::Preserve, NAME_None));
            TestEqual("Capture delegate carries original owner",
                Adapter->GetLastCapturePlan().OriginalOwnerCivilizationId,
                FName(TEXT("Forgeweave")));
            TestEqual("Capture delegate carries committed target owner",
                Adapter->GetLastCapturePlan().TargetOwnerCivilizationId,
                FName(TEXT("Synara")));
        });

        It("automatically bootstraps existing and future authoritative components and prunes destroyed bindings", [this]()
        {
            FDAPresentationWorldFixture Fixture;
            UDAPresentationBindingWorldSubsystem* Bindings =
                Fixture.World->GetSubsystem<UDAPresentationBindingWorldSubsystem>();
            TestNotNull("Shipped world lifecycle creates the presentation binding subsystem", Bindings);
            if (Bindings == nullptr) return;

            int32 ConstructionPlans = 0;
            int32 DamagePlans = 0;
            int32 CapturePlans = 0;
            Bindings->OnPresentationPlanReadyNative.AddLambda(
                [&ConstructionPlans, &DamagePlans, &CapturePlans](
                    const FName Channel, const FDAPresentationPlaybackPlan&)
                {
                    if (Channel == TEXT("construction")) ++ConstructionPlans;
                    else if (Channel == TEXT("damage")) ++DamagePlans;
                    else if (Channel == TEXT("capture")) ++CapturePlans;
                });

            TArray<FText> Errors;
            FDAVerticalSliceContentManifest ContentManifest;
            FDABuiltManifestContent Built;
            TestTrue("Gameplay content parses for production bootstrap",
                FDAContentManifestPipeline::LoadCanonical(ContentManifest, Errors));
            TestTrue("Gameplay content builds for production bootstrap",
                FDAContentManifestPipeline::BuildRuntimeContent(ContentManifest, Built, Errors));
            UDA_CardDefinition* Definition =
                Built.FindDefinition(TEXT("synara.guardian_drone_cohort"));
            TestNotNull("Construction fixture definition exists", Definition);
            if (Definition == nullptr) return;

            AActor* ExistingActor = Fixture.World->SpawnActor<AActor>();
            FCardInstance Card;
            Card.InstanceId = FGuid(31, 32, 33, 34);
            Card.DefinitionId = Definition->DefinitionId;
            FDAWorldAssetRecord ConstructionRecord;
            ConstructionRecord.WorldAssetId = FGuid(35, 36, 37, 38);
            ConstructionRecord.CardInstanceId = Card.InstanceId;
            ConstructionRecord.CardDefinitionId = Card.DefinitionId;
            ConstructionRecord.OwnerCivilizationId = TEXT("Synara");
            ConstructionRecord.ConstructionState = EDAConstructionState::Foundation;
            ConstructionRecord.StructuralIntegrity = 100.f;
            Card.WorldAssetId = ConstructionRecord.WorldAssetId;
            UDAConstructionComponent* Construction =
                Fixture.AddComponent<UDAConstructionComponent>(*ExistingActor);
            TestTrue("Existing construction authority initializes",
                Construction->InitializeFromRecord(ConstructionRecord, &Card, *Definition));

            UDACampaignSaveGame* Campaign = NewObject<UDACampaignSaveGame>();
            FDAWorldAssetRecord DamageAsset;
            DamageAsset.WorldAssetId = FGuid(39, 40, 41, 42);
            DamageAsset.CardDefinitionId = TEXT("synara.synthetic_fabrication_node");
            DamageAsset.OwnerCivilizationId = TEXT("Synara");
            DamageAsset.ConstructionState = EDAConstructionState::Operational;
            DamageAsset.StructuralIntegrity = 100.f;
            Campaign->Snapshot.WorldAssets.Add(DamageAsset);
            FDAStructuralDamageRecord DamageRecord;
            DamageRecord.WorldAssetId = DamageAsset.WorldAssetId;
            DamageRecord.CardDefinitionId = DamageAsset.CardDefinitionId;
            DamageRecord.Modules = {
                FDAStructureModuleHealthRecord(TEXT("control_center"), 100.f, true)};
            Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(DamageRecord);
            UDAStructuralDamageComponent* Damage =
                Fixture.AddComponent<UDAStructuralDamageComponent>(*ExistingActor);
            TestTrue("Existing damage authority initializes",
                Damage->InitializeFromCampaign(*Campaign, DamageAsset.WorldAssetId));

            FDAWorldAssetRecord CaptureAsset;
            CaptureAsset.WorldAssetId = FGuid(43, 44, 45, 46);
            CaptureAsset.CardDefinitionId = TEXT("forgeweave.infinite_foundry");
            CaptureAsset.OwnerCivilizationId = TEXT("Forgeweave");
            CaptureAsset.ConstructionState = EDAConstructionState::Disabled;
            CaptureAsset.StructuralIntegrity = 10.f;
            Campaign->Snapshot.WorldAssets.Add(CaptureAsset);
            FDACaptureRecord CaptureRecord;
            CaptureRecord.WorldAssetId = CaptureAsset.WorldAssetId;
            Campaign->Snapshot.OperationConflict.CaptureRecords.Add(CaptureRecord);
            UDACaptureComponent* Capture =
                Fixture.AddComponent<UDACaptureComponent>(*ExistingActor);
            TestTrue("Existing capture authority initializes",
                Capture->InitializeFromCampaign(*Campaign, CaptureAsset.WorldAssetId));

            Fixture.World->BeginPlay();
            TestEqual("World begin play discovers all existing authoritative components",
                Bindings->GetBoundComponentCount(), 3);
            TestTrue("Automatically bound construction broadcast reaches presentation",
                Construction->AdvanceCycle());
            TestTrue("Automatically bound damage broadcast reaches presentation",
                Damage->ApplyStructuralDamage(50.f));
            FDACaptureInteractionContext Context;
            Context.InteractionId = FGuid(47, 48, 49, 50);
            Context.CaptureActorId = FGuid(51, 52, 53, 54);
            Context.AgentRole = EDACaptureAgentRole::Founder;
            Context.bActorPresent = true;
            TestTrue("Automatically bound capture begins", Capture->BeginCapture(Context, TEXT("Synara")));
            TestTrue("Automatically bound capture commits", Capture->AdvanceCapture(
                UDACaptureComponent::BaseCaptureTimeSeconds, Context));
            TestTrue("Automatically bound capture outcome commits",
                Capture->ResolveOutcome(EDACaptureOutcome::Preserve, NAME_None));
            TestTrue("Construction delegate produced a plan", ConstructionPlans > 0);
            TestTrue("Damage delegate produced a plan", DamagePlans > 0);
            TestTrue("Capture delegate produced a plan", CapturePlans > 0);

            AActor* FutureActor = Fixture.World->SpawnActor<AActor>();
            FDAWorldAssetRecord FutureDamageAsset = DamageAsset;
            FutureDamageAsset.WorldAssetId = FGuid(55, 56, 57, 58);
            FutureDamageAsset.StructuralIntegrity = 100.f;
            Campaign->Snapshot.WorldAssets.Add(FutureDamageAsset);
            FDAStructuralDamageRecord FutureDamageRecord = DamageRecord;
            FutureDamageRecord.WorldAssetId = FutureDamageAsset.WorldAssetId;
            Campaign->Snapshot.OperationConflict.StructuralDamageRecords.Add(FutureDamageRecord);
            UDAStructuralDamageComponent* FutureDamage =
                Fixture.AddComponent<UDAStructuralDamageComponent>(*FutureActor);
            TestTrue("Future streamed damage authority initializes",
                FutureDamage->InitializeFromCampaign(*Campaign, FutureDamageAsset.WorldAssetId));
            Fixture.World->Tick(LEVELTICK_All, 0.f);
            TestEqual("World lifecycle discovers a post-bootstrap component",
                Bindings->GetBoundComponentCount(), 4);
            const int32 DamagePlansBeforeFutureBroadcast = DamagePlans;
            TestTrue("Future authoritative damage mutation broadcasts",
                FutureDamage->ApplyStructuralDamage(50.f));
            TestTrue("Future delegate reaches the production presentation bootstrap",
                DamagePlans > DamagePlansBeforeFutureBroadcast);

            Fixture.World->DestroyActor(FutureActor);
            Fixture.World->Tick(LEVELTICK_All, 0.f);
            TestEqual("Destroyed or streamed actors are safely unbound",
                Bindings->GetBoundComponentCount(), 3);
        });

        It("reads Daxton and first Ascension authority while cinematic skip remains presentation-only", [this]()
        {
            FDAPresentationContentManifest Manifest;
            TArray<FText> Errors;
            TestTrue("Canonical presentation source parses", FDAPresentationContentPipeline::LoadCanonical(Manifest, Errors));
            FDADaxtonCampaignState Daxton;
            Daxton.Phase = EDADaxtonEncounterPhase::PhaseTwo;
            Daxton.bPoweredArmorActive = true;
            FDAPresentationPlaybackPlan DaxtonPlan;
            TestTrue("Daxton committed phase resolves", FDAPresentationRuntimeProjection::BuildDaxton(
                Manifest, Daxton, DaxtonPlan));
            TestEqual("Phase II uses the authored Overdrive hook", DaxtonPlan.GeometryHook,
                FName(TEXT("powered_armor_overdrive")));

            FDAAscensionPresentationState Ascension;
            Ascension.bAscended = true;
            Ascension.bShouldPlayCinematic = true;
            Ascension.bCinematicMayBeSkipped = true;
            Ascension.CinematicSequenceAsset = FSoftObjectPath(
                TEXT("/Game/Cinematics/CS_ForgeweaveAscension.CS_ForgeweaveAscension"));
            Ascension.OrderedBeats = {
                EDAAscensionPresentationBeat::SystemsHaltAndReact,
                EDAAscensionPresentationBeat::ForgeRelicEmerges,
                EDAAscensionPresentationBeat::WorldTransit,
                EDAAscensionPresentationBeat::FounderHallReceivesRelic,
                EDAAscensionPresentationBeat::Unlocks};
            FDAPresentationPlaybackPlan AscensionPlan;
            TestTrue("Committed Ascension projection resolves", FDAPresentationRuntimeProjection::BuildAscension(
                Manifest, Ascension, AscensionPlan));
            const bool bAuthoritativeOutcomeBeforeSkip = Ascension.bAscended;
            FDAPresentationRuntimeProjection::Skip(AscensionPlan);
            TestTrue("Skip completes presentation", AscensionPlan.bSkipped);
            TestTrue("Skip cannot alter authoritative outcome", Ascension.bAscended == bAuthoritativeOutcomeBeforeSkip);
            TestTrue("Ascension plan is explicitly non-authoritative", AscensionPlan.bNonAuthoritative);
        });
    });
}
