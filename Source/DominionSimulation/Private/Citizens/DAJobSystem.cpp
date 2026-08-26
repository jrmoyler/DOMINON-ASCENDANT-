#include "Citizens/DAJobSystem.h"

#include "Content/DACardDefinition.h"

#include "Algo/Sort.h"
#include "Economy/DAEconomyTypes.h"

namespace
{
    struct FDACandidateBuckets
    {
        TMap<FName, TArray<int32>> ByCity;
        TMap<FName, TArray<int32>> ByDistrict;
        TMap<FName, TArray<int32>> ByCommuteZone;
        TMap<FName, TArray<int32>> ByClass;
        TMap<FName, TArray<int32>> BySkill;
    };

    void AddBucketMember(TMap<FName, TArray<int32>>& Buckets, const FName Key, const int32 CitizenIndex)
    {
        if (!Key.IsNone())
        {
            Buckets.FindOrAdd(Key).Add(CitizenIndex);
        }
    }

    void AppendUniqueBucket(const TMap<FName, TArray<int32>>& Buckets, const FName Key, TArray<int32>& OutCandidates)
    {
        if (const TArray<int32>* Bucket = Buckets.Find(Key))
        {
            for (const int32 CitizenIndex : *Bucket)
            {
                OutCandidates.AddUnique(CitizenIndex);
            }
        }
    }

    bool HasRequiredSkill(const FDACitizenRecord& Citizen, const FDAJobOpening& Job)
    {
        if (Job.RequiredSkillTags.IsEmpty())
        {
            return true;
        }

        return Job.RequiredSkillTags.ContainsByPredicate([&Citizen](const FName RequiredSkill)
        {
            return Citizen.SkillTags.Contains(RequiredSkill);
        });
    }

    float GetOutputMultiplier(const EDAJobMatchQuality Quality)
    {
        switch (Quality)
        {
        case EDAJobMatchQuality::Excellent:
            return 1.15f;
        case EDAJobMatchQuality::Good:
            return 1.f;
        case EDAJobMatchQuality::Acceptable:
            return 0.85f;
        case EDAJobMatchQuality::Poor:
            return 0.65f;
        case EDAJobMatchQuality::Unqualified:
        default:
            return 0.f;
        }
    }

    FDACitizenRecord MakeNamedCitizen(
        const TCHAR* CitizenId,
        const TCHAR* DisplayName,
        const TCHAR* JobTitle,
        const TCHAR* CityId,
        const TCHAR* CohortId,
        const TCHAR* CitizenClass,
        const TCHAR* Skill,
        const uint32 RegionCode,
        const uint32 RosterIndex)
    {
        FDACitizenRecord Citizen;
        Citizen.CitizenId = FName(CitizenId);
        Citizen.DisplayName = DisplayName;
        Citizen.JobTitle = JobTitle;
        Citizen.CityId = FName(CityId);
        Citizen.CohortId = FName(CohortId);
        Citizen.HouseholdId = FName(*FString::Printf(TEXT("household.named.%02u"), RosterIndex));
        Citizen.HomeAssetId = FGuid(0xDA000001u, RegionCode, RosterIndex, 0xC1712E00u);
        Citizen.DistrictId = FName(*FString::Printf(TEXT("district.%s.residential"), CityId + 5));
        Citizen.CommuteZone = FName(*FString::Printf(TEXT("commute.%s.central"), CityId + 5));
        Citizen.CitizenClass = FName(CitizenClass);
        Citizen.SkillTags.Add(FName(Skill));
        Citizen.EducationLevel = 2;
        Citizen.JobId = FName(*FString::Printf(TEXT("job.named.%02u"), RosterIndex));
        Citizen.bAvailableForWork = true;
        Citizen.bNamed = true;
        Citizen.Representation = EDACitizenRepresentation::Cohort;
        return Citizen;
    }
}

int32 UDAJobSystem::CalculateDefaultWorkforce(const int32 Population)
{
    return FMath::RoundToInt(static_cast<float>(FMath::Max(0, Population)) * 0.65f);
}

