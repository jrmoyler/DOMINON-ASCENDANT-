#include "Content/DARegionalCrisisContent.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr TCHAR FrozenFingerprint[] = TEXT("1bc31247330a8bc0af7103aaa8b70b51d8cd5d7a");
    constexpr TCHAR FrozenSemanticFingerprint[] = TEXT("173bf6f7bad79b10cb6d6fa56ffb18700e9a3f3e");
    TSet<FString> Keys(std::initializer_list<const TCHAR*> Values)
    { TSet<FString> Out; for (const TCHAR* Value : Values) Out.Add(Value); return Out; }
    bool ExactKeys(const TSharedPtr<FJsonObject>& Object, const FString& At,
        const TSet<FString>& Required, const TSet<FString>& Allowed, TArray<FText>& Errors)
    {
        if (!Object.IsValid()) { Errors.Add(FText::FromString(At + TEXT(" must be an object."))); return false; }
        bool bValid = true;
        for (const FString& Key : Required) if (!Object->HasField(Key))
        { Errors.Add(FText::FromString(At + TEXT(" is missing '") + Key + TEXT("'."))); bValid = false; }
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values) if (!Allowed.Contains(Pair.Key))
        { Errors.Add(FText::FromString(At + TEXT(" has unknown key '") + Pair.Key + TEXT("'."))); bValid = false; }
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
    { FString Value; if (!ReadString(Object, Key, At, Value, Errors)) return false; Out = FName(*Value); return true; }
    bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        double& Out, TArray<FText>& Errors)
    {
        if (!Object.IsValid() || !Object->TryGetNumberField(Key, Out) || !FMath::IsFinite(Out))
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be finite numeric data."))); return false; }
        return true;
    }
    bool ReadInteger(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        int32& Out, TArray<FText>& Errors)
    {
        double Value = 0.0;
        if (!ReadNumber(Object, Key, At, Value, Errors) || Value < MIN_int32 || Value > MAX_int32
            || Value != static_cast<double>(static_cast<int64>(Value)))
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be a bounded integer."))); return false; }
        Out = static_cast<int32>(Value); return true;
    }
    bool ReadInteger64(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, const FString& At,
        int64& Out, TArray<FText>& Errors)
    {
        double Value = 0.0;
        // JSON numbers cannot carry every int64 exactly. Authored campaign deltas are deliberately
        // restricted to the lossless IEEE-754 integer range before any narrowing conversion.
        constexpr double MaxExactInteger = 9007199254740991.0;
        if (!ReadNumber(Object, Key, At, Value, Errors)
            || Value < -MaxExactInteger || Value > MaxExactInteger
            || Value != static_cast<double>(static_cast<int64>(Value)))
        { Errors.Add(FText::FromString(At + TEXT(".") + Key + TEXT(" must be a bounded integer."))); return false; }
        Out = static_cast<int64>(Value); return true;
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
            if (C == TEXT('"')) Out += TEXT("\\\""); else if (C == TEXT('\\')) Out += TEXT("\\\\");
            else if (C == TEXT('\n')) Out += TEXT("\\n"); else if (C == TEXT('\r')) Out += TEXT("\\r");
            else if (C == TEXT('\t')) Out += TEXT("\\t"); else Out.AppendChar(C);
        }
        return Out + TEXT("\"");
    }
    void CanonicalValue(const TSharedPtr<FJsonValue>& Value, FString& Out, const bool bRoot)
    {
        if (!Value.IsValid()) return;
        switch (Value->Type)
        {
        case EJson::Null: Out += TEXT("null"); break;
        case EJson::String: Out += Escape(Value->AsString()); break;
        case EJson::Boolean: Out += Value->AsBool() ? TEXT("true") : TEXT("false"); break;
        case EJson::Number:
        {
            const double Number = Value->AsNumber(); const int64 Integer = static_cast<int64>(Number);
            Out += Number == static_cast<double>(Integer) ? FString::Printf(TEXT("%lld"), static_cast<long long>(Integer))
                : FString::Printf(TEXT("%.15g"), Number); break;
        }
        case EJson::Array:
        {
            Out += TEXT("["); const auto& Values = Value->AsArray();
            for (int32 Index = 0; Index < Values.Num(); ++Index)
            { if (Index) Out += TEXT(","); CanonicalValue(Values[Index], Out, false); }
            Out += TEXT("]"); break;
        }
        case EJson::Object:
        {
            Out += TEXT("{"); const TSharedPtr<FJsonObject> Object = Value->AsObject();
            TArray<FString> Names; Object->Values.GetKeys(Names); Names.Sort(); bool bFirst = true;
            for (const FString& Name : Names)
            {
                if (bRoot && Name == TEXT("fingerprint")) continue;
                if (!bFirst) Out += TEXT(","); bFirst = false;
                Out += Escape(Name) + TEXT(":"); CanonicalValue(Object->Values[Name], Out, false);
            }
            Out += TEXT("}"); break;
        }
        default: break;
        }
    }
    FString Fingerprint(const TSharedPtr<FJsonObject>& Root)
    {
        FString Canonical; CanonicalValue(MakeShared<FJsonValueObject>(Root), Canonical, true);
        FTCHARToUTF8 Utf8(*Canonical); uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }
    EDADiplomaticMetric ParseMetric(const FString& Value, bool& bValid)
    {
        bValid = true;
        if (Value == TEXT("trust")) return EDADiplomaticMetric::Trust;
        if (Value == TEXT("respect")) return EDADiplomaticMetric::Respect;
        if (Value == TEXT("fear")) return EDADiplomaticMetric::Fear;
        if (Value == TEXT("dependence")) return EDADiplomaticMetric::Dependence;
        if (Value == TEXT("grievance")) return EDADiplomaticMetric::Grievance;
        if (Value == TEXT("compatibility")) return EDADiplomaticMetric::Compatibility;
        bValid = false; return EDADiplomaticMetric::Trust;
    }
    FString EventProjection(const FDARegionalWorldEventEntry& Event)
    {
        FString Out = Event.EventId.ToString() + TEXT("|") + Event.Title + TEXT("|") + Event.AssetPath
            + TEXT("|") + Event.Scope.ToString();
        Out += FString::Printf(TEXT("|%s|%s|%.17g|%d|%s|%s"), *Event.TriggerMetric.ToString(), *Event.TriggerComparison.ToString(),
            Event.TriggerThreshold, Event.WarningDurationWorldTicks, *Event.InitialStageId.ToString(), *Event.IgnoredResolutionId.ToString());
        for (const auto& Stage : Event.Stages)
        {
            Out += FString::Printf(TEXT("|%s|%d|%.17g"), *Stage.StageId.ToString(), Stage.DurationWorldTicks, Stage.PriceModifier);
            for (const FName Next : Stage.NextStageIds) Out += TEXT("|") + Next.ToString();
        }
        for (const FName System : Event.Systems) Out += TEXT("|") + System.ToString();
        for (const auto& Resolution : Event.Resolutions)
        {
            Out += FString::Printf(TEXT("|%s|%d|%.17g|%lld|%.17g|%s|%d|%.9g|%.9g"), *Resolution.ResolutionId.ToString(),
                Resolution.RecoveryWorldTicks, Resolution.MarketModifier, static_cast<long long>(Resolution.TradeRouteCapacityDelta),
                Resolution.EcologyDelta, *Resolution.RelationshipId.ToString(), static_cast<int32>(Resolution.RelationshipMetric),
                Resolution.RelationshipDelta, Resolution.ResourceHungerDelta);
            for (const FName Tag : Resolution.HistoryTags) Out += TEXT("|") + Tag.ToString();
            TArray<FName> Citizens; Resolution.CitizenOutcomes.GetKeys(Citizens); Citizens.Sort([](FName A, FName B){ return A.LexicalLess(B); });
            for (const FName Citizen : Citizens) Out += TEXT("|") + Citizen.ToString() + TEXT("=") + Resolution.CitizenOutcomes[Citizen].ToString();
        }
        return Out;
    }
    FString QuestProjection(const FDARegionalQuestEntry& Quest)
    {
        FString Out = Quest.QuestId.ToString() + TEXT("|") + Quest.Title + TEXT("|")
            + Quest.AssetPath + TEXT("|") + Quest.Trigger;
        for (const FName Citizen : Quest.CitizenIds) Out += TEXT("|") + Citizen.ToString();
        for (const FName Choice : Quest.Choices) Out += TEXT("|") + Choice.ToString();
        for (const FName System : Quest.Systems) Out += TEXT("|") + System.ToString();
        for (const FName Tag : Quest.OutcomeTags) Out += TEXT("|") + Tag.ToString();
        TArray<FName> ChoiceKeys; Quest.ChoiceOutcomeTags.GetKeys(ChoiceKeys);
        ChoiceKeys.Sort([](FName A, FName B){ return A.LexicalLess(B); });
        for (const FName Choice : ChoiceKeys)
            Out += TEXT("|") + Choice.ToString() + TEXT("=") + Quest.ChoiceOutcomeTags[Choice].ToString();
        for (const FName Condition : Quest.DialogueConditions) Out += TEXT("|") + Condition.ToString();
        for (const FDARegionalQuestNodeEntry& Node : Quest.Nodes)
        {
            Out += TEXT("|") + Node.NodeId.ToString() + TEXT(":") + Node.NodeType.ToString();
            for (const FDARegionalQuestEdgeEntry& Edge : Node.Edges)
                Out += TEXT("|") + Edge.Branch.ToString() + TEXT("->") + Edge.Target.ToString();
        }
        return Out;
    }
    FString SemanticFingerprint(const FDARegionalCrisisManifest& Manifest)
    {
        FString Material;
        for (int32 Index = 0; Index < Manifest.Events.Num(); ++Index)
        { if (Index) Material += TEXT("\n"); Material += EventProjection(Manifest.Events[Index]); }
        Material += TEXT("\n--quests--\n");
        for (int32 Index = 0; Index < Manifest.Quests.Num(); ++Index)
        { if (Index) Material += TEXT("\n"); Material += QuestProjection(Manifest.Quests[Index]); }
        FTCHARToUTF8 Utf8(*Material); uint8 Hash[FSHA1::DigestSize];
        FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash);
        return BytesToHex(Hash, UE_ARRAY_COUNT(Hash)).ToLower();
    }
}

