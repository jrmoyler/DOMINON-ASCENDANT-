#include "LOD/DASimulationLODSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    DA::SimulationLOD::Context ToPolicyContext(const FDASimulationLODContext& Context)
    {
        return {
            Context.bFounderDistrict,
            Context.bCombat,
            Context.bCurrentCity,
            Context.bNearbyDistrict,
            Context.bRegionLoaded,
        };
    }

    bool IsFinitePositive(const double Value)
    {
        return FMath::IsFinite(Value) && Value > 0.0;
    }

    bool IsSha256(const FString& Value)
    {
        if (Value.Len() != 64) return false;
        for (const TCHAR Character : Value)
            if ((Character < TEXT('0') || Character > TEXT('9'))
                && (Character < TEXT('a') || Character > TEXT('f'))) return false;
        return true;
    }

    FString Sha256Hex(const TArray<uint8>& Bytes)
    {
        uint8 Digest[32] = {};
        FSHA256 Hasher;
        Hasher.Update(Bytes.GetData(), static_cast<uint64>(Bytes.Num()));
        Hasher.Final();
        Hasher.GetHash(Digest);
        FString Result;
        Result.Reserve(64);
        for (const uint8 Byte : Digest) Result += FString::Printf(TEXT("%02x"), Byte);
        return Result;
    }

    TSharedRef<FJsonObject> CommandCandidate()
    {
        TSharedRef<FJsonObject> Command = MakeShared<FJsonObject>();
        Command->SetArrayField(TEXT("argv"), {});
        Command->SetField(TEXT("exitCode"), MakeShared<FJsonValueNull>());
        return Command;
    }
}

FDASimulationLODContext FDASimulationLODContext::FounderDistrict(const bool bCombat)
{
    FDASimulationLODContext Result;
    Result.bFounderDistrict = true;
    Result.bCombat = bCombat;
    Result.bCurrentCity = true;
    Result.bNearbyDistrict = true;
    Result.bRegionLoaded = true;
    return Result;
}

FDASimulationLODContext FDASimulationLODContext::NearbyDistrict()
{
    FDASimulationLODContext Result;
    Result.bCurrentCity = true;
    Result.bNearbyDistrict = true;
    Result.bRegionLoaded = true;
    return Result;
}

FDASimulationLODContext FDASimulationLODContext::DistantActiveCity()
{
    FDASimulationLODContext Result;
    Result.bCurrentCity = true;
    Result.bRegionLoaded = true;
    return Result;
}

FDASimulationLODContext FDASimulationLODContext::UnloadedMajorRegion()
{
    return {};
}

EDASimulationLOD UDASimulationLODSubsystem::ResolveLOD(
    const FDASimulationLODContext& Context)
{
    return static_cast<EDASimulationLOD>(DA::SimulationLOD::Resolve(ToPolicyContext(Context)));
}

EDACitizenLODRepresentation UDASimulationLODSubsystem::ResolveRepresentation(
    const EDASimulationLOD LOD, const bool bNamedCitizen)
{
    return static_cast<EDACitizenLODRepresentation>(DA::SimulationLOD::RepresentationFor(
        static_cast<DA::SimulationLOD::Level>(LOD), bNamedCitizen));
}

bool UDASimulationLODSubsystem::RegisterNamedCitizen(const FName CitizenId)
{
    if (CitizenId.IsNone() || NamedCitizens.Contains(CitizenId)) return false;
    FDANamedCitizenLODState State;
    State.CitizenId = CitizenId;
    NamedCitizens.Add(CitizenId, MoveTemp(State));
    return true;
}

bool UDASimulationLODSubsystem::TransitionNamedCitizen(
    const FName CitizenId, const FDASimulationLODContext& Context)
{
    FDANamedCitizenLODState* State = NamedCitizens.Find(CitizenId);
    if (State == nullptr || State->CitizenId != CitizenId) return false;
    State->LOD = ResolveLOD(Context);
    State->Representation = ResolveRepresentation(State->LOD, true);
    return true;
}

