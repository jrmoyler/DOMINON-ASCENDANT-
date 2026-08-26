#include "DAFirstAscensionContentCommandlet.h"

#include "Ascension/DAAscensionContent.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Content/DACardDefinition.h"
#include "Content/DARegionalCrisisContent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneCinematicShotSection.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "UObject/SavePackage.h"
#include "UObject/MetaData.h"

namespace
{
    struct FExpectedAscensionShot
    {
        FString Beat;
        FString SequenceAssetPath;
        int32 DurationFrames = 0;
        TArray<FString> ActionIds;
        TArray<FString> AudioCueIds;
        TArray<FString> VfxCueIds;
    };

    const TArray<FExpectedAscensionShot>& ExpectedAscensionShots()
    {
        static const TArray<FExpectedAscensionShot> Shots = {
            {TEXT("systems_halt_react"),
             TEXT("/Game/Cinematics/ForgeweaveAscension/Shots/LS_01_SystemsHaltReact"), 90,
             {TEXT("systems.halt"), TEXT("citizens.react")},
             {TEXT("audio.ascension.systems_halt"), TEXT("audio.ascension.crowd_reaction")},
             {TEXT("vfx.ascension.grid_power_falloff"), TEXT("vfx.ascension.forge_glow_first_pulse")}},
            {TEXT("forge_relic_emerges"),
             TEXT("/Game/Cinematics/ForgeweaveAscension/Shots/LS_02_ForgeRelicEmerges"), 120,
             {TEXT("forge.freeze"), TEXT("relic.forge.emerge"), TEXT("relic.forge.lock")},
             {TEXT("audio.ascension.forge_rumble"), TEXT("audio.ascension.relic_reveal")},
             {TEXT("vfx.ascension.forge_embers_suspend"), TEXT("vfx.ascension.forge_relic_materialize")}},
            {TEXT("world_transit"),
             TEXT("/Game/Cinematics/ForgeweaveAscension/Shots/LS_03_WorldTransit"), 90,
             {TEXT("world.transit.begin"), TEXT("world.transit.arrive")},
             {TEXT("audio.ascension.transit_surge"), TEXT("audio.ascension.hall_approach")},
             {TEXT("vfx.ascension.relic_transit_ribbon"), TEXT("vfx.ascension.founder_hall_portal")}},
            {TEXT("founder_hall_receives_relic"),
             TEXT("/Game/Cinematics/ForgeweaveAscension/Shots/LS_04_FounderHallReceivesRelic"), 120,
             {TEXT("founder_hall.receive_relic"), TEXT("founder_hall.open_hidden_chamber")},
             {TEXT("audio.ascension.hall_resonance"), TEXT("audio.ascension.hidden_chamber_unlock")},
             {TEXT("vfx.ascension.relic_plinth_ignite"), TEXT("vfx.ascension.hidden_chamber_reveal")}},
            {TEXT("unlocks"),
             TEXT("/Game/Cinematics/ForgeweaveAscension/Shots/LS_05_Unlocks"), 90,
             {TEXT("unlocks.forgeweave_cards"), TEXT("unlocks.replication_doctrine"),
              TEXT("unlocks.convergence_authority")},
             {TEXT("audio.ascension.unlock_resolution"), TEXT("audio.ascension.convergence_sting")},
             {TEXT("vfx.ascension.forgeweave_cards_unfurl"), TEXT("vfx.ascension.founder_hall_slot_one")}}};
        return Shots;
    }

    const TArray<FString>& ExpectedAscensionBeats()
    {
        static const TArray<FString> Beats = {
            TEXT("systems_halt_react"),
            TEXT("forge_relic_emerges"),
            TEXT("world_transit"),
            TEXT("founder_hall_receives_relic"),
            TEXT("unlocks")};
        return Beats;
    }

    struct FAuthoredAscensionCue
    {
        int32 Frame = 0;
        FString CueId;
    };

    struct FAuthoredAscensionAction
    {
        int32 Frame = 0;
        FString ActionId;
        FString TargetId;
        FString Payload;
    };

    struct FAuthoredAscensionCamera
    {
        FVector StartLocation = FVector::ZeroVector;
        FVector EndLocation = FVector::ZeroVector;
        FRotator StartRotation = FRotator::ZeroRotator;
        FRotator EndRotation = FRotator::ZeroRotator;
        double FieldOfView = 0.0;
    };

    struct FAuthoredAscensionShot
    {
        FString Beat;
        FString SequenceAssetPath;
        int32 DurationFrames = 0;
        FAuthoredAscensionCamera Camera;
        TArray<FAuthoredAscensionAction> Actions;
        TArray<FAuthoredAscensionCue> AudioCues;
        TArray<FAuthoredAscensionCue> VfxCues;
        FString PayloadFingerprint;
        ULevelSequence* Sequence = nullptr;
    };

    struct FAuthoredAscensionShotList
    {
        TArray<FAuthoredAscensionShot> Shots;
        FString Fingerprint;
        int32 DisplayRate = 0;
    };

    FString ObjectPath(const FString& PackagePath)
    {
        return PackagePath + TEXT(".") + FPackageName::GetLongPackageAssetName(PackagePath);
    }

    FString Sha1Utf8(const FString& Value)
    {
        FTCHARToUTF8 Utf8(*Value);
        FSHAHash Hash;
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
        return Hash.ToString();
    }

    bool ExactKeys(const TSharedPtr<FJsonObject>& Object,
        const TArray<FString>& Expected)
    {
        return Object.IsValid() && Object->Values.Num() == Expected.Num()
            && !Expected.ContainsByPredicate([&Object](const FString& Key)
            { return !Object->HasField(Key); });
    }

