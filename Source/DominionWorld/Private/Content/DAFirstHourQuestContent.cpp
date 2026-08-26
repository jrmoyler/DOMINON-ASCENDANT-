#include "Content/DAFirstHourQuestContent.h"
#include "Misc/PackageName.h"

#include "Citizens/DAJobSystem.h"
#include "Content/DAContentManifest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr TCHAR FrozenFingerprint[] = TEXT("2c82c47966c118441564e717007e451326bfdea4");

    TSet<FString> KeySet(std::initializer_list<const TCHAR*> Keys)
    {
        TSet<FString> Result;
        for (const TCHAR* Key : Keys) Result.Add(Key);
        return Result;
    }

    bool ExactKeys(const TSharedPtr<FJsonObject>& Object, const FString& At,
        const TSet<FString>& Required, const TSet<FString>& Allowed, TArray<FText>& Errors)
    {
        if (!Object.IsValid()) { Errors.Add(FText::FromString(At + TEXT(" must be an object."))); return false; }
        bool bValid = true;
        for (const FString& Key : Required) if (!Object->HasField(Key))
        { Errors.Add(FText::FromString(At + TEXT(" is missing '") + Key + TEXT("'."))); bValid = false; }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values) if (!Allowed.Contains(Field.Key))
        { Errors.Add(FText::FromString(At + TEXT(" has unknown key '") + Field.Key + TEXT("'."))); bValid = false; }
        return bValid;
    }

    bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        FString& Out, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || !Object->HasTypedField<EJson::String>(Key))
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be a string."))); return false; }
        Out = Object->GetStringField(Key);
        if (Out.IsEmpty()) { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" cannot be empty."))); return false; }
        return true;
    }

    bool ReadName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        FName& Out, TArray<FText>& Errors)
    {
        FString Value;
        if (!ReadString(Object, Key, At, Value, Errors)) return false;
        Out = FName(*Value); return true;
    }

    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        int32& Out, TArray<FText>& Errors)
    {
        double Value = 0;
        if (!Object.IsValid() || !Object->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value)
            || Value != static_cast<double>(static_cast<int32>(Value)))
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be an integer."))); return false; }
        Out = static_cast<int32>(Value); return true;
    }

    bool ReadNames(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        TArray<FName>& Out, TArray<FText>& Errors)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object.IsValid() || !Object->TryGetArrayField(Key, Values) || Values == nullptr)
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be an array."))); return false; }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            if (!Value.IsValid() || Value->Type != EJson::String || Value->AsString().IsEmpty())
            { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" entries must be strings."))); return false; }
            Out.Add(FName(*Value->AsString()));
        }
        return true;
    }

    FString Escape(const FString& Value)
    {
        FString Out(TEXT("\""));
        for (const TCHAR C : Value)
        {
            switch (C)
            {
            case TEXT('"'): Out += TEXT("\\\""); break;
            case TEXT('\\'): Out += TEXT("\\\\"); break;
            case TEXT('\b'): Out += TEXT("\\b"); break;
            case TEXT('\f'): Out += TEXT("\\f"); break;
            case TEXT('\n'): Out += TEXT("\\n"); break;
            case TEXT('\r'): Out += TEXT("\\r"); break;
            case TEXT('\t'): Out += TEXT("\\t"); break;
            default:
                if (C < 0x20) Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(C));
                else Out.AppendChar(C);
            }
        }
        return Out + TEXT("\"");
    }

    void CanonicalValue(const TSharedPtr<FJsonValue>& Value, FString& Out, const bool bRoot)
    {
        switch (Value->Type)
        {
        case EJson::Null: Out += TEXT("null"); break;
        case EJson::String: Out += Escape(Value->AsString()); break;
        case EJson::Boolean: Out += Value->AsBool() ? TEXT("true") : TEXT("false"); break;
        case EJson::Number:
        {
            const double Number = Value->AsNumber();
            const int64 Integer = static_cast<int64>(Number);
            Out += Number == static_cast<double>(Integer)
                ? FString::Printf(TEXT("%lld"), static_cast<long long>(Integer))
                : FString::Printf(TEXT("%.17g"), Number);
            break;
        }
        case EJson::Array:
        {
            Out += TEXT("[");
            const TArray<TSharedPtr<FJsonValue>>& Values = Value->AsArray();
            for (int32 Index = 0; Index < Values.Num(); ++Index)
            { if (Index) Out += TEXT(","); CanonicalValue(Values[Index], Out, false); }
            Out += TEXT("]"); break;
        }
        case EJson::Object:
        {
            Out += TEXT("{");
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            TArray<FString> Keys; Object->Values.GetKeys(Keys); Keys.Sort();
            bool bFirst = true;
            for (const FString& Key : Keys)
            {
                if (bRoot && Key == TEXT("fingerprint")) continue;
                if (!bFirst) Out += TEXT(","); bFirst = false;
                Out += Escape(Key); Out += TEXT(":"); CanonicalValue(Object->Values[Key], Out, false);
            }
            Out += TEXT("}"); break;
        }
        default: break;
        }
    }

    FString ComputeCanonicalFingerprint(const TSharedPtr<FJsonObject>& Root)
    {
        FString Canonical;
        CanonicalValue(MakeShared<FJsonValueObject>(Root), Canonical, true);
        FTCHARToUTF8 Utf8(*Canonical); uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }

    void AppendProjection(FString& Out, const FString& Value)
    { Out += FString::Printf(TEXT("%d:"), Value.Len()) + Value + TEXT("|"); }
    void AppendProjection(FString& Out, const FName Value) { AppendProjection(Out, Value.ToString()); }
    void AppendProjection(FString& Out, const int64 Value) { Out += FString::Printf(TEXT("%lld|"), static_cast<long long>(Value)); }
    void AppendProjection(FString& Out, const int32 Value) { AppendProjection(Out, static_cast<int64>(Value)); }
    void AppendProjection(FString& Out, const double Value) { Out += FString::Printf(TEXT("%.17g|"), Value); }

    void AppendRewardProjection(FString& Out, const FDAFirstHourReward& Reward)
    {
        AppendProjection(Out, Reward.ActionId); AppendProjection(Out, static_cast<int64>(Reward.Type));
        AppendProjection(Out, Reward.DefinitionId); AppendProjection(Out, Reward.ContentId); AppendProjection(Out, Reward.Quantity);
    }

    FString ComputeQuestProjection(const FDAFirstHourQuestEntry& Quest)
    {
        FString Out; AppendProjection(Out, Quest.Definition.QuestId); AppendProjection(Out, Quest.Title);
        AppendProjection(Out, Quest.AssetPath); AppendProjection(Out, Quest.Definition.SourceDefinitionId);
        AppendProjection(Out, Quest.Definition.Version); AppendProjection(Out, Quest.Definition.StartNodeId);
        AppendProjection(Out, Quest.Definition.BuildManifest().ComputeFingerprint());
        AppendProjection(Out, static_cast<int64>(Quest.StartCondition)); AppendProjection(Out, Quest.PrerequisiteQuestIds.Num());
        for (const FName Prerequisite : Quest.PrerequisiteQuestIds) AppendProjection(Out, Prerequisite);
        AppendProjection(Out, Quest.WorldAssetBindings.Num());
        for (const FDAFirstHourWorldAssetRequirement& Binding : Quest.WorldAssetBindings)
        {
            AppendProjection(Out, Binding.BindingId); AppendProjection(Out, Binding.DefinitionId);
            AppendProjection(Out, static_cast<int64>(Binding.BindWhen)); AppendProjection(Out, Binding.CityId);
            AppendProjection(Out, Binding.OwnerId); AppendProjection(Out, Binding.bRequireOperational ? 1 : 0);
        }
        AppendProjection(Out, Quest.Nodes.Num());
        for (const FDAFirstHourNode& Node : Quest.Nodes)
        {
            const FDAQuestNodeDefinition& Runtime = Node.RuntimeNode;
            AppendProjection(Out, Runtime.NodeId); AppendProjection(Out, static_cast<int64>(Runtime.Type));
            AppendProjection(Out, Runtime.SourceDefinitionId); AppendProjection(Out, static_cast<int64>(Node.Condition));
            AppendProjection(Out, Node.BindingId); AppendProjection(Out, static_cast<int64>(Runtime.Payload.Variant));
            AppendProjection(Out, Runtime.Payload.Timer.DurationWorldTicks); AppendProjection(Out, Runtime.Payload.Condition.EvaluationKey);
            AppendProjection(Out, static_cast<int64>(Runtime.Payload.Condition.Comparison)); AppendProjection(Out, Runtime.Payload.Condition.ExpectedValue);
            AppendProjection(Out, Runtime.Payload.WorldAssetBindingId); AppendProjection(Out, Runtime.Edges.Num());
            for (const FDAQuestEdgeDefinition& Edge : Runtime.Edges)
            {
                AppendProjection(Out, Edge.BranchTag); AppendProjection(Out, Edge.TargetNodeId);
                AppendProjection(Out, static_cast<int64>(Edge.Condition)); AppendProjection(Out, Edge.WorldAssetBindingId);
            }
        }
        AppendProjection(Out, Quest.CompletionHistoryTags.Num()); for (const FName Tag : Quest.CompletionHistoryTags) AppendProjection(Out, Tag);
        AppendProjection(Out, Quest.Rewards.Num()); for (const FDAFirstHourReward& Reward : Quest.Rewards) AppendRewardProjection(Out, Reward);
        TArray<FName> Branches; Quest.Outcomes.GetKeys(Branches); Branches.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
        AppendProjection(Out, Branches.Num());
        for (const FName Branch : Branches)
        {
            const FDAFirstHourOutcome& Outcome = Quest.Outcomes[Branch]; AppendProjection(Out, Branch); AppendProjection(Out, Outcome.ActionId);
            AppendProjection(Out, Outcome.HistoryTags.Num()); for (const FName Tag : Outcome.HistoryTags) AppendProjection(Out, Tag);
            AppendProjection(Out, Outcome.StoryState); AppendProjection(Out, Outcome.SemanticEffects.Num()); for (const FName Effect : Outcome.SemanticEffects) AppendProjection(Out, Effect);
            AppendProjection(Out, Outcome.EligibilityAny.Num()); for (const EDAFirstHourEligibility Eligibility : Outcome.EligibilityAny) AppendProjection(Out, static_cast<int64>(Eligibility));
            AppendProjection(Out, Outcome.bHasDependencyDelta ? 1 : 0); AppendProjection(Out, Outcome.DependencyDelta);
            AppendProjection(Out, Outcome.bHasHumanAgencySupportDelta ? 1 : 0); AppendProjection(Out, Outcome.HumanAgencySupportDelta);
            AppendProjection(Out, Outcome.bHasNiaTrustDelta ? 1 : 0); AppendProjection(Out, Outcome.NiaTrustDelta);
            AppendProjection(Out, Outcome.Rewards.Num()); for (const FDAFirstHourReward& Reward : Outcome.Rewards) AppendRewardProjection(Out, Reward);
        }
        return Out;
    }

    FString ComputeCitizenProjection(const FDAFirstHourCitizenDefinition& Citizen)
    {
        FString Out; AppendProjection(Out, Citizen.CitizenId); AppendProjection(Out, Citizen.DisplayName); AppendProjection(Out, Citizen.AssetPath);
        AppendProjection(Out, Citizen.Origin); AppendProjection(Out, Citizen.Occupation); AppendProjection(Out, Citizen.StartingClass);
        AppendProjection(Out, Citizen.Traits.Num()); for (const FString& Trait : Citizen.Traits) AppendProjection(Out, Trait);
        AppendProjection(Out, Citizen.TowerJobId); AppendProjection(Out, Citizen.ChampionEligibility.RequiredStoryState);
        AppendProjection(Out, Citizen.ChampionEligibility.RequiredCrisisQuestId); AppendProjection(Out, Citizen.ChampionEligibility.RequiredCrisisSourceDefinitionId);
        AppendProjection(Out, Citizen.ChampionEligibility.RequiredCrisisDefinitionFingerprint);
        AppendProjection(Out, Citizen.ChampionEligibility.RequiredCrisisCompletionActionId);
        AppendProjection(Out, Citizen.ChampionEligibility.ChampionDefinitionId); return Out;
    }

    FString HashProjection(const FString& Projection)
    {
        FTCHARToUTF8 Utf8(*Projection); uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }

    FString ComputeSemanticProjectionFingerprint(const FDAFirstHourQuestManifest& Manifest)
    {
        FString Projection; AppendProjection(Projection, Manifest.SchemaVersion); AppendProjection(Projection, Manifest.CampaignId);
        AppendProjection(Projection, Manifest.Quests.Num()); for (const FDAFirstHourQuestEntry& Quest : Manifest.Quests) Projection += ComputeQuestProjection(Quest);
        Projection += ComputeCitizenProjection(Manifest.Citizen); return HashProjection(Projection);
    }

    bool QuestEntriesEqual(const FDAFirstHourQuestEntry& Left, const FDAFirstHourQuestEntry& Right)
    { return ComputeQuestProjection(Left) == ComputeQuestProjection(Right); }

    bool CitizenDefinitionsEqual(const FDAFirstHourCitizenDefinition& Left, const FDAFirstHourCitizenDefinition& Right)
    { return ComputeCitizenProjection(Left) == ComputeCitizenProjection(Right); }

    bool ValidateExactFrozenProjection(const FDAFirstHourQuestManifest& Manifest)
    { return !Manifest.SemanticFingerprint.IsEmpty() && Manifest.SemanticFingerprint == ComputeSemanticProjectionFingerprint(Manifest); }

    template <typename T>
    bool ParseEnum(const FString& Value, const TArray<TPair<const TCHAR*, T>>& Values, T& Out)
    {
        for (const TPair<const TCHAR*, T>& Pair : Values) if (Value == Pair.Key) { Out = Pair.Value; return true; }
        return false;
    }

    bool NodeType(const FString& V, EDAQuestNodeType& Out)
    {
        return ParseEnum(V, TArray<TPair<const TCHAR*, EDAQuestNodeType>>{
            {TEXT("Start"), EDAQuestNodeType::Start}, {TEXT("Dialogue"), EDAQuestNodeType::Dialogue},
            {TEXT("Objective"), EDAQuestNodeType::Objective}, {TEXT("Investigation"), EDAQuestNodeType::Investigation},
            {TEXT("Build"), EDAQuestNodeType::Build}, {TEXT("Explore"), EDAQuestNodeType::Explore},
            {TEXT("Combat"), EDAQuestNodeType::Combat}, {TEXT("Choice"), EDAQuestNodeType::Choice},
            {TEXT("WorldCondition"), EDAQuestNodeType::WorldCondition}, {TEXT("CitizenCondition"), EDAQuestNodeType::CitizenCondition},
            {TEXT("Resolution"), EDAQuestNodeType::Resolution}} , Out);
    }

    bool StartCondition(const FString& V, EDAFirstHourStartCondition& Out)
    {
        return ParseEnum(V, TArray<TPair<const TCHAR*, EDAFirstHourStartCondition>>{
            {TEXT("NewCampaign"), EDAFirstHourStartCondition::NewCampaign}, {TEXT("PrerequisitesMet"), EDAFirstHourStartCondition::PrerequisitesMet},
            {TEXT("HabitatOccupied"), EDAFirstHourStartCondition::HabitatOccupied}, {TEXT("AutonomousExchangeAvailable"), EDAFirstHourStartCondition::AutonomousExchangeAvailable},
            {TEXT("DependencyAbove25"), EDAFirstHourStartCondition::DependencyAbove25}, {TEXT("SixPlayerBuildings"), EDAFirstHourStartCondition::SixPlayerBuildings},
            {TEXT("WorldMapUnlocked"), EDAFirstHourStartCondition::WorldMapUnlocked}, {TEXT("ForgeweaveContact"), EDAFirstHourStartCondition::ForgeweaveContact}}, Out);
    }

    bool Condition(const FString& V, EDAFirstHourNodeCondition& Out)
    {
#define DA_CONDITION(Name) {TEXT(#Name), EDAFirstHourNodeCondition::Name}
        const bool b = ParseEnum(V, TArray<TPair<const TCHAR*, EDAFirstHourNodeCondition>>{
            DA_CONDITION(NewCampaign), DA_CONDITION(PrerequisitesMet), DA_CONDITION(FounderReached), DA_CONDITION(FounderHallPowered),
            DA_CONDITION(FounderHallOnline), DA_CONDITION(CustodianMarkingsInspected), DA_CONDITION(AdaptiveHabitatInspected),
            DA_CONDITION(AdaptiveHabitatPlaced), DA_CONDITION(HabitatConstructionComplete), DA_CONDITION(UtilityExpansionAcknowledged),
            DA_CONDITION(BoundAssetOperational), DA_CONDITION(NiaPresent),
            DA_CONDITION(HabitatOccupied), DA_CONDITION(HabitatPowerFullySupplied), DA_CONDITION(HabitatWaterFullySupplied), DA_CONDITION(NiaSpokenTo),
            DA_CONDITION(TowerHalfStaffed), DA_CONDITION(NiaAssignedToTower), DA_CONDITION(AutonomousExchangeAvailable), DA_CONDITION(ReplacementModelDiscovered),
            DA_CONDITION(ExplicitChoice), DA_CONDITION(DependencyAbove25), DA_CONDITION(WorkersRetrained), DA_CONDITION(AutomationCapEnacted),
            DA_CONDITION(WorldMapUnlocked), DA_CONDITION(ForgeweaveContact), DA_CONDITION(EnteredUtilityTunnel), DA_CONDITION(AncientNodeRestored),
            DA_CONDITION(MaintenanceDronesDefeated), DA_CONDITION(UnknownSymbolInspected), DA_CONDITION(EdenBasinReached), DA_CONDITION(WaterQualityInspected),
            DA_CONDITION(AmaraSpokenTo), DA_CONDITION(OriSpokenTo), DA_CONDITION(Resolved)}, Out);
#undef DA_CONDITION
        return b;
    }

    bool RewardType(const FString& V, EDAFirstHourRewardType& Out)
    {
#define DA_REWARD(Name) {TEXT(#Name), EDAFirstHourRewardType::Name}
        const bool b = ParseEnum(V, TArray<TPair<const TCHAR*, EDAFirstHourRewardType>>{
            DA_REWARD(CityMode), DA_REWARD(CardInstance), DA_REWARD(Blueprint), DA_REWARD(UtilitySystems), DA_REWARD(OperatorXp),
            DA_REWARD(InsightReward), DA_REWARD(IntelligenceAuditorPath), DA_REWARD(AxiomArchiveFragment),
            DA_REWARD(DiplomacyContact), DA_REWARD(EdenTradeAccess)}, Out);
#undef DA_REWARD
        return b;
    }

    bool ParseReward(const TSharedPtr<FJsonObject>& Object, const FString& At, FDAFirstHourReward& Out, TArray<FText>& Errors)
    {
        const TSet<FString> Required = KeySet({TEXT("actionId"), TEXT("type"), TEXT("quantity")});
        ExactKeys(Object, At, Required, KeySet({TEXT("actionId"), TEXT("type"), TEXT("definitionId"), TEXT("contentId"), TEXT("quantity")}), Errors);
        FString Type; ReadName(Object, TEXT("actionId"), At, Out.ActionId, Errors); ReadString(Object, TEXT("type"), At, Type, Errors);
        if (!RewardType(Type, Out.Type)) Errors.Add(FText::FromString(At + TEXT(" has unknown reward type.")));
        ReadInteger(Object, TEXT("quantity"), At, Out.Quantity, Errors);
        if (Object->HasField(TEXT("definitionId"))) ReadName(Object, TEXT("definitionId"), At, Out.DefinitionId, Errors);
        if (Object->HasField(TEXT("contentId"))) ReadName(Object, TEXT("contentId"), At, Out.ContentId, Errors);
        if (Out.Quantity < 1 || Out.Quantity > 100 || Out.DefinitionId.IsNone() == Out.ContentId.IsNone())
            Errors.Add(FText::FromString(At + TEXT(" must contain exactly one definition/content ID and bounded quantity.")));
        return true;
    }

    bool ParseRewards(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        TArray<FDAFirstHourReward>& Out, TArray<FText>& Errors)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Key, Values) || Values == nullptr)
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be an array."))); return false; }
        for (int32 Index = 0; Index < Values->Num(); ++Index)
        {
            if (!(*Values)[Index].IsValid() || (*Values)[Index]->Type != EJson::Object)
            { Errors.Add(FText::FromString(At + TEXT(" reward must be an object."))); continue; }
            ParseReward((*Values)[Index]->AsObject(), FString::Printf(TEXT("%s.%s[%d]"), *At, Key, Index), Out.Emplace_GetRef(), Errors);
        }
        return true;
    }

    bool ParseNode(const TSharedPtr<FJsonObject>& Object, const FString& At, FDAFirstHourNode& Out, TArray<FText>& Errors)
    {
        const TSet<FString> Required = KeySet({TEXT("id"), TEXT("type"), TEXT("sourceDefinitionId"), TEXT("condition"), TEXT("edges")});
        ExactKeys(Object, At, Required, KeySet({TEXT("id"), TEXT("type"), TEXT("sourceDefinitionId"), TEXT("condition"), TEXT("bindingId"), TEXT("edges")}), Errors);
        FString Type, ConditionText;
        ReadName(Object, TEXT("id"), At, Out.RuntimeNode.NodeId, Errors);
        ReadString(Object, TEXT("type"), At, Type, Errors); ReadName(Object, TEXT("sourceDefinitionId"), At, Out.RuntimeNode.SourceDefinitionId, Errors);
        ReadString(Object, TEXT("condition"), At, ConditionText, Errors);
        if (!NodeType(Type, Out.RuntimeNode.Type)) Errors.Add(FText::FromString(At + TEXT(" has unknown node type.")));
        if (!Condition(ConditionText, Out.Condition)) Errors.Add(FText::FromString(At + TEXT(" has unknown concrete condition.")));
        if (Object->HasField(TEXT("bindingId"))) ReadName(Object, TEXT("bindingId"), At, Out.BindingId, Errors);
        if (Out.RuntimeNode.Type == EDAQuestNodeType::WorldCondition) Out.RuntimeNode.Payload.Variant = EDAQuestPayloadVariant::World;
        else if (Out.RuntimeNode.Type == EDAQuestNodeType::CitizenCondition) Out.RuntimeNode.Payload.Variant = EDAQuestPayloadVariant::Citizen;
        if (Out.RuntimeNode.Payload.Variant != EDAQuestPayloadVariant::None)
        { Out.RuntimeNode.Payload.Condition.EvaluationKey = FName(*ConditionText); Out.RuntimeNode.Payload.Condition.ExpectedValue = 1.0; }
        const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
        if (!Object->TryGetArrayField(TEXT("edges"), Edges) || Edges == nullptr) return false;
        for (int32 Index = 0; Index < Edges->Num(); ++Index)
        {
            if (!(*Edges)[Index].IsValid() || (*Edges)[Index]->Type != EJson::Object) { Errors.Add(FText::FromString(At + TEXT(" edge must be an object."))); continue; }
            const TSharedPtr<FJsonObject> Edge = (*Edges)[Index]->AsObject();
            const FString EdgeAt = FString::Printf(TEXT("%s.edges[%d]"), *At, Index);
            ExactKeys(Edge, EdgeAt, KeySet({TEXT("branchTag"), TEXT("targetNodeId")}), KeySet({TEXT("branchTag"), TEXT("targetNodeId")}), Errors);
            FDAQuestEdgeDefinition& RuntimeEdge = Out.RuntimeNode.Edges.Emplace_GetRef();
            ReadName(Edge, TEXT("branchTag"), EdgeAt, RuntimeEdge.BranchTag, Errors); ReadName(Edge, TEXT("targetNodeId"), EdgeAt, RuntimeEdge.TargetNodeId, Errors);
        }
        return true;
    }

    bool ParseOutcome(const TSharedPtr<FJsonObject>& Object, const FString& At, FDAFirstHourOutcome& Out, TArray<FText>& Errors)
    {
        const TSet<FString> Required = KeySet({TEXT("actionId"), TEXT("historyTags"), TEXT("semanticEffects"), TEXT("eligibilityAny"), TEXT("rewards")});
        ExactKeys(Object, At, Required, KeySet({TEXT("actionId"), TEXT("historyTags"), TEXT("storyState"), TEXT("semanticEffects"), TEXT("eligibilityAny"), TEXT("dependencyDelta"), TEXT("humanAgencySupportDelta"), TEXT("niaTrustDelta"), TEXT("rewards")}), Errors);
        ReadName(Object, TEXT("actionId"), At, Out.ActionId, Errors); ReadNames(Object, TEXT("historyTags"), At, Out.HistoryTags, Errors);
        ReadNames(Object, TEXT("semanticEffects"), At, Out.SemanticEffects, Errors);
        if (Object->HasField(TEXT("storyState"))) ReadName(Object, TEXT("storyState"), At, Out.StoryState, Errors);
        TArray<FName> Eligibility; ReadNames(Object, TEXT("eligibilityAny"), At, Eligibility, Errors);
        for (const FName Entry : Eligibility)
        {
            if (Entry == TEXT("Vision")) Out.EligibilityAny.Add(EDAFirstHourEligibility::Vision);
            else if (Entry == TEXT("ResearchAction")) Out.EligibilityAny.Add(EDAFirstHourEligibility::ResearchAction);
            else Errors.Add(FText::FromString(At + TEXT(" contains an unknown eligibility.")));
        }
        if (Object->HasField(TEXT("dependencyDelta"))) { Out.bHasDependencyDelta = ReadInteger(Object, TEXT("dependencyDelta"), At, Out.DependencyDelta, Errors); }
        if (Object->HasField(TEXT("humanAgencySupportDelta"))) { Out.bHasHumanAgencySupportDelta = ReadInteger(Object, TEXT("humanAgencySupportDelta"), At, Out.HumanAgencySupportDelta, Errors); }
        if (Object->HasField(TEXT("niaTrustDelta"))) { Out.bHasNiaTrustDelta = ReadInteger(Object, TEXT("niaTrustDelta"), At, Out.NiaTrustDelta, Errors); }
        TSet<FName> UniqueHistory; for (const FName Tag : Out.HistoryTags) UniqueHistory.Add(Tag);
        TSet<FName> UniqueEffects; for (const FName Effect : Out.SemanticEffects) UniqueEffects.Add(Effect);
        if (Out.SemanticEffects.IsEmpty() || UniqueHistory.Num() != Out.HistoryTags.Num()
            || UniqueEffects.Num() != Out.SemanticEffects.Num() || FMath::Abs(Out.DependencyDelta) > 100
            || FMath::Abs(Out.HumanAgencySupportDelta) > 100 || FMath::Abs(Out.NiaTrustDelta) > 100)
            Errors.Add(FText::FromString(At + TEXT(" requires unique semantic/history IDs and bounded authored deltas.")));
        ParseRewards(Object, TEXT("rewards"), At, Out.Rewards, Errors); return true;
    }
}