const FDARegionalCrisisResolutionDefinition* FDARegionalWorldEventEntry::FindResolution(const FName ResolutionId) const
{ return Resolutions.FindByPredicate([ResolutionId](const auto& Row){ return Row.ResolutionId == ResolutionId; }); }
const FDARegionalWorldEventEntry* FDARegionalCrisisManifest::FindEvent(const FName EventId) const
{ return Events.FindByPredicate([EventId](const auto& Row){ return Row.EventId == EventId; }); }
const FDARegionalQuestEntry* FDARegionalCrisisManifest::FindQuest(const FName QuestId) const
{ return Quests.FindByPredicate([QuestId](const auto& Row){ return Row.QuestId == QuestId; }); }
FPrimaryAssetId UDARegionalWorldEventDefinition::GetPrimaryAssetId() const
{ return FPrimaryAssetId(TEXT("DARegionalWorldEvent"), Event.EventId); }
FPrimaryAssetId UDARegionalQuestDefinition::GetPrimaryAssetId() const
{ return FPrimaryAssetId(TEXT("DARegionalQuest"), Quest.QuestId); }
FString FDARegionalCrisisPipeline::GetCanonicalManifestPath()
{ return FPaths::ProjectContentDir() / TEXT("DA/Manifests/RegionalCrisisCampaign.json"); }
bool FDARegionalCrisisPipeline::LoadCanonical(FDARegionalCrisisManifest& Out, TArray<FText>& Errors)
{ return LoadFile(GetCanonicalManifestPath(), Out, Errors); }
bool FDARegionalCrisisPipeline::LoadFile(const FString& Filename, FDARegionalCrisisManifest& Out, TArray<FText>& Errors)
{ FString Json; if (!FFileHelper::LoadFileToString(Json, *Filename)) { Errors.Add(FText::FromString(TEXT("Could not load regional crisis manifest."))); return false; } return ParseJson(Json, Out, Errors); }