int32 UDAJobSystem::CalculateFacilityWorkforceRequirement(
    const int32 BaseRequirement, const UDA_CardDefinition& Definition)
{
    float Modifier = 0.f;
    if (BaseRequirement <= 0
        || !Definition.TryGetWorkforceRequirementModifier(Modifier)
        || !FMath::IsFinite(Modifier)) return FMath::Max(0, BaseRequirement);
    return FMath::Max(0, FMath::RoundToInt(
        static_cast<float>(BaseRequirement) * FMath::Max(0.f, 1.f + Modifier)));
}

FDAJobMatchResult UDAJobSystem::EvaluateMatch(const FDACitizenRecord& Citizen, const FDAJobOpening& Job) const
{
    FDAJobMatchResult Result;
    if (!Citizen.bAvailableForWork
        || Citizen.CityId != Job.CityId
        || Citizen.EducationLevel < Job.MinimumEducationLevel)
    {
        return Result;
    }

    const bool bDistrictMatch = Citizen.DistrictId == Job.DistrictId;
    const bool bCommuteMatch = Citizen.CommuteZone == Job.CommuteZone;
    const bool bClassMatch = Job.PreferredClass.IsNone() || Citizen.CitizenClass == Job.PreferredClass;
    const bool bSkillMatch = HasRequiredSkill(Citizen, Job);

    if (bDistrictMatch && bCommuteMatch && bClassMatch && bSkillMatch)
    {
        Result.Quality = EDAJobMatchQuality::Excellent;
    }
    else if (bCommuteMatch && bClassMatch && bSkillMatch)
    {
        Result.Quality = EDAJobMatchQuality::Good;
    }
    else if (bCommuteMatch && (bClassMatch || bSkillMatch))
    {
        Result.Quality = EDAJobMatchQuality::Acceptable;
    }
    else
    {
        Result.Quality = EDAJobMatchQuality::Poor;
    }
    Result.OutputMultiplier = GetOutputMultiplier(Result.Quality);
    return Result;
}