const FDAFirstHourNode* FDAFirstHourQuestEntry::FindNode(const FName NodeId) const
{ return Nodes.FindByPredicate([NodeId](const FDAFirstHourNode& Node) { return Node.RuntimeNode.NodeId == NodeId; }); }

const FDAFirstHourWorldAssetRequirement* FDAFirstHourQuestEntry::FindBinding(const FName BindingId) const
{ return WorldAssetBindings.FindByPredicate([BindingId](const FDAFirstHourWorldAssetRequirement& B) { return B.BindingId == BindingId; }); }

const FDAFirstHourQuestEntry* FDAFirstHourQuestManifest::FindQuest(const FName QuestId) const
{ return Quests.FindByPredicate([QuestId](const FDAFirstHourQuestEntry& Quest) { return Quest.Definition.QuestId == QuestId; }); }

FPrimaryAssetId UDA_FirstHourQuestDefinition::GetPrimaryAssetId() const
{ return FPrimaryAssetId(TEXT("DAQuestDefinition"), Quest.Definition.QuestId); }

FPrimaryAssetId UDA_CitizenDefinition::GetPrimaryAssetId() const
{ return FPrimaryAssetId(TEXT("DACitizenDefinition"), Citizen.CitizenId); }

FString FDAFirstHourQuestPipeline::GetCanonicalManifestPath()
{ return FPaths::ProjectContentDir() / TEXT("DA/Manifests/FirstHourQuests.json"); }