    bool ReadExactInteger(const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field, int32& OutValue)
    {
        double Value = 0.0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Field, Value)
            || !FMath::IsFinite(Value) || Value != FMath::RoundToDouble(Value)
            || Value < static_cast<double>(MIN_int32)
            || Value > static_cast<double>(MAX_int32)) return false;
        OutValue = static_cast<int32>(Value);
        return true;
    }

    bool SaveAsset(UObject* Source, const FString& PackageName, const FString& SourceFingerprint,
        TArray<FText>& Errors, const FString& AuthoredShotFingerprint = FString(),
        const FString& CinematicBeat = FString(),
        const FString& CinematicPayloadFingerprint = FString(),
        UObject** OutSavedAsset = nullptr)
    {
        UPackage* Package = CreatePackage(*PackageName);
        UObject* Asset = Package == nullptr || Source == nullptr ? nullptr : StaticDuplicateObject(
            Source, Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)));
        if (Asset == nullptr)
        { Errors.Add(FText::FromString(TEXT("Could not create ") + PackageName)); return false; }
        Asset->SetFlags(RF_Public | RF_Standalone);
        if (!SourceFingerprint.IsEmpty())
            Package->GetMetaData()->SetValue(Asset, TEXT("DA.SourceFingerprint"), *SourceFingerprint);
        if (!AuthoredShotFingerprint.IsEmpty())
            Package->GetMetaData()->SetValue(Asset, TEXT("DA.AuthoredShotFingerprint"),
                *AuthoredShotFingerprint);
        if (!CinematicBeat.IsEmpty())
            Package->GetMetaData()->SetValue(Asset, TEXT("DA.CinematicBeat"), *CinematicBeat);
        if (!CinematicPayloadFingerprint.IsEmpty())
            Package->GetMetaData()->SetValue(Asset, TEXT("DA.CinematicPayloadFingerprint"),
                *CinematicPayloadFingerprint);
        FAssetRegistryModule::AssetCreated(Asset);
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
        { Errors.Add(FText::FromString(TEXT("Could not save ") + Filename)); return false; }
        if (OutSavedAsset != nullptr) *OutSavedAsset = Asset;
        return true;
    }

    bool ReadVector3(const TSharedPtr<FJsonObject>& Object,
        const TCHAR* Field, FVector& OutValue)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values)
            || Values == nullptr || Values->Num() != 3) return false;
        double Components[3] = {};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (!(*Values)[Index].IsValid()
                || !(*Values)[Index]->TryGetNumber(Components[Index])
                || !FMath::IsFinite(Components[Index])
                || FMath::Abs(Components[Index]) > 100000.0) return false;
        }
        OutValue = FVector(Components[0], Components[1], Components[2]);
        return true;
    }

    bool ReadTimedCues(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
        const FString& Prefix, const TArray<FString>& ExpectedIds,
        const int32 DurationFrames, TArray<FAuthoredAscensionCue>& OutCues)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values)
            || Values == nullptr || Values->Num() != ExpectedIds.Num()) return false;
        int32 PreviousFrame = INDEX_NONE;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Cue = (*Values)[Index].IsValid()
                ? (*Values)[Index]->AsObject() : nullptr;
            FAuthoredAscensionCue& Authored = OutCues.Emplace_GetRef();
            if (!ExactKeys(Cue, {TEXT("frame"), TEXT("cueId")})
                || !ReadExactInteger(Cue, TEXT("frame"), Authored.Frame)
                || !Cue->TryGetStringField(TEXT("cueId"), Authored.CueId)
                || Authored.Frame < 0 || Authored.Frame >= DurationFrames
                || (PreviousFrame != INDEX_NONE && Authored.Frame <= PreviousFrame)
                || Authored.CueId != ExpectedIds[Index]
                || !Authored.CueId.StartsWith(Prefix)) return false;
            PreviousFrame = Authored.Frame;
        }
        return true;
    }

    bool ReadActions(const TSharedPtr<FJsonObject>& Object,
        const TArray<FString>& ExpectedIds, const int32 DurationFrames,
        TArray<FAuthoredAscensionAction>& OutActions)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(TEXT("actions"), Values)
            || Values == nullptr || Values->Num() != ExpectedIds.Num()) return false;
        int32 PreviousFrame = INDEX_NONE;
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Action = (*Values)[Index].IsValid()
                ? (*Values)[Index]->AsObject() : nullptr;
            FAuthoredAscensionAction& Authored = OutActions.Emplace_GetRef();
            if (!ExactKeys(Action,
                    {TEXT("frame"), TEXT("actionId"), TEXT("targetId"), TEXT("payload")})
                || !ReadExactInteger(Action, TEXT("frame"), Authored.Frame)
                || !Action->TryGetStringField(TEXT("actionId"), Authored.ActionId)
                || !Action->TryGetStringField(TEXT("targetId"), Authored.TargetId)
                || !Action->TryGetStringField(TEXT("payload"), Authored.Payload)
                || Authored.Frame < 0 || Authored.Frame >= DurationFrames
                || (PreviousFrame != INDEX_NONE && Authored.Frame <= PreviousFrame)
                || Authored.ActionId != ExpectedIds[Index]
                || Authored.TargetId.IsEmpty() || Authored.Payload.IsEmpty()) return false;
            PreviousFrame = Authored.Frame;
        }
        return true;
    }

    bool ReadCamera(const TSharedPtr<FJsonObject>& Shot,
        FAuthoredAscensionCamera& OutCamera)
    {
        const TSharedPtr<FJsonObject>* Camera = nullptr;
        FVector StartRotation;
        FVector EndRotation;
        if (!Shot.IsValid() || !Shot->TryGetObjectField(TEXT("camera"), Camera)
            || Camera == nullptr
            || !ExactKeys(*Camera, {TEXT("startLocation"), TEXT("endLocation"),
                TEXT("startRotation"), TEXT("endRotation"), TEXT("fieldOfView")})
            || !ReadVector3(*Camera, TEXT("startLocation"), OutCamera.StartLocation)
            || !ReadVector3(*Camera, TEXT("endLocation"), OutCamera.EndLocation)
            || !ReadVector3(*Camera, TEXT("startRotation"), StartRotation)
            || !ReadVector3(*Camera, TEXT("endRotation"), EndRotation)
            || !(*Camera)->TryGetNumberField(TEXT("fieldOfView"), OutCamera.FieldOfView)
            || !FMath::IsFinite(OutCamera.FieldOfView)
            || OutCamera.FieldOfView < 20.0 || OutCamera.FieldOfView > 120.0
            || OutCamera.StartLocation.Equals(OutCamera.EndLocation)) return false;
        if (FMath::Abs(StartRotation.X) > 360.0 || FMath::Abs(StartRotation.Y) > 360.0
            || FMath::Abs(StartRotation.Z) > 360.0 || FMath::Abs(EndRotation.X) > 360.0
            || FMath::Abs(EndRotation.Y) > 360.0 || FMath::Abs(EndRotation.Z) > 360.0)
            return false;
        OutCamera.StartRotation = FRotator(
            StartRotation.X, StartRotation.Y, StartRotation.Z);
        OutCamera.EndRotation = FRotator(
            EndRotation.X, EndRotation.Y, EndRotation.Z);
        return true;
    }

    bool LoadAuthoredShots(const FDAFirstAscensionContentManifest& Manifest,
        FAuthoredAscensionShotList& OutShotList, TArray<FText>& Errors)
    {
        OutShotList = {};
        const FString ShotSource = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), Manifest.CinematicShotSourcePath));
        FString ShotJson;
        TSharedPtr<FJsonObject> ShotRoot;
        if (!FFileHelper::LoadFileToString(ShotJson, *ShotSource))
        {
            Errors.Add(FText::FromString(TEXT("MissingSource: complete authored Ascension cinematic source is absent: ")
                + Manifest.CinematicShotSourcePath));
            return false;
        }
        OutShotList.Fingerprint = Sha1Utf8(ShotJson);
        const TSharedRef<TJsonReader<>> ShotReader = TJsonReaderFactory<>::Create(ShotJson);
        FString CinematicId;
        int32 SchemaVersion = 0;
        if (!FJsonSerializer::Deserialize(ShotReader, ShotRoot)
            || !ExactKeys(ShotRoot,
                {TEXT("schemaVersion"), TEXT("cinematicId"), TEXT("displayRate"), TEXT("shots")})
            || !ReadExactInteger(ShotRoot, TEXT("schemaVersion"), SchemaVersion)
            || !ReadExactInteger(ShotRoot, TEXT("displayRate"), OutShotList.DisplayRate)
            || !ShotRoot->TryGetStringField(TEXT("cinematicId"), CinematicId)
            || SchemaVersion != 1
            || CinematicId != TEXT("cinematic.forgeweave.first_ascension")
            || OutShotList.DisplayRate != 30)
        {
            Errors.Add(FText::FromString(TEXT("Ascension cinematic source requires its exact v1 identity and 30 fps authority.")));
            return false;
        }
        const TArray<TSharedPtr<FJsonValue>>* Shots = nullptr;
        if (!ShotRoot->TryGetArrayField(TEXT("shots"), Shots) || Shots == nullptr
            || Shots->Num() != ExpectedAscensionShots().Num())
        {
            Errors.Add(FText::FromString(TEXT("Ascension cinematic source must contain exactly the five required ordered beats.")));
            return false;
        }
        TSet<FString> PackagePaths;
        for (int32 Index = 0; Index < Shots->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> Shot = (*Shots)[Index].IsValid()
                ? (*Shots)[Index]->AsObject() : nullptr;
            const FExpectedAscensionShot& Expected = ExpectedAscensionShots()[Index];
            FAuthoredAscensionShot& Authored = OutShotList.Shots.Emplace_GetRef();
            if (!ExactKeys(Shot, {TEXT("beat"), TEXT("sequenceAssetPath"),
                    TEXT("durationFrames"), TEXT("camera"), TEXT("actions"),
                    TEXT("audioCues"), TEXT("vfxCues")})
                || !Shot->TryGetStringField(TEXT("beat"), Authored.Beat)
                || !Shot->TryGetStringField(TEXT("sequenceAssetPath"), Authored.SequenceAssetPath)
                || !ReadExactInteger(Shot, TEXT("durationFrames"), Authored.DurationFrames)
                || Authored.Beat != ExpectedAscensionBeats()[Index]
                || Authored.Beat != Expected.Beat
                || Authored.SequenceAssetPath != Expected.SequenceAssetPath
                || Authored.SequenceAssetPath == Manifest.CinematicAssetPath
                || PackagePaths.Contains(Authored.SequenceAssetPath)
                || Authored.DurationFrames != Expected.DurationFrames
                || !ReadCamera(Shot, Authored.Camera)
                || !ReadActions(Shot, Expected.ActionIds,
                    Authored.DurationFrames, Authored.Actions)
                || !ReadTimedCues(Shot, TEXT("audioCues"), TEXT("audio.ascension."),
                    Expected.AudioCueIds, Authored.DurationFrames, Authored.AudioCues)
                || !ReadTimedCues(Shot, TEXT("vfxCues"), TEXT("vfx.ascension."),
                    Expected.VfxCueIds, Authored.DurationFrames, Authored.VfxCues))
            {
                Errors.Add(FText::FromString(FString::Printf(
                    TEXT("Ascension cinematic shot %d failed exact beat/camera/action/audio/VFX semantic validation."),
                    Index)));
                return false;
            }
            FString CanonicalPayload;
            const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&CanonicalPayload);
            if (!FJsonSerializer::Serialize(Shot.ToSharedRef(), Writer)) return false;
            Authored.PayloadFingerprint = Sha1Utf8(CanonicalPayload);
            PackagePaths.Add(Authored.SequenceAssetPath);
        }
        return true;
    }

    bool AddLinearCameraKeys(UMovieScene3DTransformSection& Section,
        const FAuthoredAscensionShot& Shot)
    {
        const auto Channels =
            Section.GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
        if (Channels.Num() < 9) return false;
        const double StartValues[6] = {
            Shot.Camera.StartLocation.X, Shot.Camera.StartLocation.Y, Shot.Camera.StartLocation.Z,
            Shot.Camera.StartRotation.Roll, Shot.Camera.StartRotation.Pitch,
            Shot.Camera.StartRotation.Yaw};
        const double EndValues[6] = {
            Shot.Camera.EndLocation.X, Shot.Camera.EndLocation.Y, Shot.Camera.EndLocation.Z,
            Shot.Camera.EndRotation.Roll, Shot.Camera.EndRotation.Pitch,
            Shot.Camera.EndRotation.Yaw};
        for (int32 Index = 0; Index < 6; ++Index)
        {
            Channels[Index]->AddLinearKey(FFrameNumber(0), StartValues[Index]);
            Channels[Index]->AddLinearKey(
                FFrameNumber(Shot.DurationFrames - 1), EndValues[Index]);
        }
        for (int32 Index = 6; Index < 9; ++Index) Channels[Index]->SetDefault(1.0);
        return true;
    }

    TArray<FMovieSceneMarkedFrame> BuildPayloadMarkers(
        const FAuthoredAscensionShot& Shot)
    {
        TArray<FMovieSceneMarkedFrame> Markers;
        for (const FAuthoredAscensionAction& Action : Shot.Actions)
        {
            FMovieSceneMarkedFrame& Marker = Markers.Emplace_GetRef();
            Marker.FrameNumber = FFrameNumber(Action.Frame);
            Marker.Label = FString::Printf(TEXT("action:%s|target=%s|payload=%s"),
                *Action.ActionId, *Action.TargetId, *Action.Payload);
        }
        for (const FAuthoredAscensionCue& Cue : Shot.AudioCues)
        {
            FMovieSceneMarkedFrame& Marker = Markers.Emplace_GetRef();
            Marker.FrameNumber = FFrameNumber(Cue.Frame);
            Marker.Label = TEXT("audio:") + Cue.CueId;
        }
        for (const FAuthoredAscensionCue& Cue : Shot.VfxCues)
        {
            FMovieSceneMarkedFrame& Marker = Markers.Emplace_GetRef();
            Marker.FrameNumber = FFrameNumber(Cue.Frame);
            Marker.Label = TEXT("vfx:") + Cue.CueId;
        }
        Markers.Sort([](const FMovieSceneMarkedFrame& Left,
            const FMovieSceneMarkedFrame& Right)
        {
            return Left.FrameNumber != Right.FrameNumber
                ? Left.FrameNumber < Right.FrameNumber
                : Left.Label < Right.Label;
        });
        return Markers;
    }

    bool ValidatePayloadMarkers(const UMovieScene& MovieScene,
        const FAuthoredAscensionShot& Shot)
    {
        const TArray<FMovieSceneMarkedFrame> Expected = BuildPayloadMarkers(Shot);
        const TArray<FMovieSceneMarkedFrame>& Actual = MovieScene.GetMarkedFrames();
        if (!MovieScene.AreMarkedFramesLocked() || Actual.Num() != Expected.Num())
            return false;
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
        {
            if (Actual[Index].FrameNumber != Expected[Index].FrameNumber
                || Actual[Index].Label != Expected[Index].Label) return false;
        }
        return true;
    }

    bool ValidateGeneratedShot(const ULevelSequence* Sequence,
        const FAuthoredAscensionShot& Shot, TArray<FText>& Errors)
    {
        UMovieScene* MovieScene = Sequence == nullptr ? nullptr : Sequence->GetMovieScene();
        const UMovieSceneCameraCutTrack* CutTrack = MovieScene == nullptr ? nullptr
            : MovieScene->FindTrack<UMovieSceneCameraCutTrack>();
        const UMovieSceneCameraCutSection* CutSection = CutTrack != nullptr
            && CutTrack->GetAllSections().Num() == 1
            ? Cast<UMovieSceneCameraCutSection>(CutTrack->GetAllSections()[0]) : nullptr;
        const FGuid CameraGuid = CutSection == nullptr
            ? FGuid() : CutSection->GetCameraBindingID().GetGuid();
        const UMovieSceneSpawnTrack* SpawnTrack = MovieScene == nullptr || !CameraGuid.IsValid()
            ? nullptr : MovieScene->FindTrack<UMovieSceneSpawnTrack>(CameraGuid);
        const UMovieScene3DTransformTrack* TransformTrack = MovieScene == nullptr
            || !CameraGuid.IsValid()
            ? nullptr : MovieScene->FindTrack<UMovieScene3DTransformTrack>(CameraGuid);
        const UMovieScene3DTransformSection* TransformSection = TransformTrack != nullptr
            && TransformTrack->GetAllSections().Num() == 1
            ? Cast<UMovieScene3DTransformSection>(TransformTrack->GetAllSections()[0]) : nullptr;
        const UMovieSceneSpawnSection* SpawnSection = SpawnTrack != nullptr
            && SpawnTrack->GetAllSections().Num() == 1
            ? Cast<UMovieSceneSpawnSection>(SpawnTrack->GetAllSections()[0]) : nullptr;
        bool bExactTransform = TransformSection != nullptr;
        if (TransformSection != nullptr)
        {
            const auto Channels =
                TransformSection->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
            bExactTransform = Channels.Num() >= 9;
            for (int32 Index = 0; bExactTransform && Index < 6; ++Index)
                bExactTransform = Channels[Index] != nullptr
                    && Channels[Index]->GetNumKeys() == 2;
        }
        const bool bExactRange = MovieScene != nullptr
            && !MovieScene->GetPlaybackRange().IsEmpty()
            && MovieScene->GetPlaybackRange().GetLowerBoundValue().Value == 0
            && MovieScene->GetPlaybackRange().GetUpperBoundValue().Value
                == Shot.DurationFrames;
        const auto HasExactShotRange = [&Shot](const UMovieSceneSection* Section)
        {
            return Section != nullptr && Section->HasStartFrame() && Section->HasEndFrame()
                && Section->GetInclusiveStartFrame().Value == 0
                && Section->GetExclusiveEndFrame().Value == Shot.DurationFrames;
        };
        if (!bExactRange || CutSection == nullptr || !CameraGuid.IsValid()
            || MovieScene->FindSpawnable(CameraGuid) == nullptr
            || SpawnSection == nullptr || !bExactTransform
            || !HasExactShotRange(SpawnSection)
            || !HasExactShotRange(TransformSection)
            || !HasExactShotRange(CutSection)
            || MovieScene->GetDisplayRate() != FFrameRate(30, 1)
            || !ValidatePayloadMarkers(*MovieScene, Shot))
        {
            Errors.Add(FText::FromString(TEXT("Generated Ascension shot is not a usable ranged spawn-camera sequence: ")
                + Shot.Beat));
            return false;
        }
        return true;
    }

    bool GenerateShotSequence(const FAuthoredAscensionShot& Shot,
        const int32 DisplayRate, ULevelSequence*& OutSequence, TArray<FText>& Errors)
    {
        OutSequence = NewObject<ULevelSequence>();
        if (OutSequence == nullptr) return false;
        OutSequence->Initialize();
        UMovieScene* MovieScene = OutSequence->GetMovieScene();
        ACineCameraActor* CameraTemplate = NewObject<ACineCameraActor>(
            GetTransientPackage(), ACineCameraActor::StaticClass(), NAME_None, RF_Transient);
        if (MovieScene == nullptr || CameraTemplate == nullptr
            || CameraTemplate->GetCineCameraComponent() == nullptr)
        {
            Errors.Add(FText::FromString(TEXT("Could not create the authored Ascension camera template.")));
            return false;
        }
        MovieScene->SetDisplayRate(FFrameRate(DisplayRate, 1));
        MovieScene->SetTickResolutionDirectly(FFrameRate(DisplayRate, 1));
        MovieScene->SetPlaybackRange(0, Shot.DurationFrames);
        for (const FMovieSceneMarkedFrame& Marker : BuildPayloadMarkers(Shot))
            MovieScene->AddMarkedFrame(Marker);
        MovieScene->SetMarkedFramesLocked(true);
        CameraTemplate->SetActorLocationAndRotation(
            Shot.Camera.StartLocation, Shot.Camera.StartRotation);
        CameraTemplate->GetCineCameraComponent()->SetFieldOfView(
            static_cast<float>(Shot.Camera.FieldOfView));
        const FGuid CameraGuid = MovieScene->AddSpawnable(
            TEXT("AscensionCamera_") + Shot.Beat, *CameraTemplate);

        UMovieSceneSpawnTrack* SpawnTrack = CameraGuid.IsValid()
            ? MovieScene->AddTrack<UMovieSceneSpawnTrack>(CameraGuid) : nullptr;
        UMovieSceneSpawnSection* SpawnSection = SpawnTrack == nullptr ? nullptr
            : Cast<UMovieSceneSpawnSection>(SpawnTrack->CreateNewSection());
        UMovieScene3DTransformTrack* TransformTrack = CameraGuid.IsValid()
            ? MovieScene->AddTrack<UMovieScene3DTransformTrack>(CameraGuid) : nullptr;
        UMovieScene3DTransformSection* TransformSection = TransformTrack == nullptr ? nullptr
            : Cast<UMovieScene3DTransformSection>(TransformTrack->CreateNewSection());
        UMovieSceneCameraCutTrack* CutTrack = MovieScene->AddTrack<UMovieSceneCameraCutTrack>();
        UMovieSceneCameraCutSection* CutSection = CutTrack == nullptr ? nullptr
            : Cast<UMovieSceneCameraCutSection>(CutTrack->CreateNewSection());
        if (SpawnSection == nullptr || TransformSection == nullptr || CutSection == nullptr)
        {
            Errors.Add(FText::FromString(TEXT("Could not create exact spawn/transform/camera-cut tracks for: ")
                + Shot.Beat));
            return false;
        }
        const TRange<FFrameNumber> ShotRange(
            FFrameNumber(0), FFrameNumber(Shot.DurationFrames));
        SpawnSection->SetRange(ShotRange);
        SpawnSection->GetChannel().SetDefault(true);
        SpawnTrack->AddSection(*SpawnSection);
        TransformSection->SetRange(ShotRange);
        if (!AddLinearCameraKeys(*TransformSection, Shot)) return false;
        TransformTrack->AddSection(*TransformSection);
        FMovieSceneObjectBindingID CameraBinding;
        CameraBinding.SetGuid(CameraGuid);
        CutSection->SetCameraBindingID(CameraBinding);
        CutSection->SetRange(ShotRange);
        CutTrack->AddSection(*CutSection);
        return ValidateGeneratedShot(OutSequence, Shot, Errors);
    }

    bool GenerateAndSaveShotSequences(FAuthoredAscensionShotList& ShotList,
        const FString& SourceFingerprint, TArray<FText>& Errors)
    {
        for (FAuthoredAscensionShot& Shot : ShotList.Shots)
        {
            ULevelSequence* Generated = nullptr;
            UObject* Saved = nullptr;
            if (!GenerateShotSequence(Shot, ShotList.DisplayRate, Generated, Errors)
                || !SaveAsset(Generated, Shot.SequenceAssetPath, SourceFingerprint, Errors,
                    ShotList.Fingerprint, Shot.Beat, Shot.PayloadFingerprint, &Saved))
                return false;
            Shot.Sequence = Cast<ULevelSequence>(Saved);
            if (!ValidateGeneratedShot(Shot.Sequence, Shot, Errors)) return false;
        }
        return true;
    }

    bool ValidateCinematic(const ULevelSequence* Cinematic,
        const FAuthoredAscensionShotList& ShotList, TArray<FText>& Errors)
    {
        const UMovieScene* MovieScene = Cinematic == nullptr ? nullptr
            : Cinematic->GetMovieScene();
        const UMovieSceneCinematicShotTrack* ShotTrack = MovieScene == nullptr ? nullptr
            : MovieScene->FindTrack<UMovieSceneCinematicShotTrack>();
        const TArray<UMovieSceneSection*>* Sections = ShotTrack == nullptr ? nullptr
            : &ShotTrack->GetAllSections();
        int32 ExpectedStart = 0;
        if (MovieScene == nullptr || ShotTrack == nullptr || Sections == nullptr
            || Sections->Num() != ShotList.Shots.Num()
            || MovieScene->GetDisplayRate() != FFrameRate(ShotList.DisplayRate, 1))
        {
            Errors.Add(FText::FromString(TEXT("Generated Ascension LevelSequence has no exact cinematic shot track.")));
            return false;
        }
        for (int32 Index = 0; Index < ShotList.Shots.Num(); ++Index)
        {
            const UMovieSceneCinematicShotSection* Section =
                Cast<UMovieSceneCinematicShotSection>((*Sections)[Index]);
            const FAuthoredAscensionShot& Authored = ShotList.Shots[Index];
            if (Authored.Sequence == nullptr || Section == nullptr
                || Section->GetSequence() != Authored.Sequence
                || !Section->HasStartFrame() || !Section->HasEndFrame()
                || Section->GetInclusiveStartFrame().Value != ExpectedStart
                || Section->GetExclusiveEndFrame().Value
                    != ExpectedStart + Authored.DurationFrames)
            {
                Errors.Add(FText::FromString(FString::Printf(
                    TEXT("Generated Ascension shot %d does not match its authored sequence/range."),
                    Index)));
                return false;
            }
            ExpectedStart += Authored.DurationFrames;
        }
        if (MovieScene->GetPlaybackRange().IsEmpty()
            || MovieScene->GetPlaybackRange().GetLowerBoundValue().Value != 0
            || MovieScene->GetPlaybackRange().GetUpperBoundValue().Value != ExpectedStart)
        {
            Errors.Add(FText::FromString(TEXT("Generated Ascension LevelSequence playback range is incomplete.")));
            return false;
        }
        return true;
    }

    bool BuildCinematic(const FAuthoredAscensionShotList& ShotList,
        ULevelSequence*& OutCinematic, TArray<FText>& Errors)
    {
        OutCinematic = NewObject<ULevelSequence>();
        if (OutCinematic == nullptr) return false;
        OutCinematic->Initialize();
        UMovieScene* MovieScene = OutCinematic->GetMovieScene();
        UMovieSceneCinematicShotTrack* ShotTrack = MovieScene == nullptr ? nullptr
            : MovieScene->AddTrack<UMovieSceneCinematicShotTrack>();
        if (MovieScene == nullptr || ShotTrack == nullptr)
        {
            Errors.Add(FText::FromString(TEXT("Could not create the Ascension cinematic shot track.")));
            return false;
        }
        MovieScene->SetDisplayRate(FFrameRate(ShotList.DisplayRate, 1));
        MovieScene->SetTickResolutionDirectly(FFrameRate(ShotList.DisplayRate, 1));
        int32 StartFrame = 0;
        for (const FAuthoredAscensionShot& Shot : ShotList.Shots)
        {
            UMovieSceneCinematicShotSection* Section = Cast<UMovieSceneCinematicShotSection>(
                ShotTrack->AddSequence(Shot.Sequence, FFrameNumber(StartFrame), Shot.DurationFrames));
            if (Section == nullptr)
            {
                Errors.Add(FText::FromString(TEXT("Could not convert authored Ascension shot into LevelSequence section: ")
                    + Shot.Beat));
                return false;
            }
            StartFrame += Shot.DurationFrames;
        }
        MovieScene->SetPlaybackRange(0, StartFrame);
        return ValidateCinematic(OutCinematic, ShotList, Errors);
    }

    UClass* ImportClass(const FName Name)
    {
        return Name == TEXT("StaticMesh") ? UStaticMesh::StaticClass()
            : Name == TEXT("Texture2D") ? UTexture2D::StaticClass() : nullptr;
    }

    bool ValidateRegistry(const FString& PackagePath, UClass* Expected,
        TArray<FText>& Errors, const FString& ExpectedFingerprint = FString())
    {
        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry")).Get();
        Registry.ScanPathsSynchronous({FPackageName::GetLongPackagePath(PackagePath)}, true);
        const FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath(PackagePath)));
        UObject* Asset = AssetData.IsValid() ? AssetData.GetAsset() : nullptr;
        if (!AssetData.IsValid() || Expected == nullptr
            || AssetData.PackageName != FName(*PackagePath)
            || AssetData.AssetClassPath != Expected->GetClassPathName()
            || Asset == nullptr || Asset->GetClass() != Expected)
        {
            Errors.Add(FText::FromString(TEXT("NoRegistry: exact First Ascension asset is absent or wrong class: ")
                + PackagePath));
            return false;
        }
        if (!ExpectedFingerprint.IsEmpty()
            && Asset->GetPackage()->GetMetaData()->GetValue(
                Asset, TEXT("DA.SourceFingerprint")) != ExpectedFingerprint)
        {
            Errors.Add(FText::FromString(TEXT("NoFingerprint: generated asset differs from First Ascension source: ")
                + PackagePath));
            return false;
        }
        return true;
    }

    bool LoadAndValidateGeneratedShotSequences(
        FAuthoredAscensionShotList& ShotList,
        const FString& SourceFingerprint,
        TArray<FText>& Errors)
    {
        bool bValid = true;
        for (FAuthoredAscensionShot& Shot : ShotList.Shots)
        {
            bValid &= ValidateRegistry(Shot.SequenceAssetPath,
                ULevelSequence::StaticClass(), Errors, SourceFingerprint);
            Shot.Sequence = LoadObject<ULevelSequence>(
                nullptr, *ObjectPath(Shot.SequenceAssetPath));
            const UMetaData* MetaData = Shot.Sequence == nullptr ? nullptr
                : Shot.Sequence->GetPackage()->GetMetaData();
            if (Shot.Sequence == nullptr || MetaData == nullptr
                || MetaData->GetValue(Shot.Sequence, TEXT("DA.AuthoredShotFingerprint"))
                    != ShotList.Fingerprint
                || MetaData->GetValue(Shot.Sequence, TEXT("DA.CinematicBeat")) != Shot.Beat
                || MetaData->GetValue(Shot.Sequence, TEXT("DA.CinematicPayloadFingerprint"))
                    != Shot.PayloadFingerprint
                || !ValidateGeneratedShot(Shot.Sequence, Shot, Errors))
            {
                Errors.Add(FText::FromString(TEXT("Generated Ascension shot cache differs from authored payload: ")
                    + Shot.Beat));
                bValid = false;
            }
        }
        return bValid;
    }

    bool ValidateFusion(const FDAFirstAscensionContentManifest& Manifest,
        TArray<FText>& Errors)
    {
        const UDA_CardDefinition* Definition = LoadObject<UDA_CardDefinition>(
            nullptr, *ObjectPath(Manifest.FusionAssetPath));
        int32 ConstructionCycles = 0, CraftCapital = 0, CraftInsight = 0;
        int32 UtilityPower = 0, UtilityData = 0;
        FName CraftingFacility;
        float Workforce = 0.f, Throughput = 0.f, Adjacency = 0.f, Dependency = 0.f, Hunger = 0.f;
        if (Definition == nullptr || Definition->DefinitionId != Manifest.FusionDefinitionId
            || !Definition->TryGetConstructionCycles(ConstructionCycles)
            || !Definition->TryGetCraftCapital(CraftCapital)
            || !Definition->TryGetCraftInsight(CraftInsight)
            || !Definition->TryGetRequiredCraftingFacilityId(CraftingFacility)
            || !Definition->TryGetUtilityPower(UtilityPower)
            || !Definition->TryGetUtilityData(UtilityData)
            || !Definition->TryGetWorkforceRequirementModifier(Workforce)
            || !Definition->TryGetIndustrialThroughputModifier(Throughput)
            || !Definition->TryGetAdjacentIndustrialConstructionSpeedModifier(Adjacency)
            || !Definition->TryGetSynaraDependencyPerCycle(Dependency)
            || !Definition->TryGetForgeweaveResourceHungerPerCycle(Hunger)
            || ConstructionCycles != Manifest.ConstructionCycles
            || CraftCapital != Manifest.CraftCapital
            || CraftInsight != Manifest.CraftInsight
            || CraftingFacility != Manifest.RequiredCraftingFacilityId
            || UtilityPower != Manifest.UtilityPower
            || UtilityData != Manifest.UtilityData
            || !FMath::IsNearlyEqual(Workforce, Manifest.WorkforceRequirementModifier)
            || !FMath::IsNearlyEqual(Throughput, Manifest.IndustrialThroughputModifier)
            || !FMath::IsNearlyEqual(Adjacency, Manifest.AdjacentIndustrialConstructionSpeedModifier)
            || !FMath::IsNearlyEqual(Dependency, Manifest.DependencyPerCycle)
            || !FMath::IsNearlyEqual(Hunger, Manifest.ResourceHungerPerCycle))
        {
            Errors.Add(FText::FromString(TEXT("NoCache: Autonomous Factory generated card is absent or differs from exact authored effects.")));
            return false;
        }
        return ValidateRegistry(Manifest.FusionAssetPath, UDA_CardDefinition::StaticClass(), Errors);
    }

    bool ValidateGeneratedCinematic(const FDAFirstAscensionContentManifest& Manifest,
        TArray<FText>& Errors)
    {
        FAuthoredAscensionShotList ShotList;
        if (!LoadAuthoredShots(Manifest, ShotList, Errors)) return false;
        bool bValid = LoadAndValidateGeneratedShotSequences(
            ShotList, Manifest.Fingerprint, Errors);
        bValid &= ValidateRegistry(Manifest.CinematicAssetPath,
            ULevelSequence::StaticClass(), Errors, Manifest.Fingerprint);
        const ULevelSequence* Cinematic = LoadObject<ULevelSequence>(
            nullptr, *ObjectPath(Manifest.CinematicAssetPath));
        const UMetaData* MetaData = Cinematic == nullptr ? nullptr
            : Cinematic->GetPackage()->GetMetaData();
        bValid &= ValidateCinematic(Cinematic, ShotList, Errors)
            && MetaData != nullptr
            && MetaData->GetValue(Cinematic, TEXT("DA.AuthoredShotFingerprint"))
                == ShotList.Fingerprint
            && MetaData->GetValue(Cinematic, TEXT("DA.CinematicPayloadFingerprint"))
                == ShotList.Fingerprint;
        if (!bValid)
            Errors.Add(FText::FromString(TEXT("Generated Ascension cinematic cache differs from its complete authored source.")));
        return bValid;
    }

    bool ValidateGenerated(const FDAFirstAscensionContentManifest& Manifest,
        TArray<FText>& Errors)
    {
        FAuthoredAscensionShotList ShotList;
        const bool bShotsValid = LoadAuthoredShots(Manifest, ShotList, Errors);
        bool bValid = ValidateRegistry(Manifest.QuestAssetPath,
                UDARegionalQuestDefinition::StaticClass(), Errors)
            && ValidateRegistry(Manifest.DoctrineAssetPath,
                UDAReplicationDoctrineDefinition::StaticClass(), Errors)
            && ValidateRegistry(Manifest.CinematicAssetPath, ULevelSequence::StaticClass(),
                Errors, Manifest.Fingerprint)
            && ValidateFusion(Manifest, Errors);
        const UDARegionalQuestDefinition* Quest = LoadObject<UDARegionalQuestDefinition>(
            nullptr, *ObjectPath(Manifest.QuestAssetPath));
        const UDAReplicationDoctrineDefinition* Doctrine = LoadObject<UDAReplicationDoctrineDefinition>(
            nullptr, *ObjectPath(Manifest.DoctrineAssetPath));
        bValid &= Quest != nullptr && Quest->Quest.QuestId == Manifest.QuestId
            && Quest->Quest.Title == Manifest.QuestTitle
            && Quest->SourceFingerprint == Manifest.Fingerprint;
        bValid &= Doctrine != nullptr && Doctrine->DoctrineId == Manifest.DoctrineId
            && Doctrine->CadenceDevelopmentCycles == Manifest.ReplicationCadenceDevelopmentCycles
            && Doctrine->SourceFingerprint == Manifest.Fingerprint;
        bValid &= bShotsValid
            && LoadAndValidateGeneratedShotSequences(
                ShotList, Manifest.Fingerprint, Errors);
        const ULevelSequence* Cinematic = LoadObject<ULevelSequence>(
            nullptr, *ObjectPath(Manifest.CinematicAssetPath));
        bValid &= bShotsValid && ValidateCinematic(Cinematic, ShotList, Errors)
            && Cinematic != nullptr
            && Cinematic->GetPackage()->GetMetaData()->GetValue(
                Cinematic, TEXT("DA.AuthoredShotFingerprint")) == ShotList.Fingerprint
            && Cinematic->GetPackage()->GetMetaData()->GetValue(
                Cinematic, TEXT("DA.CinematicPayloadFingerprint")) == ShotList.Fingerprint;
        for (const FDAFirstAscensionBuildingImport& Import : Manifest.BuildingImports)
            bValid &= ValidateRegistry(Import.AssetPath, ImportClass(Import.AssetClass), Errors);
        if (!bValid) Errors.Add(FText::FromString(TEXT("First Ascension generated cache does not exactly match its source fingerprint.")));
        return bValid;
    }

    bool PreflightRealSources(const FDAFirstAscensionContentManifest& Manifest,
        FAuthoredAscensionShotList& OutShotList, TArray<FText>& Errors)
    {
        bool bValid = true;
        for (const FDAFirstAscensionBuildingImport& Import : Manifest.BuildingImports)
        {
            const FString Source = FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::ProjectDir(), Import.SourcePath));
            if (ImportClass(Import.AssetClass) == nullptr || !FPaths::FileExists(Source))
            {
                Errors.Add(FText::FromString(TEXT("MissingSource: supply the real authored Autonomous Factory source: ")
                    + Import.SourcePath));
                bValid = false;
            }
        }
        return LoadAuthoredShots(Manifest, OutShotList, Errors) && bValid;
    }

    bool ImportBuildingSources(const FDAFirstAscensionContentManifest& Manifest,
        TArray<FText>& Errors)
    {
        TArray<UAssetImportTask*> Tasks;
        for (const FDAFirstAscensionBuildingImport& Import : Manifest.BuildingImports)
        {
            UAssetImportTask* Task = NewObject<UAssetImportTask>();
            Task->Filename = FPaths::ConvertRelativePathToFull(
                FPaths::Combine(FPaths::ProjectDir(), Import.SourcePath));
            Task->DestinationPath = FPackageName::GetLongPackagePath(Import.AssetPath);
            Task->DestinationName = FPackageName::GetLongPackageAssetName(Import.AssetPath);
            Task->bAutomated = true;
            Task->bReplaceExisting = true;
            Task->bSave = true;
            Tasks.Add(Task);
        }
        FAssetToolsModule::GetModule().Get().ImportAssetTasks(Tasks);
        bool bValid = true;
        for (const FDAFirstAscensionBuildingImport& Import : Manifest.BuildingImports)
            bValid &= ValidateRegistry(Import.AssetPath, ImportClass(Import.AssetClass), Errors);
        return bValid;
    }
}