void UDAJobSystem::ResolveAssignments(FDACitySimulationState& State) const
{
    State.JobAssignments.Reset();
    for (FDACitizenRecord& Citizen : State.Citizens)
    {
        Citizen.JobId = NAME_None;
    }

    FDACandidateBuckets Buckets;
    for (int32 CitizenIndex = 0; CitizenIndex < State.Citizens.Num(); ++CitizenIndex)
    {
        const FDACitizenRecord& Citizen = State.Citizens[CitizenIndex];
        if (!Citizen.bAvailableForWork)
        {
            continue;
        }

        AddBucketMember(Buckets.ByCity, Citizen.CityId, CitizenIndex);
        AddBucketMember(Buckets.ByDistrict, Citizen.DistrictId, CitizenIndex);
        AddBucketMember(Buckets.ByCommuteZone, Citizen.CommuteZone, CitizenIndex);
        AddBucketMember(Buckets.ByClass, Citizen.CitizenClass, CitizenIndex);
        for (const FName Skill : Citizen.SkillTags)
        {
            AddBucketMember(Buckets.BySkill, Skill, CitizenIndex);
        }
    }

    TArray<int32> SortedJobIndices;
    SortedJobIndices.Reserve(State.JobOpenings.Num());
    for (int32 JobIndex = 0; JobIndex < State.JobOpenings.Num(); ++JobIndex)
    {
        SortedJobIndices.Add(JobIndex);
    }
    SortedJobIndices.Sort([&State](const int32 Left, const int32 Right)
    {
        return State.JobOpenings[Left].JobId.LexicalLess(State.JobOpenings[Right].JobId);
    });

    TSet<int32> AssignedCitizenIndices;
    const int32 WorkforceLimit = CalculateDefaultWorkforce(State.Population);
    int32 TotalVacancies = 0;

    for (const int32 JobIndex : SortedJobIndices)
    {
        const FDAJobOpening& Job = State.JobOpenings[JobIndex];
        const int32 Positions = FMath::Max(0, Job.OpenPositions);
        TotalVacancies += Positions;

        for (int32 Position = 0; Position < Positions && State.JobAssignments.Num() < WorkforceLimit; ++Position)
        {
            TArray<int32> CandidateIndices;
            AppendUniqueBucket(Buckets.ByDistrict, Job.DistrictId, CandidateIndices);
            AppendUniqueBucket(Buckets.ByCommuteZone, Job.CommuteZone, CandidateIndices);
            AppendUniqueBucket(Buckets.ByClass, Job.PreferredClass, CandidateIndices);
            for (const FName Skill : Job.RequiredSkillTags)
            {
                AppendUniqueBucket(Buckets.BySkill, Skill, CandidateIndices);
            }
            AppendUniqueBucket(Buckets.ByCity, Job.CityId, CandidateIndices);

            int32 BestCitizenIndex = INDEX_NONE;
            FDAJobMatchResult BestMatch;
            for (const int32 CandidateIndex : CandidateIndices)
            {
                if (AssignedCitizenIndices.Contains(CandidateIndex))
                {
                    continue;
                }

                const FDACitizenRecord& Candidate = State.Citizens[CandidateIndex];
                const FDAJobMatchResult Match = EvaluateMatch(Candidate, Job);
                const bool bBetterQuality = static_cast<uint8>(Match.Quality) > static_cast<uint8>(BestMatch.Quality);
                const bool bStableTieBreak = Match.Quality == BestMatch.Quality
                    && Match.Quality != EDAJobMatchQuality::Unqualified
                    && (BestCitizenIndex == INDEX_NONE
                        || Candidate.CitizenId.LexicalLess(State.Citizens[BestCitizenIndex].CitizenId));
                if (bBetterQuality || bStableTieBreak)
                {
                    BestCitizenIndex = CandidateIndex;
                    BestMatch = Match;
                }
            }

            if (BestCitizenIndex == INDEX_NONE || BestMatch.Quality == EDAJobMatchQuality::Unqualified)
            {
                break;
            }

            FDACitizenRecord& Citizen = State.Citizens[BestCitizenIndex];
            Citizen.JobId = Job.JobId;
            AssignedCitizenIndices.Add(BestCitizenIndex);

            FDAJobAssignment& Assignment = State.JobAssignments.AddDefaulted_GetRef();
            Assignment.CitizenId = Citizen.CitizenId;
            Assignment.JobId = Job.JobId;
            Assignment.FacilityWorldAssetId = Job.FacilityWorldAssetId;
            Assignment.MatchQuality = BestMatch.Quality;
            Assignment.OutputMultiplier = BestMatch.OutputMultiplier;
        }
    }

    State.JobVacancies = FMath::Max(0, TotalVacancies - State.JobAssignments.Num());
}