bool FDAFirstHourQuestPipeline::LoadCanonical(FDAFirstHourQuestManifest& OutManifest, TArray<FText>& Errors)
{ return LoadFile(GetCanonicalManifestPath(), OutManifest, Errors); }

bool FDAFirstHourQuestPipeline::LoadFile(const FString& Filename, FDAFirstHourQuestManifest& OutManifest, TArray<FText>& Errors)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Filename)) { Errors.Add(FText::FromString(TEXT("Could not load first-hour manifest: ") + Filename)); return false; }
    return ParseJson(Json, OutManifest, Errors);
}

bool FDAFirstHourQuestPipeline::ParseJson(const FString& Json, FDAFirstHourQuestManifest& Out, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num(); Out = FDAFirstHourQuestManifest();
    TSharedPtr<FJsonObject> Root; const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Errors.Add(FText::FromString(TEXT("First-hour manifest is not valid JSON."))); return false; }
    ExactKeys(Root, TEXT("manifest"), KeySet({TEXT("schemaVersion"), TEXT("campaignId"), TEXT("fingerprint"), TEXT("quests"), TEXT("citizen")}),
        KeySet({TEXT("schemaVersion"), TEXT("campaignId"), TEXT("fingerprint"), TEXT("quests"), TEXT("citizen")}), Errors);
    ReadInteger(Root, TEXT("schemaVersion"), TEXT("manifest"), Out.SchemaVersion, Errors);
    ReadName(Root, TEXT("campaignId"), TEXT("manifest"), Out.CampaignId, Errors);
    ReadString(Root, TEXT("fingerprint"), TEXT("manifest"), Out.Fingerprint, Errors);
    const FString Computed = ComputeCanonicalFingerprint(Root);
    if (Out.Fingerprint != Computed || Out.Fingerprint != FrozenFingerprint)
        Errors.Add(FText::FromString(TEXT("Manifest fingerprint does not match canonical JSON or frozen v1.1 authority.")));

    const TArray<TSharedPtr<FJsonValue>>* Quests = nullptr;
    if (!Root->TryGetArrayField(TEXT("quests"), Quests) || Quests == nullptr) Errors.Add(FText::FromString(TEXT("manifest.quests must be an array.")));
    else for (int32 QIndex = 0; QIndex < Quests->Num(); ++QIndex)
    {
        if (!(*Quests)[QIndex].IsValid() || (*Quests)[QIndex]->Type != EJson::Object) { Errors.Add(FText::FromString(TEXT("Quest must be an object."))); continue; }
        const TSharedPtr<FJsonObject> Q = (*Quests)[QIndex]->AsObject(); const FString At = FString::Printf(TEXT("quests[%d]"), QIndex);
        const TSet<FString> QKeys = KeySet({TEXT("id"), TEXT("title"), TEXT("sourceDefinitionId"), TEXT("assetPath"), TEXT("prerequisiteQuestIds"), TEXT("startCondition"), TEXT("worldAssetBindings"), TEXT("nodes"), TEXT("completionHistoryTags"), TEXT("rewards"), TEXT("outcomes")});
        ExactKeys(Q, At, QKeys, QKeys, Errors); FDAFirstHourQuestEntry& Entry = Out.Quests.Emplace_GetRef();
        ReadName(Q, TEXT("id"), At, Entry.Definition.QuestId, Errors); ReadString(Q, TEXT("title"), At, Entry.Title, Errors);
        ReadName(Q, TEXT("sourceDefinitionId"), At, Entry.Definition.SourceDefinitionId, Errors); ReadString(Q, TEXT("assetPath"), At, Entry.AssetPath, Errors);
        ReadNames(Q, TEXT("prerequisiteQuestIds"), At, Entry.PrerequisiteQuestIds, Errors);
        FString Start; ReadString(Q, TEXT("startCondition"), At, Start, Errors); if (!StartCondition(Start, Entry.StartCondition)) Errors.Add(FText::FromString(At + TEXT(" has unknown startCondition.")));
        const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
        if (Q->TryGetArrayField(TEXT("worldAssetBindings"), Bindings) && Bindings) for (int32 Index = 0; Index < Bindings->Num(); ++Index)
        {
            const TSharedPtr<FJsonObject> B = (*Bindings)[Index]->AsObject(); const FString BAt = FString::Printf(TEXT("%s.worldAssetBindings[%d]"), *At, Index);
            const TSet<FString> Keys = KeySet({TEXT("bindingId"), TEXT("definitionId"), TEXT("bindWhen"), TEXT("cityId"), TEXT("ownerId"), TEXT("requireOperational")}); ExactKeys(B, BAt, Keys, Keys, Errors);
            FDAFirstHourWorldAssetRequirement& Binding = Entry.WorldAssetBindings.Emplace_GetRef();
            ReadName(B, TEXT("bindingId"), BAt, Binding.BindingId, Errors); ReadName(B, TEXT("definitionId"), BAt, Binding.DefinitionId, Errors);
            ReadName(B, TEXT("cityId"), BAt, Binding.CityId, Errors); ReadName(B, TEXT("ownerId"), BAt, Binding.OwnerId, Errors);
            FString When; ReadString(B, TEXT("bindWhen"), BAt, When, Errors); Binding.BindWhen = When == TEXT("Start") ? EDAFirstHourBindWhen::Start : EDAFirstHourBindWhen::Objective;
            if (When != TEXT("Start") && When != TEXT("Objective")) Errors.Add(FText::FromString(BAt + TEXT(" has invalid bindWhen.")));
            if (!B->TryGetBoolField(TEXT("requireOperational"), Binding.bRequireOperational)) Errors.Add(FText::FromString(BAt + TEXT(" requireOperational must be boolean.")));
            if (Binding.BindWhen == EDAFirstHourBindWhen::Start
                && (Binding.bRequireOperational || Binding.DefinitionId == TEXT("special.founder_hall")))
                Entry.Definition.RequiredWorldAssetBindingIds.Add(Binding.BindingId);
        }
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (Q->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes) for (int32 Index = 0; Index < Nodes->Num(); ++Index)
        { FDAFirstHourNode& Node = Entry.Nodes.Emplace_GetRef(); ParseNode((*Nodes)[Index]->AsObject(), FString::Printf(TEXT("%s.nodes[%d]"), *At, Index), Node, Errors); Entry.Definition.Nodes.Add(Node.RuntimeNode); }
        if (!Entry.Nodes.IsEmpty()) Entry.Definition.StartNodeId = Entry.Nodes[0].RuntimeNode.NodeId;
        Entry.Definition.Version = 2;
        ReadNames(Q, TEXT("completionHistoryTags"), At, Entry.CompletionHistoryTags, Errors); ParseRewards(Q, TEXT("rewards"), At, Entry.Rewards, Errors);
        const TSharedPtr<FJsonObject>* Outcomes = nullptr;
        if (Q->TryGetObjectField(TEXT("outcomes"), Outcomes) && Outcomes && Outcomes->IsValid()) for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Outcomes)->Values)
        { if (Pair.Value->Type != EJson::Object) Errors.Add(FText::FromString(At + TEXT(" outcome must be an object."))); else ParseOutcome(Pair.Value->AsObject(), At + TEXT(".outcomes.") + Pair.Key, Entry.Outcomes.Add(FName(*Pair.Key)), Errors); }
    }

    const TSharedPtr<FJsonObject>* Citizen = nullptr;
    if (!Root->TryGetObjectField(TEXT("citizen"), Citizen) || Citizen == nullptr || !Citizen->IsValid()) Errors.Add(FText::FromString(TEXT("manifest.citizen must be an object.")));
    else
    {
        const TSet<FString> CKeys = KeySet({TEXT("id"), TEXT("displayName"), TEXT("assetPath"), TEXT("origin"), TEXT("occupation"), TEXT("startingClass"), TEXT("traits"), TEXT("towerJobId"), TEXT("championEligibility")}); ExactKeys(*Citizen, TEXT("citizen"), CKeys, CKeys, Errors);
        ReadName(*Citizen, TEXT("id"), TEXT("citizen"), Out.Citizen.CitizenId, Errors); ReadString(*Citizen, TEXT("displayName"), TEXT("citizen"), Out.Citizen.DisplayName, Errors);
        ReadString(*Citizen, TEXT("assetPath"), TEXT("citizen"), Out.Citizen.AssetPath, Errors); ReadString(*Citizen, TEXT("origin"), TEXT("citizen"), Out.Citizen.Origin, Errors);
        ReadString(*Citizen, TEXT("occupation"), TEXT("citizen"), Out.Citizen.Occupation, Errors); ReadString(*Citizen, TEXT("startingClass"), TEXT("citizen"), Out.Citizen.StartingClass, Errors);
        ReadName(*Citizen, TEXT("towerJobId"), TEXT("citizen"), Out.Citizen.TowerJobId, Errors);
        const TArray<TSharedPtr<FJsonValue>>* Traits = nullptr; if ((*Citizen)->TryGetArrayField(TEXT("traits"), Traits) && Traits) for (const auto& Trait : *Traits) Out.Citizen.Traits.Add(Trait->AsString());
        const TSharedPtr<FJsonObject>* E = nullptr; if ((*Citizen)->TryGetObjectField(TEXT("championEligibility"), E) && E && E->IsValid())
        {
            const TSet<FString> EKeys = KeySet({TEXT("requiredStoryState"), TEXT("requiredCrisisQuestId"), TEXT("requiredCrisisSourceDefinitionId"), TEXT("requiredCrisisDefinitionFingerprint"), TEXT("requiredCrisisCompletionActionId"), TEXT("championDefinitionId")}); ExactKeys(*E, TEXT("citizen.championEligibility"), EKeys, EKeys, Errors);
            ReadName(*E, TEXT("requiredStoryState"), TEXT("eligibility"), Out.Citizen.ChampionEligibility.RequiredStoryState, Errors); ReadName(*E, TEXT("requiredCrisisQuestId"), TEXT("eligibility"), Out.Citizen.ChampionEligibility.RequiredCrisisQuestId, Errors);
            ReadName(*E, TEXT("requiredCrisisSourceDefinitionId"), TEXT("eligibility"), Out.Citizen.ChampionEligibility.RequiredCrisisSourceDefinitionId, Errors);
            ReadString(*E, TEXT("requiredCrisisDefinitionFingerprint"), TEXT("eligibility"), Out.Citizen.ChampionEligibility.RequiredCrisisDefinitionFingerprint, Errors);
            ReadName(*E, TEXT("requiredCrisisCompletionActionId"), TEXT("eligibility"), Out.Citizen.ChampionEligibility.RequiredCrisisCompletionActionId, Errors); ReadName(*E, TEXT("championDefinitionId"), TEXT("eligibility"), Out.Citizen.ChampionEligibility.ChampionDefinitionId, Errors);
        }
    }
    Out.SemanticFingerprint = ComputeSemanticProjectionFingerprint(Out);
    return Errors.Num() == Before && Validate(Out, Errors);
}