bool FDARegionalCrisisPipeline::ParseJson(const FString& Json, FDARegionalCrisisManifest& Out, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num(); Out = {};
    TSharedPtr<FJsonObject> Root; const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Errors.Add(FText::FromString(TEXT("Regional crisis manifest is not valid JSON."))); return false; }
    const TSet<FString> RootKeys = Keys({TEXT("schemaVersion"), TEXT("campaignId"), TEXT("fingerprint"), TEXT("events"), TEXT("quests")});
    ExactKeys(Root, TEXT("manifest"), RootKeys, RootKeys, Errors);
    ReadInteger(Root, TEXT("schemaVersion"), TEXT("manifest"), Out.SchemaVersion, Errors);
    ReadName(Root, TEXT("campaignId"), TEXT("manifest"), Out.CampaignId, Errors);
    ReadString(Root, TEXT("fingerprint"), TEXT("manifest"), Out.Fingerprint, Errors);
    if (Out.Fingerprint != Fingerprint(Root) || Out.Fingerprint != FrozenFingerprint)
        Errors.Add(FText::FromString(TEXT("Regional crisis manifest fingerprint diverges from canonical JSON.")));
    const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
    if (!Root->TryGetArrayField(TEXT("events"), Events) || Events == nullptr) Errors.Add(FText::FromString(TEXT("manifest.events must be an array.")));
    else for (int32 Index = 0; Index < Events->Num(); ++Index)
    {
        const FString At = FString::Printf(TEXT("events[%d]"), Index);
        if (!(*Events)[Index].IsValid() || (*Events)[Index]->Type != EJson::Object)
        { Errors.Add(FText::FromString(At + TEXT(" must be an object."))); continue; }
        const TSharedPtr<FJsonObject> Object = (*Events)[Index]->AsObject();
        const TSet<FString> EventKeys = Keys({TEXT("id"), TEXT("title"), TEXT("assetPath"), TEXT("scope"), TEXT("trigger"), TEXT("warningDurationWorldTicks"), TEXT("initialStageId"), TEXT("ignoredResolutionId"), TEXT("stages"), TEXT("systems"), TEXT("resolutions")});
        ExactKeys(Object, At, EventKeys, EventKeys, Errors); auto& Event = Out.Events.Emplace_GetRef();
        ReadName(Object, TEXT("id"), At, Event.EventId, Errors); ReadString(Object, TEXT("title"), At, Event.Title, Errors);
        ReadString(Object, TEXT("assetPath"), At, Event.AssetPath, Errors); ReadName(Object, TEXT("scope"), At, Event.Scope, Errors);
        ReadInteger(Object, TEXT("warningDurationWorldTicks"), At, Event.WarningDurationWorldTicks, Errors);
        ReadName(Object, TEXT("initialStageId"), At, Event.InitialStageId, Errors);
        ReadName(Object, TEXT("ignoredResolutionId"), At, Event.IgnoredResolutionId, Errors);
        ReadNames(Object, TEXT("systems"), At, Event.Systems, Errors);
        const TSharedPtr<FJsonObject>* Trigger = nullptr;
        if (Object->TryGetObjectField(TEXT("trigger"), Trigger) && Trigger && Trigger->IsValid())
        {
            const TSet<FString> TriggerKeys = Keys({TEXT("metric"), TEXT("comparison"), TEXT("threshold")}); ExactKeys(*Trigger, At + TEXT(".trigger"), TriggerKeys, TriggerKeys, Errors);
            ReadName(*Trigger, TEXT("metric"), At, Event.TriggerMetric, Errors); ReadName(*Trigger, TEXT("comparison"), At, Event.TriggerComparison, Errors); ReadNumber(*Trigger, TEXT("threshold"), At, Event.TriggerThreshold, Errors);
        }
        else Errors.Add(FText::FromString(At + TEXT(".trigger must be an object.")));
        const TArray<TSharedPtr<FJsonValue>>* Stages = nullptr;
        if (!Object->TryGetArrayField(TEXT("stages"), Stages) || Stages == nullptr)
            Errors.Add(FText::FromString(At + TEXT(".stages must be an array.")));
        else for (int32 StageIndex = 0; StageIndex < Stages->Num(); ++StageIndex)
        {
            const FString StageAt = FString::Printf(TEXT("%s.stages[%d]"), *At, StageIndex);
            if (!(*Stages)[StageIndex].IsValid() || (*Stages)[StageIndex]->Type != EJson::Object)
            { Errors.Add(FText::FromString(StageAt + TEXT(" must be an object."))); continue; }
            const TSharedPtr<FJsonObject> StageObject = (*Stages)[StageIndex]->AsObject();
            const TSet<FString> StageKeys = Keys({TEXT("id"), TEXT("durationWorldTicks"), TEXT("priceModifier"), TEXT("nextStageIds")}); ExactKeys(StageObject, StageAt, StageKeys, StageKeys, Errors);
            auto& Stage = Event.Stages.Emplace_GetRef(); ReadName(StageObject, TEXT("id"), StageAt, Stage.StageId, Errors);
            ReadInteger(StageObject, TEXT("durationWorldTicks"), StageAt, Stage.DurationWorldTicks, Errors); ReadNumber(StageObject, TEXT("priceModifier"), StageAt, Stage.PriceModifier, Errors);
            ReadNames(StageObject, TEXT("nextStageIds"), StageAt, Stage.NextStageIds, Errors);
        }
        const TArray<TSharedPtr<FJsonValue>>* Resolutions = nullptr;
        if (!Object->TryGetArrayField(TEXT("resolutions"), Resolutions) || Resolutions == nullptr)
            Errors.Add(FText::FromString(At + TEXT(".resolutions must be an array.")));
        else for (int32 RIndex = 0; RIndex < Resolutions->Num(); ++RIndex)
        {
            const FString RAt = FString::Printf(TEXT("%s.resolutions[%d]"), *At, RIndex);
            if (!(*Resolutions)[RIndex].IsValid() || (*Resolutions)[RIndex]->Type != EJson::Object)
            { Errors.Add(FText::FromString(RAt + TEXT(" must be an object."))); continue; }
            const TSharedPtr<FJsonObject> R = (*Resolutions)[RIndex]->AsObject();
            const TSet<FString> RKeys = Keys({TEXT("id"), TEXT("recoveryWorldTicks"), TEXT("marketModifier"), TEXT("tradeRouteCapacityDelta"), TEXT("ecologyDelta"), TEXT("relationshipId"), TEXT("relationshipMetric"), TEXT("relationshipDelta"), TEXT("resourceHungerDelta"), TEXT("historyTags"), TEXT("citizenOutcomes")}); ExactKeys(R, RAt, RKeys, RKeys, Errors);
            auto& Resolution = Event.Resolutions.Emplace_GetRef(); ReadName(R, TEXT("id"), RAt, Resolution.ResolutionId, Errors);
            ReadInteger(R, TEXT("recoveryWorldTicks"), RAt, Resolution.RecoveryWorldTicks, Errors); ReadNumber(R, TEXT("marketModifier"), RAt, Resolution.MarketModifier, Errors);
            ReadInteger64(R, TEXT("tradeRouteCapacityDelta"), RAt, Resolution.TradeRouteCapacityDelta, Errors);
            ReadNumber(R, TEXT("ecologyDelta"), RAt, Resolution.EcologyDelta, Errors); ReadName(R, TEXT("relationshipId"), RAt, Resolution.RelationshipId, Errors);
            FString Metric; ReadString(R, TEXT("relationshipMetric"), RAt, Metric, Errors); bool bMetric = false; Resolution.RelationshipMetric = ParseMetric(Metric, bMetric); if (!bMetric) Errors.Add(FText::FromString(RAt + TEXT(" has unknown relationship metric.")));
            double FloatValue = 0.0; ReadNumber(R, TEXT("relationshipDelta"), RAt, FloatValue, Errors); Resolution.RelationshipDelta = static_cast<float>(FloatValue);
            ReadNumber(R, TEXT("resourceHungerDelta"), RAt, FloatValue, Errors); Resolution.ResourceHungerDelta = static_cast<float>(FloatValue);
            ReadNames(R, TEXT("historyTags"), RAt, Resolution.HistoryTags, Errors);
            const TSharedPtr<FJsonObject>* Outcomes = nullptr;
            if (R->TryGetObjectField(TEXT("citizenOutcomes"), Outcomes) && Outcomes && Outcomes->IsValid())
                for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Outcomes)->Values)
                    if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String || Pair.Value->AsString().IsEmpty()) Errors.Add(FText::FromString(RAt + TEXT(" citizen outcome must be a string.")));
                    else Resolution.CitizenOutcomes.Add(FName(*Pair.Key), FName(*Pair.Value->AsString()));
            else Errors.Add(FText::FromString(RAt + TEXT(" citizenOutcomes must be an object.")));
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* Quests = nullptr;
    if (!Root->TryGetArrayField(TEXT("quests"), Quests) || Quests == nullptr) Errors.Add(FText::FromString(TEXT("manifest.quests must be an array.")));
    else for (int32 Index = 0; Index < Quests->Num(); ++Index)
    {
        const FString At = FString::Printf(TEXT("quests[%d]"), Index);
        if (!(*Quests)[Index].IsValid() || (*Quests)[Index]->Type != EJson::Object)
        { Errors.Add(FText::FromString(At + TEXT(" must be an object."))); continue; }
        const TSharedPtr<FJsonObject> Object = (*Quests)[Index]->AsObject();
        const TSet<FString> QuestKeys = Keys({TEXT("id"), TEXT("title"), TEXT("assetPath"), TEXT("citizenIds"), TEXT("trigger"), TEXT("choices"), TEXT("systems"), TEXT("outcomeTags"), TEXT("choiceOutcomeTags"), TEXT("dialogueConditions"), TEXT("nodes")}); ExactKeys(Object, At, QuestKeys, QuestKeys, Errors);
        auto& Quest = Out.Quests.Emplace_GetRef(); ReadName(Object, TEXT("id"), At, Quest.QuestId, Errors); ReadString(Object, TEXT("title"), At, Quest.Title, Errors);
        ReadString(Object, TEXT("assetPath"), At, Quest.AssetPath, Errors); ReadNames(Object, TEXT("citizenIds"), At, Quest.CitizenIds, Errors);
        ReadString(Object, TEXT("trigger"), At, Quest.Trigger, Errors); ReadNames(Object, TEXT("choices"), At, Quest.Choices, Errors);
        ReadNames(Object, TEXT("systems"), At, Quest.Systems, Errors); ReadNames(Object, TEXT("outcomeTags"), At, Quest.OutcomeTags, Errors); ReadNames(Object, TEXT("dialogueConditions"), At, Quest.DialogueConditions, Errors);
        const TSharedPtr<FJsonObject>* ChoiceOutcomes = nullptr;
        if (Object->TryGetObjectField(TEXT("choiceOutcomeTags"), ChoiceOutcomes) && ChoiceOutcomes && ChoiceOutcomes->IsValid())
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ChoiceOutcomes)->Values)
                if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String || Pair.Value->AsString().IsEmpty())
                    Errors.Add(FText::FromString(At + TEXT(" choice outcome must be a string.")));
                else Quest.ChoiceOutcomeTags.Add(FName(*Pair.Key), FName(*Pair.Value->AsString()));
        else Errors.Add(FText::FromString(At + TEXT(".choiceOutcomeTags must be an object.")));
        const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
        if (!Object->TryGetArrayField(TEXT("nodes"), Nodes) || Nodes == nullptr)
            Errors.Add(FText::FromString(At + TEXT(".nodes must be an array.")));
        else for (int32 NodeIndex = 0; NodeIndex < Nodes->Num(); ++NodeIndex)
        {
            const FString NodeAt = FString::Printf(TEXT("%s.nodes[%d]"), *At, NodeIndex);
            if (!(*Nodes)[NodeIndex].IsValid() || (*Nodes)[NodeIndex]->Type != EJson::Object)
            { Errors.Add(FText::FromString(NodeAt + TEXT(" must be an object."))); continue; }
            const TSharedPtr<FJsonObject> NodeObject = (*Nodes)[NodeIndex]->AsObject();
            const TSet<FString> NodeKeys = Keys({TEXT("id"), TEXT("type"), TEXT("edges")});
            ExactKeys(NodeObject, NodeAt, NodeKeys, NodeKeys, Errors);
            auto& Node = Quest.Nodes.Emplace_GetRef();
            ReadName(NodeObject, TEXT("id"), NodeAt, Node.NodeId, Errors);
            ReadName(NodeObject, TEXT("type"), NodeAt, Node.NodeType, Errors);
            const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
            if (!NodeObject->TryGetArrayField(TEXT("edges"), Edges) || Edges == nullptr)
                Errors.Add(FText::FromString(NodeAt + TEXT(".edges must be an array.")));
            else for (int32 EdgeIndex = 0; EdgeIndex < Edges->Num(); ++EdgeIndex)
            {
                const FString EdgeAt = FString::Printf(TEXT("%s.edges[%d]"), *NodeAt, EdgeIndex);
                if (!(*Edges)[EdgeIndex].IsValid() || (*Edges)[EdgeIndex]->Type != EJson::Object)
                { Errors.Add(FText::FromString(EdgeAt + TEXT(" must be an object."))); continue; }
                const TSharedPtr<FJsonObject> EdgeObject = (*Edges)[EdgeIndex]->AsObject();
                const TSet<FString> EdgeKeys = Keys({TEXT("branch"), TEXT("target")});
                ExactKeys(EdgeObject, EdgeAt, EdgeKeys, EdgeKeys, Errors);
                auto& Edge = Node.Edges.Emplace_GetRef();
                ReadName(EdgeObject, TEXT("branch"), EdgeAt, Edge.Branch, Errors);
                ReadName(EdgeObject, TEXT("target"), EdgeAt, Edge.Target, Errors);
            }
        }
    }
    Out.SemanticFingerprint = SemanticFingerprint(Out);
    return Errors.Num() == Before && Validate(Out, Errors);
}