TArray<FDACitizenRecord> UDAJobSystem::CreateNamedCitizenRoster()
{
    TArray<FDACitizenRecord> Citizens;
    Citizens.Reserve(20);

    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.nia_vale"), TEXT("Nia Vale"), TEXT("junior systems technician"), TEXT("city.synara_frontier"), TEXT("cohort.synara.technical"), TEXT("class.technical"), TEXT("skill.systems"), 1, 1));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.jalen_orr"), TEXT("Jalen Orr"), TEXT("utility engineer"), TEXT("city.synara_frontier"), TEXT("cohort.synara.technical"), TEXT("class.technical"), TEXT("skill.utilities"), 1, 2));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.tomas_rell"), TEXT("Tomas Rell"), TEXT("Corner Exchange operator"), TEXT("city.synara_frontier"), TEXT("cohort.synara.commerce"), TEXT("class.merchant"), TEXT("skill.commerce"), 1, 3));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.maelin_qu"), TEXT("Maelin Qu"), TEXT("Agency Forum organizer"), TEXT("city.synara_frontier"), TEXT("cohort.synara.civic"), TEXT("class.civic"), TEXT("skill.organizing"), 1, 4));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.suri_kade"), TEXT("Suri Kade"), TEXT("research analyst"), TEXT("city.synara_frontier"), TEXT("cohort.synara.research"), TEXT("class.research"), TEXT("skill.analysis"), 1, 5));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.dev_arlen"), TEXT("Dev Arlen"), TEXT("transit technician"), TEXT("city.synara_frontier"), TEXT("cohort.synara.technical"), TEXT("class.technical"), TEXT("skill.transit"), 1, 6));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.synara.ivo_renn"), TEXT("Ivo Renn"), TEXT("Guardian Drone controller"), TEXT("city.synara_frontier"), TEXT("cohort.synara.security"), TEXT("class.security"), TEXT("skill.drones"), 1, 7));

    Citizens.Add(MakeNamedCitizen(TEXT("citizen.forgeweave.mara_kest"), TEXT("Mara Kest"), TEXT("maintenance foreman / worker organizer"), TEXT("city.ironheart"), TEXT("cohort.forgeweave.industrial"), TEXT("class.industrial"), TEXT("skill.maintenance"), 2, 8));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.forgeweave.darek_vol"), TEXT("Darek Vol"), TEXT("foundry chief engineer"), TEXT("city.ironheart"), TEXT("cohort.forgeweave.industrial"), TEXT("class.industrial"), TEXT("skill.engineering"), 2, 9));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.forgeweave.sora_pell"), TEXT("Sora Pell"), TEXT("industrial medic"), TEXT("city.ironheart"), TEXT("cohort.forgeweave.health"), TEXT("class.health"), TEXT("skill.medicine"), 2, 10));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.forgeweave.bren_tal"), TEXT("Bren Tal"), TEXT("freight dispatcher"), TEXT("city.ironheart"), TEXT("cohort.forgeweave.logistics"), TEXT("class.logistics"), TEXT("skill.freight"), 2, 11));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.forgeweave.yara_vennik"), TEXT("Yara Vennik"), TEXT("young Forge Guard veteran"), TEXT("city.ironheart"), TEXT("cohort.forgeweave.security"), TEXT("class.security"), TEXT("skill.guard"), 2, 12));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.forgeweave.olan_grest"), TEXT("Olan Grest"), TEXT("machine-parts merchant"), TEXT("city.ironheart"), TEXT("cohort.forgeweave.commerce"), TEXT("class.merchant"), TEXT("skill.machine_parts"), 2, 13));

    Citizens.Add(MakeNamedCitizen(TEXT("citizen.eden.ori_sen"), TEXT("Ori Sen"), TEXT("hydrologist"), TEXT("city.eden_basin"), TEXT("cohort.eden.restoration"), TEXT("class.research"), TEXT("skill.hydrology"), 3, 14));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.eden.luma_rei"), TEXT("Luma Rei"), TEXT("restoration technician"), TEXT("city.eden_basin"), TEXT("cohort.eden.restoration"), TEXT("class.technical"), TEXT("skill.restoration"), 3, 15));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.eden.kiran_moss"), TEXT("Kiran Moss"), TEXT("regenerative farmer"), TEXT("city.eden_basin"), TEXT("cohort.eden.agriculture"), TEXT("class.agriculture"), TEXT("skill.regenerative_farming"), 3, 16));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.eden.tessa_vahl"), TEXT("Tessa Vahl"), TEXT("ecological council clerk"), TEXT("city.eden_basin"), TEXT("cohort.eden.civic"), TEXT("class.civic"), TEXT("skill.ecological_policy"), 3, 17));

    Citizens.Add(MakeNamedCitizen(TEXT("citizen.neutral.tal_arden"), TEXT("Tal Arden"), TEXT("reservoir settlement elder"), TEXT("city.arden_reservoir"), TEXT("cohort.neutral.reservoir"), TEXT("class.civic"), TEXT("skill.settlement_leadership"), 4, 18));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.neutral.jori_pell"), TEXT("Jori Pell"), TEXT("Ore Station Seven coordinator"), TEXT("city.ore_station_seven"), TEXT("cohort.neutral.ore_station"), TEXT("class.logistics"), TEXT("skill.station_coordination"), 5, 19));
    Citizens.Add(MakeNamedCitizen(TEXT("citizen.neutral.sera_noll"), TEXT("Sera Noll"), TEXT("River Crossing trader"), TEXT("city.river_crossing"), TEXT("cohort.neutral.river_crossing"), TEXT("class.merchant"), TEXT("skill.trade"), 6, 20));

    return Citizens;
}