bool UDASimulationLODSubsystem::RebuildNamedCitizensFromCampaign(
    const FDACampaignLiveSignalState& Signals, const FDASimulationLODContext& Context)
{
    FString Error;
    if (!Signals.Validate(Error)) return false;
    TMap<FName, FDANamedCitizenLODState> Candidate;
    const EDASimulationLOD LOD = ResolveLOD(Context);
    for (const FDACampaignCitizenSignal& Citizen : Signals.Citizens)
    {
        FDANamedCitizenLODState State;
        State.CitizenId = Citizen.CitizenId;
        State.LOD = LOD;
        State.Representation = ResolveRepresentation(LOD, true);
        Candidate.Add(State.CitizenId, MoveTemp(State));
    }
    NamedCitizens = MoveTemp(Candidate);
    return true;
}

const FDANamedCitizenLODState* UDASimulationLODSubsystem::FindNamedCitizen(
    const FName CitizenId) const
{
    return NamedCitizens.Find(CitizenId);
}

void UDASimulationLODSubsystem::BeginBenchmarkCapture()
{
    bCaptureActive = true;
    FrameSampleCount = 0;
    DevelopmentCycleSampleCount = 0;
    ModeTransitionSampleCount = 0;
    TotalFrameSeconds = 0.0;
    WorstFrameSeconds = 0.0;
    WorstDevelopmentCycleSeconds = 0.0;
    WorstModeTransitionSeconds = 0.0;
    ModeTransitionSamples.Reset();
}

bool UDASimulationLODSubsystem::RecordFrameSeconds(const double Seconds)
{
    if (!bCaptureActive || !IsFinitePositive(Seconds)) return false;
    ++FrameSampleCount;
    TotalFrameSeconds += Seconds;
    WorstFrameSeconds = FMath::Max(WorstFrameSeconds, Seconds);
    return FMath::IsFinite(TotalFrameSeconds);
}

bool UDASimulationLODSubsystem::RecordDevelopmentCycleSeconds(const double Seconds)
{
    if (!bCaptureActive || !IsFinitePositive(Seconds)) return false;
    ++DevelopmentCycleSampleCount;
    WorstDevelopmentCycleSeconds = FMath::Max(WorstDevelopmentCycleSeconds, Seconds);
    return true;
}

bool UDASimulationLODSubsystem::RecordModeTransitionSeconds(const double Seconds)
{
    if (!bCaptureActive || !IsFinitePositive(Seconds)) return false;
    ++ModeTransitionSampleCount;
    WorstModeTransitionSeconds = FMath::Max(WorstModeTransitionSeconds, Seconds);
    ModeTransitionSamples.Add(Seconds);
    return true;
}

FDABenchmarkSummary UDASimulationLODSubsystem::FinishBenchmarkCapture(
    const bool bReferenceClassHardware)
{
    FDABenchmarkSummary Summary;
    Summary.FrameSampleCount = FrameSampleCount;
    Summary.DevelopmentCycleSampleCount = DevelopmentCycleSampleCount;
    Summary.bReferenceClassHardware = bReferenceClassHardware;
    if (!bCaptureActive || FrameSampleCount == 0)
    {
        return Summary;
    }

    bCaptureActive = false;
    Summary.State = EDABenchmarkEvidenceState::Captured;
    Summary.AverageFramesPerSecond = TotalFrameSeconds > 0.0
        ? static_cast<double>(FrameSampleCount) / TotalFrameSeconds : 0.0;
    Summary.WorstFrameFramesPerSecond = WorstFrameSeconds > 0.0
        ? 1.0 / WorstFrameSeconds : 0.0;
    Summary.WorstDevelopmentCycleMilliseconds = WorstDevelopmentCycleSeconds * 1000.0;
    Summary.WorstModeTransitionSeconds = WorstModeTransitionSeconds;
    Summary.ModeTransitionSeconds = ModeTransitionSamples;
    constexpr double MeasurementTolerance = 0.0001;
    Summary.bPerformanceTargetsPassed = bReferenceClassHardware
        && FrameSampleCount >= MinimumBenchmarkFrameSamples
        && DevelopmentCycleSampleCount >= 1
        && ModeTransitionSampleCount == 3
        && Summary.AverageFramesPerSecond + MeasurementTolerance >= TargetFramesPerSecond
        && Summary.WorstFrameFramesPerSecond + MeasurementTolerance >= FallbackFramesPerSecond
        && Summary.WorstDevelopmentCycleMilliseconds <= SevereHitchThresholdMilliseconds
        && Summary.WorstModeTransitionSeconds < TransitionBudgetSeconds;
    return Summary;
}

