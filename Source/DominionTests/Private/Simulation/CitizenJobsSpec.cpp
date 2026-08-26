#include "Citizens/DAJobSystem.h"
#include "Algo/Reverse.h"
#include "Economy/DAEconomyTypes.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

BEGIN_DEFINE_SPEC(FDACitizenJobsSpec, "Dominion.Simulation.Citizens.Jobs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDACitizenJobsSpec)

namespace
{
    FDACitizenRecord MakeCandidate(
        const TCHAR* CitizenId,
        const TCHAR* DistrictId,
        const TCHAR* CommuteZone,
        const TCHAR* CitizenClass,
        const TCHAR* Skill,
        const int32 Education = 2)
    {
        FDACitizenRecord Citizen;
        Citizen.CitizenId = FName(CitizenId);
        Citizen.CityId = TEXT("city.test");
        Citizen.DistrictId = FName(DistrictId);
        Citizen.CommuteZone = FName(CommuteZone);
        Citizen.CitizenClass = FName(CitizenClass);
        Citizen.SkillTags.Add(FName(Skill));
        Citizen.EducationLevel = Education;
        Citizen.bAvailableForWork = true;
        return Citizen;
    }

    FDAJobOpening MakeJob(const TCHAR* JobId, const TCHAR* DistrictId = TEXT("district.core"))
    {
        FDAJobOpening Job;
        Job.JobId = FName(JobId);
        Job.CityId = TEXT("city.test");
        Job.DistrictId = FName(DistrictId);
        Job.CommuteZone = TEXT("commute.central");
        Job.PreferredClass = TEXT("class.technical");
        Job.RequiredSkillTags.Add(TEXT("skill.systems"));
        Job.MinimumEducationLevel = 2;
        Job.OpenPositions = 1;
        return Job;
    }
}

void FDACitizenJobsSpec::Define()
{
    It("rounds the default workforce for twenty-four residents to sixteen", [this]()
    {
        TestEqual("24 population yields 16 workers", UDAJobSystem::CalculateDefaultWorkforce(24), 16);
    });

    It("maps excellent good acceptable and poor matches to their frozen output multipliers", [this]()
    {
        UDAJobSystem* Jobs = NewObject<UDAJobSystem>();
        const FDAJobOpening Job = MakeJob(TEXT("job.test.systems"));

        const FDACitizenRecord Excellent = MakeCandidate(
            TEXT("citizen.test.excellent"), TEXT("district.core"), TEXT("commute.central"),
            TEXT("class.technical"), TEXT("skill.systems"));
        const FDACitizenRecord Good = MakeCandidate(
            TEXT("citizen.test.good"), TEXT("district.rim"), TEXT("commute.central"),
            TEXT("class.technical"), TEXT("skill.systems"));
        const FDACitizenRecord Acceptable = MakeCandidate(
            TEXT("citizen.test.acceptable"), TEXT("district.rim"), TEXT("commute.central"),
            TEXT("class.technical"), TEXT("skill.logistics"));
        const FDACitizenRecord Poor = MakeCandidate(
            TEXT("citizen.test.poor"), TEXT("district.rim"), TEXT("commute.remote"),
            TEXT("class.service"), TEXT("skill.logistics"));

        TestEqual("Excellent output", Jobs->EvaluateMatch(Excellent, Job).OutputMultiplier, 1.15f, 0.001f);
        TestEqual("Good output", Jobs->EvaluateMatch(Good, Job).OutputMultiplier, 1.f, 0.001f);
        TestEqual("Acceptable output", Jobs->EvaluateMatch(Acceptable, Job).OutputMultiplier, 0.85f, 0.001f);
        TestEqual("Poor output", Jobs->EvaluateMatch(Poor, Job).OutputMultiplier, 0.65f, 0.001f);
    });

    It("resolves matching deterministically by quality then stable citizen identity", [this]()
    {
        UDAJobSystem* Jobs = NewObject<UDAJobSystem>();
        FDACitySimulationState First;
        First.Population = 24;
        First.Citizens = {
            MakeCandidate(TEXT("citizen.zeta"), TEXT("district.core"), TEXT("commute.central"), TEXT("class.technical"), TEXT("skill.systems")),
            MakeCandidate(TEXT("citizen.alpha"), TEXT("district.core"), TEXT("commute.central"), TEXT("class.technical"), TEXT("skill.systems")),
            MakeCandidate(TEXT("citizen.beta"), TEXT("district.rim"), TEXT("commute.central"), TEXT("class.technical"), TEXT("skill.systems"))
        };
        First.JobOpenings = {MakeJob(TEXT("job.b")), MakeJob(TEXT("job.a"))};

        FDACitySimulationState Second = First;
        Algo::Reverse(Second.Citizens);
        Algo::Reverse(Second.JobOpenings);

        Jobs->ResolveAssignments(First);
        Jobs->ResolveAssignments(Second);

        TestEqual("Two vacancies are filled", First.JobAssignments.Num(), 2);
        TestEqual("Input ordering does not affect assignment count", Second.JobAssignments.Num(), 2);
        if (First.JobAssignments.Num() == 2 && Second.JobAssignments.Num() == 2)
        {
            TestEqual("First stable job goes to first stable excellent citizen", First.JobAssignments[0].JobId, FName(TEXT("job.a")));
            TestEqual("Stable citizen tie-break", First.JobAssignments[0].CitizenId, FName(TEXT("citizen.alpha")));
            TestEqual("Second stable job receives remaining excellent citizen", First.JobAssignments[1].CitizenId, FName(TEXT("citizen.zeta")));
            TestEqual("Reordered input yields identical first assignment", Second.JobAssignments[0].CitizenId, First.JobAssignments[0].CitizenId);
            TestEqual("Reordered input yields identical second assignment", Second.JobAssignments[1].CitizenId, First.JobAssignments[1].CitizenId);
        }
    });

    It("reconciles persistent job identity on every full reassignment", [this]()
    {
        UDAJobSystem* Jobs = NewObject<UDAJobSystem>();
        FDACitySimulationState State;
        State.Population = 24;
        State.Citizens = {
            MakeCandidate(TEXT("citizen.alpha"), TEXT("district.core"), TEXT("commute.central"), TEXT("class.technical"), TEXT("skill.systems")),
            MakeCandidate(TEXT("citizen.beta"), TEXT("district.core"), TEXT("commute.central"), TEXT("class.technical"), TEXT("skill.systems"))
        };
        State.JobOpenings = {MakeJob(TEXT("job.closed_after_first_pass"))};

        Jobs->ResolveAssignments(State);
        TestEqual("First pass assigns the stable first citizen", State.Citizens[0].JobId, FName(TEXT("job.closed_after_first_pass")));

        State.JobOpenings.Reset();
        Jobs->ResolveAssignments(State);

        TestEqual("Closing the job produces no assignments", State.JobAssignments.Num(), 0);
        TestTrue("Closing the job clears its persistent citizen identity", State.Citizens[0].JobId.IsNone());

        State.JobOpenings = {MakeJob(TEXT("job.replacement"))};
        Jobs->ResolveAssignments(State);
        TestEqual("Reopened workforce assigns the replacement job", State.Citizens[0].JobId, FName(TEXT("job.replacement")));

        State.Citizens[0].bAvailableForWork = false;
        Jobs->ResolveAssignments(State);

        TestTrue("Unavailable citizen loses the persistent replacement job", State.Citizens[0].JobId.IsNone());
        TestEqual("Available citizen receives the replacement job", State.Citizens[1].JobId, FName(TEXT("job.replacement")));

        State.Population = 0;
        Jobs->ResolveAssignments(State);

        TestEqual("Zero workforce produces no assignments", State.JobAssignments.Num(), 0);
        TestTrue("Workforce reduction clears the previously assigned persistent job", State.Citizens[1].JobId.IsNone());
    });

    It("seeds exactly the frozen twenty named citizens with stable identities and authored roles", [this]()
    {
        const TArray<FDACitizenRecord> Citizens = UDAJobSystem::CreateNamedCitizenRoster();
        TestEqual("Frozen roster has exactly twenty citizens", Citizens.Num(), 20);

        struct FFrozenCitizen
        {
            const TCHAR* CitizenId;
            const TCHAR* DisplayName;
            const TCHAR* JobTitle;
        };

        const FFrozenCitizen Expected[] = {
            {TEXT("citizen.synara.nia_vale"), TEXT("Nia Vale"), TEXT("junior systems technician")},
            {TEXT("citizen.synara.jalen_orr"), TEXT("Jalen Orr"), TEXT("utility engineer")},
            {TEXT("citizen.synara.tomas_rell"), TEXT("Tomas Rell"), TEXT("Corner Exchange operator")},
            {TEXT("citizen.synara.maelin_qu"), TEXT("Maelin Qu"), TEXT("Agency Forum organizer")},
            {TEXT("citizen.synara.suri_kade"), TEXT("Suri Kade"), TEXT("research analyst")},
            {TEXT("citizen.synara.dev_arlen"), TEXT("Dev Arlen"), TEXT("transit technician")},
            {TEXT("citizen.synara.ivo_renn"), TEXT("Ivo Renn"), TEXT("Guardian Drone controller")},
            {TEXT("citizen.forgeweave.mara_kest"), TEXT("Mara Kest"), TEXT("maintenance foreman / worker organizer")},
            {TEXT("citizen.forgeweave.darek_vol"), TEXT("Darek Vol"), TEXT("foundry chief engineer")},
            {TEXT("citizen.forgeweave.sora_pell"), TEXT("Sora Pell"), TEXT("industrial medic")},
            {TEXT("citizen.forgeweave.bren_tal"), TEXT("Bren Tal"), TEXT("freight dispatcher")},
            {TEXT("citizen.forgeweave.yara_vennik"), TEXT("Yara Vennik"), TEXT("young Forge Guard veteran")},
            {TEXT("citizen.forgeweave.olan_grest"), TEXT("Olan Grest"), TEXT("machine-parts merchant")},
            {TEXT("citizen.eden.ori_sen"), TEXT("Ori Sen"), TEXT("hydrologist")},
            {TEXT("citizen.eden.luma_rei"), TEXT("Luma Rei"), TEXT("restoration technician")},
            {TEXT("citizen.eden.kiran_moss"), TEXT("Kiran Moss"), TEXT("regenerative farmer")},
            {TEXT("citizen.eden.tessa_vahl"), TEXT("Tessa Vahl"), TEXT("ecological council clerk")},
            {TEXT("citizen.neutral.tal_arden"), TEXT("Tal Arden"), TEXT("reservoir settlement elder")},
            {TEXT("citizen.neutral.jori_pell"), TEXT("Jori Pell"), TEXT("Ore Station Seven coordinator")},
            {TEXT("citizen.neutral.sera_noll"), TEXT("Sera Noll"), TEXT("River Crossing trader")}
        };

        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected) && Index < Citizens.Num(); ++Index)
        {
            TestEqual(FString::Printf(TEXT("Citizen %d stable ID"), Index), Citizens[Index].CitizenId, FName(Expected[Index].CitizenId));
            TestEqual(FString::Printf(TEXT("Citizen %d frozen name"), Index), Citizens[Index].DisplayName, FString(Expected[Index].DisplayName));
            TestEqual(FString::Printf(TEXT("Citizen %d authored role"), Index), Citizens[Index].JobTitle, FString(Expected[Index].JobTitle));
            TestTrue(FString::Printf(TEXT("Citizen %d is named"), Index), Citizens[Index].bNamed);
            TestFalse(FString::Printf(TEXT("Citizen %d belongs to a cohort"), Index), Citizens[Index].CohortId.IsNone());
            TestFalse(FString::Printf(TEXT("Citizen %d belongs to a household"), Index), Citizens[Index].HouseholdId.IsNone());
            TestTrue(FString::Printf(TEXT("Citizen %d has a persistent home"), Index), Citizens[Index].HomeAssetId.IsValid());
        }
    });

    It("preserves citizen identity home job and history through representation streaming", [this]()
    {
        FDACitizenRecord Citizen = MakeCandidate(
            TEXT("citizen.test.persistent"), TEXT("district.core"), TEXT("commute.central"),
            TEXT("class.technical"), TEXT("skill.systems"));
        Citizen.HomeAssetId = FGuid(1, 2, 3, 4);
        Citizen.JobId = TEXT("job.test.systems");
        Citizen.History = {TEXT("history.arrived"), TEXT("history.promoted")};

        Citizen.PromoteToNamed(TEXT("Persistent Citizen"));
        Citizen.SetRepresentation(EDACitizenRepresentation::FullActor);
        Citizen.SetRepresentation(EDACitizenRepresentation::Unloaded);
        TestEqual("Unloading does not clear citizen ID", Citizen.CitizenId, FName(TEXT("citizen.test.persistent")));
        TestEqual("Unloading does not clear home", Citizen.HomeAssetId, FGuid(1, 2, 3, 4));
        TestEqual("Unloading does not clear job", Citizen.JobId, FName(TEXT("job.test.systems")));
        const FDACitizenRecord PersistentRecord = Citizen;
        Citizen = PersistentRecord;
        Citizen.SetRepresentation(EDACitizenRepresentation::FullActor);

        TestEqual("Citizen ID survives unload and reload", Citizen.CitizenId, FName(TEXT("citizen.test.persistent")));
        TestEqual("Home survives unload and reload", Citizen.HomeAssetId, FGuid(1, 2, 3, 4));
        TestEqual("Job survives unload and reload", Citizen.JobId, FName(TEXT("job.test.systems")));
        if (TestEqual("History entry count survives unload and reload", Citizen.History.Num(), 2))
        {
            TestEqual("Arrival history survives unload and reload", Citizen.History[0], FName(TEXT("history.arrived")));
            TestEqual("Promotion history survives unload and reload", Citizen.History[1], FName(TEXT("history.promoted")));
        }
        TestEqual("Named display name survives unload and reload", Citizen.DisplayName, FString(TEXT("Persistent Citizen")));
        TestTrue("Promotion remains persistent", Citizen.bNamed);
    });
}