UDAFirstAscensionContentCommandlet::UDAFirstAscensionContentCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UDAFirstAscensionContentCommandlet::Main(const FString& Params)
{
    FModuleManager::Get().LoadModule(TEXT("DominionWorld"));
    FString ManifestPath;
    FParse::Value(*Params, TEXT("Manifest="), ManifestPath);
    if (ManifestPath.IsEmpty()) ManifestPath = FDAFirstAscensionContentPipeline::GetCanonicalManifestPath();
    FDAFirstAscensionContentManifest Manifest;
    TArray<FText> Errors;
    if (!FDAFirstAscensionContentPipeline::LoadFile(ManifestPath, Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }
    const bool bCinematicOnly = FParse::Param(*Params, TEXT("CinematicOnly"));
    if (FParse::Param(*Params, TEXT("ValidateOnly")))
    {
        const bool bValid = bCinematicOnly
            ? ValidateGeneratedCinematic(Manifest, Errors)
            : ValidateGenerated(Manifest, Errors);
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return bValid ? 0 : 2;
    }

    if (bCinematicOnly)
    {
        FAuthoredAscensionShotList ShotList;
        ULevelSequence* Cinematic = nullptr;
        const bool bValid = LoadAuthoredShots(Manifest, ShotList, Errors)
            && GenerateAndSaveShotSequences(ShotList, Manifest.Fingerprint, Errors)
            && BuildCinematic(ShotList, Cinematic, Errors)
            && SaveAsset(Cinematic, Manifest.CinematicAssetPath,
                Manifest.Fingerprint, Errors, ShotList.Fingerprint,
                TEXT("composite"), ShotList.Fingerprint)
            && ValidateGeneratedCinematic(Manifest, Errors);
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return bValid ? 0 : 1;
    }

    // Source preflight occurs before any package mutation; absent source files never create placeholders.
    FAuthoredAscensionShotList ShotList;
    if (!PreflightRealSources(Manifest, ShotList, Errors) || !ValidateFusion(Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }
    UDARegionalQuestDefinition* Quest = NewObject<UDARegionalQuestDefinition>();
    Quest->Quest.QuestId = Manifest.QuestId;
    Quest->Quest.Title = Manifest.QuestTitle;
    Quest->Quest.AssetPath = Manifest.QuestAssetPath;
    Quest->Quest.OutcomeTags = {Manifest.QuestCompletionHistory};
    Quest->SourceFingerprint = Manifest.Fingerprint;
    UDAReplicationDoctrineDefinition* Doctrine = NewObject<UDAReplicationDoctrineDefinition>();
    Doctrine->DoctrineId = Manifest.DoctrineId;
    Doctrine->CadenceDevelopmentCycles = Manifest.ReplicationCadenceDevelopmentCycles;
    Doctrine->SourceFingerprint = Manifest.Fingerprint;
    ULevelSequence* Cinematic = nullptr;
    if (!ImportBuildingSources(Manifest, Errors)
        || !GenerateAndSaveShotSequences(ShotList, Manifest.Fingerprint, Errors)
        || !BuildCinematic(ShotList, Cinematic, Errors)
        || !SaveAsset(Quest, Manifest.QuestAssetPath, Manifest.Fingerprint, Errors)
        || !SaveAsset(Doctrine, Manifest.DoctrineAssetPath, Manifest.Fingerprint, Errors)
        || !SaveAsset(Cinematic, Manifest.CinematicAssetPath, Manifest.Fingerprint,
            Errors, ShotList.Fingerprint, TEXT("composite"), ShotList.Fingerprint)
        || !ValidateGenerated(Manifest, Errors))
    {
        for (const FText& Error : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
        return 1;
    }
    return 0;
}