bool FDAFirstHourQuestPipeline::Validate(const FDAFirstHourQuestManifest& Manifest, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    if (Manifest.SchemaVersion != 2 || Manifest.CampaignId != TEXT("campaign.vertical_slice.first_hour")
        || Manifest.Fingerprint != FrozenFingerprint
        || Manifest.Fingerprint != FDAFirstHourFrozenPolicy::ManifestFingerprint()
        || Manifest.Quests.Num() != 9
        || !ValidateExactFrozenProjection(Manifest))
        Errors.Add(FText::FromString(TEXT("First-hour manifest identity must match the frozen schema-v2 authority.")));
    static const TArray<FName> QuestIds = {TEXT("quest.wake_the_hall"), TEXT("quest.a_place_to_stay"), TEXT("quest.power_water_people"), TEXT("quest.nia_needs_a_job"), TEXT("quest.replacement_model"), TEXT("quest.agency_has_a_price"), TEXT("quest.signal_in_foundation"), TEXT("quest.iron_at_border"), TEXT("quest.basin_speaks")};
    FDAVerticalSliceContentManifest CardManifest; TArray<FText> CardErrors;
    if (!FDAContentManifestPipeline::LoadCanonical(CardManifest, CardErrors)) Errors.Append(CardErrors);
    TSet<FName> DefinitionIds; for (const FDAManifestCardDefinition& Definition : CardManifest.Definitions) DefinitionIds.Add(Definition.DefinitionId);
    TSet<FName> Actions;
    for (int32 Index = 0; Index < Manifest.Quests.Num(); ++Index)
    {
        const FDAFirstHourQuestEntry& Quest = Manifest.Quests[Index]; FString Error;
        const FDAQuestDefinitionManifest DefinitionManifest = Quest.Definition.BuildManifest();
        if (!QuestIds.IsValidIndex(Index) || Quest.Definition.QuestId != QuestIds[Index]
            || !Quest.Definition.Validate(Error)
            || !FDAFirstHourFrozenPolicy::ValidatePinnedQuestDefinition(DefinitionManifest, Error))
            Errors.Add(FText::FromString(TEXT("Frozen quest order/graph invalid: ") + Error));
        for (const FDAFirstHourWorldAssetRequirement& Binding : Quest.WorldAssetBindings) if (!DefinitionIds.Contains(Binding.DefinitionId) || Binding.CityId != TEXT("player_capital") || Binding.OwnerId != TEXT("civilization.synara")) Errors.Add(FText::FromString(TEXT("Quest binding is not a canonical Task19 player-capital definition.")));
        auto CheckReward = [&DefinitionIds, &Actions, &Errors](const FDAFirstHourReward& Reward)
        { if ((!Reward.DefinitionId.IsNone() && !DefinitionIds.Contains(Reward.DefinitionId)) || Reward.ActionId.IsNone() || Actions.Contains(Reward.ActionId)) Errors.Add(FText::FromString(TEXT("Reward definition/action is invalid or duplicated."))); Actions.Add(Reward.ActionId); };
        for (const FDAFirstHourReward& Reward : Quest.Rewards) CheckReward(Reward);
        for (const TPair<FName, FDAFirstHourOutcome>& Outcome : Quest.Outcomes)
        { if (Outcome.Value.ActionId.IsNone() || Actions.Contains(Outcome.Value.ActionId)) Errors.Add(FText::FromString(TEXT("Outcome action must be globally unique."))); Actions.Add(Outcome.Value.ActionId); for (const FDAFirstHourReward& Reward : Outcome.Value.Rewards) CheckReward(Reward); }
    }
    const bool bRosterContainsNia = UDAJobSystem::CreateNamedCitizenRoster().ContainsByPredicate([](const FDACitizenRecord& C) { return C.CitizenId == TEXT("citizen.synara.nia_vale"); });
    const FDAChampionEligibilityDefinition& E = Manifest.Citizen.ChampionEligibility;
    if (!bRosterContainsNia || Manifest.Citizen.CitizenId != TEXT("citizen.synara.nia_vale") || E.RequiredStoryState != TEXT("story.nia.human_override.supported") || E.RequiredCrisisQuestId != TEXT("quest.human_override") || E.RequiredCrisisSourceDefinitionId != TEXT("quest.human_override.v1") || E.RequiredCrisisDefinitionFingerprint != TEXT("433693a4a1332f736fc8e4a08a565fad") || E.RequiredCrisisCompletionActionId != TEXT("action.quest.human_override.completed"))
        Errors.Add(FText::FromString(TEXT("Nia identity or Human Override gate diverges from canonical citizen/narrative authority.")));
    return Errors.Num() == Before;
}