bool FDARegionalCrisisPipeline::Validate(const FDARegionalCrisisManifest& Manifest, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num();
    static const TArray<FName> EventIds = {TEXT("event.foundry_shortage"), TEXT("event.grid_strain"), TEXT("event.housing_surge"), TEXT("event.green_line"), TEXT("event.corridor_failure"), TEXT("event.migration_wave")};
    static const TArray<FName> QuestIds = {TEXT("quest.tals_reservoir"), TEXT("quest.frontier_claim"), TEXT("quest.hold_the_ridge"), TEXT("quest.first_contract"), TEXT("quest.maras_numbers"), TEXT("quest.green_line"), TEXT("quest.empty_shift"), TEXT("quest.price_of_silence"), TEXT("quest.human_override"), TEXT("quest.foundry_shortage")};
    if (Manifest.SchemaVersion != 1 || Manifest.CampaignId != TEXT("campaign.vertical_slice.regional_crisis")
        || Manifest.Fingerprint != FrozenFingerprint || Manifest.SemanticFingerprint != FrozenSemanticFingerprint
        || SemanticFingerprint(Manifest) != FrozenSemanticFingerprint
        || Manifest.Events.Num() != EventIds.Num() || Manifest.Quests.Num() != QuestIds.Num())
        Errors.Add(FText::FromString(TEXT("Regional crisis manifest identity/count/fingerprint is not canonical.")));
    for (int32 Index = 0; Index < Manifest.Events.Num(); ++Index)
    {
        const FDARegionalWorldEventEntry& Event = Manifest.Events[Index];
        if (!EventIds.IsValidIndex(Index) || Event.EventId != EventIds[Index]
            || Manifest.Events[Index].AssetPath != FString::Printf(TEXT("/Game/DA/Events/E_%s"),
                Index == 0 ? TEXT("FoundryShortage") : Index == 1 ? TEXT("GridStrain") : Index == 2 ? TEXT("HousingSurge") : Index == 3 ? TEXT("GreenLine") : Index == 4 ? TEXT("CorridorFailure") : TEXT("MigrationWave")))
            Errors.Add(FText::FromString(TEXT("Regional event order or expected package path diverges.")));
        if (Event.Stages.IsEmpty() || Event.InitialStageId.IsNone() || Event.IgnoredResolutionId.IsNone()
            || !Event.Stages.ContainsByPredicate([&Event](const FDARegionalCrisisStageDefinition& Stage)
                { return Stage.StageId == Event.InitialStageId; })
            || Event.FindResolution(Event.IgnoredResolutionId) == nullptr)
            Errors.Add(FText::FromString(TEXT("Regional event requires authored initial, staged, and ignored-resolution lifecycle.")));
        for (const FDARegionalCrisisStageDefinition& Stage : Event.Stages)
            for (const FName Next : Stage.NextStageIds)
                if (!Event.Stages.ContainsByPredicate([Next](const FDARegionalCrisisStageDefinition& Row){ return Row.StageId == Next; })
                    && Event.FindResolution(Next) == nullptr)
                    Errors.Add(FText::FromString(TEXT("Regional event graph edge targets foreign stage data.")));
    }
    static const TArray<FString> QuestPaths = {
        TEXT("/Game/DA/Quests/Q_TalsReservoir"),
        TEXT("/Game/DA/Quests/Q_FrontierClaim"),
        TEXT("/Game/DA/Quests/Q_HoldTheRidge"),
        TEXT("/Game/DA/Quests/Q_FirstContract"),
        TEXT("/Game/DA/Quests/Q_MarasNumbers"),
        TEXT("/Game/DA/Quests/Q_GreenLine"),
        TEXT("/Game/DA/Quests/Q_EmptyShift"),
        TEXT("/Game/DA/Quests/Q_PriceOfSilence"),
        TEXT("/Game/DA/Quests/Q_HumanOverride"),
        TEXT("/Game/DA/Quests/Q_FoundryShortage")};
    for (int32 Index = 0; Index < Manifest.Quests.Num(); ++Index)
    {
        if (!QuestIds.IsValidIndex(Index) || Manifest.Quests[Index].QuestId != QuestIds[Index]
            || !QuestPaths.IsValidIndex(Index) || Manifest.Quests[Index].AssetPath != QuestPaths[Index])
            Errors.Add(FText::FromString(TEXT("Regional quest order or expected package path diverges.")));
        const FDARegionalQuestEntry& Quest = Manifest.Quests[Index];
        if (Quest.ChoiceOutcomeTags.Num() != Quest.Choices.Num()
            || !Quest.Nodes.ContainsByPredicate([](const FDARegionalQuestNodeEntry& Node)
                { return Node.NodeId == TEXT("start") && Node.NodeType == TEXT("start"); })
            || !Quest.Nodes.ContainsByPredicate([](const FDARegionalQuestNodeEntry& Node)
                { return Node.NodeId == TEXT("choice") && Node.NodeType == TEXT("choice"); }))
            Errors.Add(FText::FromString(TEXT("Regional quest must contain the complete authored graph and choice outcomes.")));
        for (const FName Choice : Quest.Choices)
            if (!Quest.ChoiceOutcomeTags.Contains(Choice)
                || !Quest.Nodes.ContainsByPredicate([Choice](const FDARegionalQuestNodeEntry& Node)
                    { return Node.NodeId == FName(*(TEXT("resolution.") + Choice.ToString()))
                        && Node.NodeType == TEXT("resolution"); }))
                Errors.Add(FText::FromString(TEXT("Regional quest choice lacks its exact outcome tag or resolution node.")));
    }
    const auto* Foundry = Manifest.FindEvent(TEXT("event.foundry_shortage"));
    static const TArray<FName> ResolutionIds = {TEXT("industrial_support"), TEXT("eden_restriction"), TEXT("brokered_compact"), TEXT("market_exploitation"), TEXT("collapse")};
    if (Foundry == nullptr || Foundry->TriggerMetric != TEXT("forgeweave.resource_hunger") || Foundry->TriggerComparison != TEXT("greater_than")
        || Foundry->TriggerThreshold != 70.0 || Foundry->WarningDurationWorldTicks != 2 || Foundry->Stages.Num() != 4
        || Foundry->Stages[0].PriceModifier != 0.20 || Foundry->Stages[1].PriceModifier != 0.35
        || Foundry->Stages[2].PriceModifier != 0.35 || Foundry->Stages[3].PriceModifier != 0.60
        || Foundry->Resolutions.Num() != 5)
        Errors.Add(FText::FromString(TEXT("Foundry Shortage threshold, explicit timing, stage prices, or resolutions diverge.")));
    else for (int32 Index = 0; Index < Foundry->Resolutions.Num(); ++Index)
    {
        const auto& Resolution = Foundry->Resolutions[Index];
        if (Resolution.ResolutionId != ResolutionIds[Index] || Resolution.RecoveryWorldTicks < 4 || Resolution.RecoveryWorldTicks > 8
            || Resolution.CitizenOutcomes.Num() != 3 || !Resolution.CitizenOutcomes.Contains(TEXT("citizen.neutral.tal_arden"))
            || !Resolution.CitizenOutcomes.Contains(TEXT("citizen.forgeweave.mara_kest")) || !Resolution.CitizenOutcomes.Contains(TEXT("citizen.eden.ori_sen")))
            Errors.Add(FText::FromString(TEXT("Foundry Shortage resolution effects are not the exact authored projection.")));
    }
    return Errors.Num() == Before;
}