FDABenchmarkHardwareCapture FDABenchmarkHardwareCapture::Detect(
    const FString& VerifiedStorageClass)
{
    FDABenchmarkHardwareCapture Result;
    const FString ActualCpu = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
    const FString ActualGpu = FPlatformMisc::GetPrimaryGPUBrand().TrimStartAndEnd();
    Result.Cpu = ActualCpu.Contains(TEXT("Ryzen 5 5600"), ESearchCase::IgnoreCase)
        ? TEXT("Ryzen 5 5600 or equivalent") : ActualCpu;
    Result.Gpu = ActualGpu.Contains(TEXT("RTX 3060"), ESearchCase::IgnoreCase)
            || ActualGpu.Contains(TEXT("RX 6700 XT"), ESearchCase::IgnoreCase)
        ? TEXT("RTX 3060 or RX 6700 XT class") : ActualGpu;
    Result.SystemMemoryGiB = static_cast<int32>(FPlatformMemory::GetConstants().TotalPhysicalGB);
    Result.Storage = VerifiedStorageClass.IsEmpty() ? TEXT("UNVERIFIED") : VerifiedStorageClass;
    return Result;
}

bool FDABenchmarkEvidenceCapture::WriteCandidate(const FString& EvidencePath,
    const FString& PlanPath, FString& OutError) const
{
    const bool bValidPerformance = Performance.State == EDABenchmarkEvidenceState::Captured
        && Performance.FrameSampleCount >= UDASimulationLODSubsystem::MinimumBenchmarkFrameSamples
        && Performance.DevelopmentCycleSampleCount == 1000
        && Performance.ModeTransitionSeconds.Num() == 3
        && FMath::IsFinite(Performance.AverageFramesPerSecond)
        && Performance.AverageFramesPerSecond > 0.0
        && FMath::IsFinite(Performance.WorstFrameFramesPerSecond)
        && Performance.WorstFrameFramesPerSecond > 0.0
        && FMath::IsFinite(Performance.WorstDevelopmentCycleMilliseconds)
        && Performance.WorstDevelopmentCycleMilliseconds >= 0.0
        && Performance.ModeTransitionSeconds.ContainsByPredicate(
            [](const double Value) { return !FMath::IsFinite(Value) || Value < 0.0; }) == false;
    const bool bValidSoak = Soak.DevelopmentCycles == 1000 && Soak.EnabledEventCount == 6
        && Soak.PlayerEconomyDevelopmentCycles == 1000
        && Soak.TradeWorldTicks == 200 && Soak.ForgeweaveWorldTicks == 200
        && Soak.UtilityTopologyNodeCount >= 2 && Soak.bUtilityTopologyResolved
        && Soak.bPopulationStayedNonnegative && Soak.EventEmissionCount >= 0
        && Soak.EventEmissionCount <= 6 && Soak.EventTransitionCount >= 0
        && !Soak.bEventRunawayDetected && !Soak.bStaleRequiredQuestDetected
        && !Soak.bRequiredQuestResolutionFailed
        && Soak.InitialSaveBytes >= 0 && Soak.FinalSaveBytes >= 0
        && IsSha256(Soak.RepeatabilityDigestA) && IsSha256(Soak.RepeatabilityDigestB);
    if (!bValidPerformance || !bValidSoak || Hardware.Cpu.IsEmpty() || Hardware.Gpu.IsEmpty()
        || Hardware.SystemMemoryGiB <= 0 || Hardware.Storage.IsEmpty())
    {
        OutError = TEXT("Benchmark candidate requires complete finite measured instrumentation, canonical soak, and hardware.");
        return false;
    }
    TArray<uint8> PlanBytes;
    if (!FFileHelper::LoadFileToArray(PlanBytes, *PlanPath) || PlanBytes.IsEmpty())
    {
        OutError = TEXT("Benchmark candidate cannot hash the exact runner plan bytes.");
        return false;
    }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("$schema"), TEXT("PerformanceEvidence.schema.json"));
    Root->SetNumberField(TEXT("schemaVersion"), 1);
    Root->SetStringField(TEXT("sceneId"), TEXT("benchmark.vertical_slice.synara_capital"));
    Root->SetStringField(TEXT("planSha256"), Sha256Hex(PlanBytes));
    Root->SetStringField(TEXT("state"), TEXT("CANDIDATE"));
    Root->SetArrayField(TEXT("readinessReasons"), {
        MakeShared<FJsonValueString>(TEXT("Awaiting runner command reconciliation."))});

    TSharedRef<FJsonObject> HardwareJson = MakeShared<FJsonObject>();
    HardwareJson->SetStringField(TEXT("cpu"), Hardware.Cpu);
    HardwareJson->SetStringField(TEXT("gpu"), Hardware.Gpu);
    HardwareJson->SetNumberField(TEXT("systemMemoryGiB"), Hardware.SystemMemoryGiB);
    HardwareJson->SetStringField(TEXT("storage"), Hardware.Storage);
    Root->SetObjectField(TEXT("hardware"), HardwareJson);

    TSharedRef<FJsonObject> Commands = MakeShared<FJsonObject>();
    Commands->SetObjectField(TEXT("prepare"), CommandCandidate());
    Commands->SetObjectField(TEXT("automation"), CommandCandidate());
    Commands->SetObjectField(TEXT("cook"), CommandCandidate());
    Root->SetObjectField(TEXT("commands"), Commands);

    TSharedRef<FJsonObject> Measurements = MakeShared<FJsonObject>();
    Measurements->SetNumberField(TEXT("frameSampleCount"), Performance.FrameSampleCount);
    Measurements->SetNumberField(TEXT("averageFramesPerSecond"), Performance.AverageFramesPerSecond);
    Measurements->SetNumberField(TEXT("worstFrameFramesPerSecond"), Performance.WorstFrameFramesPerSecond);
    Measurements->SetNumberField(TEXT("worstDevelopmentCycleMilliseconds"),
        Performance.WorstDevelopmentCycleMilliseconds);
    TArray<TSharedPtr<FJsonValue>> Transitions;
    for (const double Seconds : Performance.ModeTransitionSeconds)
        Transitions.Add(MakeShared<FJsonValueNumber>(Seconds));
    Measurements->SetArrayField(TEXT("modeTransitionSeconds"), Transitions);
    Measurements->SetNumberField(TEXT("developmentCycles"), Soak.DevelopmentCycles);
    Measurements->SetNumberField(TEXT("enabledEventCount"), Soak.EnabledEventCount);
    Measurements->SetNumberField(TEXT("playerEconomyDevelopmentCycles"),
        Soak.PlayerEconomyDevelopmentCycles);
    Measurements->SetNumberField(TEXT("tradeWorldTicks"), Soak.TradeWorldTicks);
    Measurements->SetNumberField(TEXT("forgeweaveWorldTicks"), Soak.ForgeweaveWorldTicks);
    Measurements->SetNumberField(TEXT("utilityTopologyNodeCount"), Soak.UtilityTopologyNodeCount);
    Measurements->SetBoolField(TEXT("utilityTopologyResolved"), Soak.bUtilityTopologyResolved);
    Measurements->SetBoolField(TEXT("populationStayedNonnegative"),
        Soak.bPopulationStayedNonnegative);
    Measurements->SetNumberField(TEXT("eventEmissionCount"), Soak.EventEmissionCount);
    Measurements->SetNumberField(TEXT("eventTransitionCount"), Soak.EventTransitionCount);
    Measurements->SetBoolField(TEXT("eventRunawayDetected"), Soak.bEventRunawayDetected);
    Measurements->SetBoolField(TEXT("staleRequiredQuestDetected"),
        Soak.bStaleRequiredQuestDetected);
    Measurements->SetBoolField(TEXT("requiredQuestResolutionFailed"),
        Soak.bRequiredQuestResolutionFailed);
    Measurements->SetNumberField(TEXT("initialSaveBytes"), static_cast<double>(Soak.InitialSaveBytes));
    Measurements->SetNumberField(TEXT("finalSaveBytes"), static_cast<double>(Soak.FinalSaveBytes));
    Measurements->SetStringField(TEXT("repeatabilityDigestA"), Soak.RepeatabilityDigestA);
    Measurements->SetStringField(TEXT("repeatabilityDigestB"), Soak.RepeatabilityDigestB);
    Root->SetObjectField(TEXT("measurements"), Measurements);

    TSharedRef<FJsonObject> Claims = MakeShared<FJsonObject>();
    Claims->SetBoolField(TEXT("performanceTargetsPassed"), false);
    Claims->SetBoolField(TEXT("soakPassed"), false);
    Claims->SetBoolField(TEXT("cookPassed"), false);
    Root->SetObjectField(TEXT("claims"), Claims);

    FString Document;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Document);
    if (!FJsonSerializer::Serialize(Root, Writer)
        || !FFileHelper::SaveStringToFile(Document + TEXT("\n"), *EvidencePath))
    {
        OutError = TEXT("Benchmark candidate could not be serialized.");
        return false;
    }
    OutError.Reset();
    return true;
}