bool FDAFirstHourQuestPipeline::BuildAssets(const FDAFirstHourQuestManifest& Manifest, TArray<UDA_FirstHourQuestDefinition*>& OutQuests,
    UDA_CitizenDefinition*& OutCitizen, TArray<FText>& Errors)
{
    if (!Validate(Manifest, Errors)) return false; OutQuests.Reset();
    for (const FDAFirstHourQuestEntry& Entry : Manifest.Quests) { auto* Asset = NewObject<UDA_FirstHourQuestDefinition>(GetTransientPackage()); Asset->Quest = Entry; Asset->SourceFingerprint = Manifest.Fingerprint; Asset->bRuntimeManifestFallback = true; OutQuests.Add(Asset); }
    OutCitizen = NewObject<UDA_CitizenDefinition>(GetTransientPackage()); OutCitizen->Citizen = Manifest.Citizen; OutCitizen->SourceFingerprint = Manifest.Fingerprint; OutCitizen->bRuntimeManifestFallback = true; return true;
}

bool FDAFirstHourQuestPipeline::ValidateGeneratedCache(const FDAFirstHourQuestManifest& Manifest,
    const TArray<UDA_FirstHourQuestDefinition*>& Quests, const UDA_CitizenDefinition* Citizen, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num(); if (!Validate(Manifest, Errors)) return false;
    const auto HasExactObjectPath = [&Errors](const UObject* Asset, const FString& PackagePath)
    {
        if (Asset == nullptr) return false;
        if (Asset->GetOutermost() == GetTransientPackage())
        {
            Errors.Add(FText::FromString(TEXT("generated cache object is transient")));
            return false;
        }
        const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
        return Asset->GetOutermost()->GetName() == PackagePath
            && Asset->GetPathName() == PackagePath + TEXT(".") + AssetName;
    };
    if (Quests.Num() != 9 || Citizen == nullptr || Citizen->bRuntimeManifestFallback
        || Citizen->SourceFingerprint != Manifest.Fingerprint
        || !HasExactObjectPath(Citizen, Manifest.Citizen.AssetPath)
        || !CitizenDefinitionsEqual(Citizen->Citizen, Manifest.Citizen))
        Errors.Add(FText::FromString(TEXT("Generated first-hour cache must contain exact Nia semantic/fingerprint parity.")));
    for (int32 Index = 0; Index < Quests.Num() && Index < Manifest.Quests.Num(); ++Index)
        if (Quests[Index] == nullptr || Quests[Index]->bRuntimeManifestFallback
            || Quests[Index]->SourceFingerprint != Manifest.Fingerprint
            || !HasExactObjectPath(Quests[Index], Manifest.Quests[Index].AssetPath)
            || !QuestEntriesEqual(Quests[Index]->Quest, Manifest.Quests[Index]))
            Errors.Add(FText::FromString(TEXT("Generated quest cache full semantic projection mismatch.")));
    return Errors.Num() == Before;
}

bool FDAFirstHourQuestPipeline::BuildRuntimeContent(const FDAFirstHourQuestManifest& Manifest,
    const TArray<UDA_FirstHourQuestDefinition*>& CandidateQuests, UDA_CitizenDefinition* CandidateCitizen,
    FDABuiltFirstHourContent& OutContent, TArray<FText>& Errors)
{
    TArray<FText> CacheErrors;
    if (ValidateGeneratedCache(Manifest, CandidateQuests, CandidateCitizen, CacheErrors)) { OutContent.Quests = CandidateQuests; OutContent.Citizen = CandidateCitizen; return true; }
    OutContent = FDABuiltFirstHourContent();
    return BuildAssets(Manifest, OutContent.Quests, OutContent.Citizen, Errors);
}