bool FDARegionalCrisisPipeline::BuildAssets(const FDARegionalCrisisManifest& Manifest,
    TArray<UDARegionalWorldEventDefinition*>& OutEvents, TArray<UDARegionalQuestDefinition*>& OutQuests, TArray<FText>& Errors)
{
    if (!Validate(Manifest, Errors)) return false; OutEvents.Reset(); OutQuests.Reset();
    for (const auto& Entry : Manifest.Events) { auto* Asset = NewObject<UDARegionalWorldEventDefinition>(GetTransientPackage()); Asset->Event = Entry; Asset->SourceFingerprint = Manifest.Fingerprint; Asset->bRuntimeManifestFallback = true; OutEvents.Add(Asset); }
    for (const auto& Entry : Manifest.Quests) { auto* Asset = NewObject<UDARegionalQuestDefinition>(GetTransientPackage()); Asset->Quest = Entry; Asset->SourceFingerprint = Manifest.Fingerprint; Asset->bRuntimeManifestFallback = true; OutQuests.Add(Asset); }
    return true;
}

bool FDARegionalCrisisPipeline::ValidateGeneratedCache(const FDARegionalCrisisManifest& Manifest,
    const TArray<UDARegionalWorldEventDefinition*>& Events, const TArray<UDARegionalQuestDefinition*>& Quests, TArray<FText>& Errors)
{
    const int32 Before = Errors.Num(); if (!Validate(Manifest, Errors)) return false;
    if (Events.Num() != Manifest.Events.Num() || Quests.Num() != Manifest.Quests.Num()) Errors.Add(FText::FromString(TEXT("Generated regional cache is incomplete.")));
    for (int32 Index = 0; Index < Events.Num() && Index < Manifest.Events.Num(); ++Index)
        if (Events[Index] == nullptr || Events[Index]->bRuntimeManifestFallback || Events[Index]->SourceFingerprint != Manifest.Fingerprint
            || Events[Index]->GetOutermost() == GetTransientPackage()
            || Events[Index]->GetOutermost()->GetName() != Manifest.Events[Index].AssetPath
            || EventProjection(Events[Index]->Event) != EventProjection(Manifest.Events[Index]))
            Errors.Add(FText::FromString(TEXT("Generated regional event cache lacks exact semantic parity.")));
    for (int32 Index = 0; Index < Quests.Num() && Index < Manifest.Quests.Num(); ++Index)
        if (Quests[Index] == nullptr || Quests[Index]->bRuntimeManifestFallback || Quests[Index]->SourceFingerprint != Manifest.Fingerprint
            || Quests[Index]->GetOutermost() == GetTransientPackage()
            || Quests[Index]->GetOutermost()->GetName() != Manifest.Quests[Index].AssetPath
            || QuestProjection(Quests[Index]->Quest) != QuestProjection(Manifest.Quests[Index]))
            Errors.Add(FText::FromString(TEXT("Generated regional quest cache lacks exact semantic parity.")));
    return Errors.Num() == Before;
}