FDABenchmarkSceneLoad FDABenchmarkSceneLoad::VerticalSlice()
{
    FDABenchmarkSceneLoad Result;
    Result.GameplayAssetCount = 50;
    Result.CitizensByLOD = {30, 30, 40, 20};
    Result.bWeatherActive = true;
    Result.AlliedSquadCount = 3;
    Result.EnemySquadCount = 3;
    Result.VehicleCount = 1;
    Result.ConstructionEffectCount = 1;
    Result.DamagedBuildingCount = 1;
    Result.ModeTransition = {TEXT("City"), TEXT("Command"), TEXT("Founder")};
    return Result;
}

int32 FDABenchmarkSceneLoad::GetCitizenCount() const
{
    int32 Total = 0;
    for (const int32 Count : CitizensByLOD) Total += Count;
    return Total;
}

bool FDABenchmarkSceneLoad::Validate(FString& OutError) const
{
    if (GameplayAssetCount != 50 || CitizensByLOD.Num() != 4
        || CitizensByLOD.ContainsByPredicate([](const int32 Count) { return Count <= 0; })
        || GetCitizenCount() != 120 || !bWeatherActive
        || AlliedSquadCount != 3 || EnemySquadCount != 3 || VehicleCount != 1
        || ConstructionEffectCount != 1 || DamagedBuildingCount != 1
        || ModeTransition != TArray<FName>({TEXT("City"), TEXT("Command"), TEXT("Founder")}))
    {
        OutError = TEXT("Benchmark scene must retain the exact v1.1 vertical-slice load.");
        return false;
    }
    OutError.Reset();
    return true;
}
