#include "Narrative/DANarrativeRecords.h"

#include "Containers/StringConv.h"
#include "Misc/SecureHash.h"
#include "World/DAWorldAssetRecord.h"

namespace
{
    FString GuidKey(const FGuid& Guid) { return Guid.ToString(EGuidFormats::Digits); }

    template <typename TValue, typename TKey, typename TProjection>
    bool HasUniqueValidKeys(const TArray<TValue>& Values, TSet<TKey>& OutKeys, TProjection Projection)
    {
        for (const TValue& Value : Values)
        {
            const TKey Key = Projection(Value);
            if (Key == TKey() || OutKeys.Contains(Key)) return false;
            OutKeys.Add(Key);
        }
        return true;
    }

    bool HasUniqueNames(const TArray<FName>& Names, const bool bMayBeEmpty)
    {
        if (!bMayBeEmpty && Names.IsEmpty()) return false;
        TSet<FName> Seen;
        for (const FName Name : Names)
        {
            if (Name.IsNone() || Seen.Contains(Name)) return false;
            Seen.Add(Name);
        }
        return true;
    }

    bool NamesAreNormalized(const TArray<FName>& Names)
    {
        if (!HasUniqueNames(Names, false)) return false;
        for (int32 Index = 1; Index < Names.Num(); ++Index)
        {
            if (!Names[Index - 1].LexicalLess(Names[Index])) return false;
        }
        return true;
    }

    bool GuidsAreUniqueAndSorted(const TArray<FGuid>& Guids)
    {
        TSet<FGuid> Seen;
        FString Previous;
        for (const FGuid& Guid : Guids)
        {
            const FString Current = GuidKey(Guid);
            if (!Guid.IsValid() || Seen.Contains(Guid) || (!Previous.IsEmpty() && !(Previous < Current))) return false;
            Seen.Add(Guid);
            Previous = Current;
        }
        return true;
    }

    bool ContainsWorldAsset(const TArray<FDAWorldAssetRecord>& WorldAssets, const FGuid WorldAssetId)
    {
        return WorldAssets.ContainsByPredicate(
            [WorldAssetId](const FDAWorldAssetRecord& Asset) { return Asset.WorldAssetId == WorldAssetId; });
    }

    bool IsSupportedNodeType(const EDAQuestNodeType Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAQuestNodeType::Resolution);
    }
    bool IsSupportedEdgeCondition(const EDAQuestEdgeCondition Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAQuestEdgeCondition::WorldAssetDestroyed);
    }
    bool IsSupportedPayloadVariant(const EDAQuestPayloadVariant Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAQuestPayloadVariant::Relationship);
    }
    bool IsSupportedComparison(const EDAQuestComparison Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAQuestComparison::LessOrEqual);
    }
    bool IsSupportedQuestState(const EDAQuestProgressState Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAQuestProgressState::Abandoned);
    }
    bool IsSupportedEventState(const EDAWorldEventProgressState Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAWorldEventProgressState::Expired);
    }
    bool IsSupportedPromiseState(const EDAPromiseState Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAPromiseState::Breached);
    }
    bool IsSupportedEventScope(const EDAWorldEventScope Value)
    {
        return static_cast<uint8>(Value) <= static_cast<uint8>(EDAWorldEventScope::Axiom);
    }
    bool IsTerminalNode(const EDAQuestNodeType Type)
    {
        return Type == EDAQuestNodeType::Failure || Type == EDAQuestNodeType::Resolution;
    }

    EDAQuestPayloadVariant RequiredPayloadVariant(const EDAQuestNodeType Type)
    {
        switch (Type)
        {
        case EDAQuestNodeType::Timer: return EDAQuestPayloadVariant::Timer;
        case EDAQuestNodeType::WorldCondition: return EDAQuestPayloadVariant::World;
        case EDAQuestNodeType::CitizenCondition: return EDAQuestPayloadVariant::Citizen;
        case EDAQuestNodeType::FactionCondition: return EDAQuestPayloadVariant::Faction;
        case EDAQuestNodeType::EconomyCondition: return EDAQuestPayloadVariant::Economy;
        case EDAQuestNodeType::RelationshipCondition: return EDAQuestPayloadVariant::Relationship;
        default: return EDAQuestPayloadVariant::None;
        }
    }

    bool FindQuestCycle(const FDAQuestDefinitionManifest& Manifest, const FName NodeId,
        TSet<FName>& Visiting, TSet<FName>& Visited)
    {
        if (Visiting.Contains(NodeId)) return true;
        if (Visited.Contains(NodeId)) return false;
        Visiting.Add(NodeId);
        const FDAQuestNodeDefinition* Node = Manifest.FindNode(NodeId);
        if (Node == nullptr) return false;
        for (const FDAQuestEdgeDefinition& Edge : Node->Edges)
        {
            if (FindQuestCycle(Manifest, Edge.TargetNodeId, Visiting, Visited)) return true;
        }
        Visiting.Remove(NodeId);
        Visited.Add(NodeId);
        return false;
    }

    bool FindEventCycle(const FDAWorldEventDefinitionManifest& Manifest, const FName StageId,
        TSet<FName>& Visiting, TSet<FName>& Visited)
    {
        if (Visiting.Contains(StageId)) return true;
        if (Visited.Contains(StageId)) return false;
        Visiting.Add(StageId);
        const FDAWorldEventStageDefinition* Stage = Manifest.FindStage(StageId);
        if (Stage == nullptr) return false;
        for (const FName Next : Stage->AllowedNextStageIds)
        {
            if (FindEventCycle(Manifest, Next, Visiting, Visited)) return true;
        }
        Visiting.Remove(StageId);
        Visited.Add(StageId);
        return false;
    }

    bool IsLegalCompletedQuestPath(const FDAQuestSaveState& State)
    {
        if (State.CompletedNodeIds.IsEmpty())
        {
            return State.CurrentNodeId == State.DefinitionManifest.StartNodeId;
        }
        FName ExpectedNode = State.DefinitionManifest.StartNodeId;
        for (int32 Index = 0; Index < State.CompletedNodeIds.Num(); ++Index)
        {
            const FName CompletedNodeId = State.CompletedNodeIds[Index];
            if (CompletedNodeId != ExpectedNode)
            {
                return false;
            }
            const FDAQuestNodeDefinition* CompletedNode = State.DefinitionManifest.FindNode(CompletedNodeId);
            if (CompletedNode == nullptr || IsTerminalNode(CompletedNode->Type))
            {
                return false;
            }
            const FName NextNodeId = Index + 1 < State.CompletedNodeIds.Num()
                ? State.CompletedNodeIds[Index + 1]
                : State.CurrentNodeId;
            if (!CompletedNode->Edges.ContainsByPredicate(
                    [NextNodeId](const FDAQuestEdgeDefinition& Edge) { return Edge.TargetNodeId == NextNodeId; }))
            {
                return false;
            }
            ExpectedNode = NextNodeId;
        }
        return ExpectedNode == State.CurrentNodeId;
    }

    bool IsLegalCompletedEventPath(const FDAWorldEventSaveState& State)
    {
        if (State.CompletedStageIds.IsEmpty())
        {
            return State.CurrentStageId == State.DefinitionManifest.InitialStageId;
        }
        FName ExpectedStage = State.DefinitionManifest.InitialStageId;
        for (int32 Index = 0; Index < State.CompletedStageIds.Num(); ++Index)
        {
            const FName CompletedStageId = State.CompletedStageIds[Index];
            if (CompletedStageId != ExpectedStage)
            {
                return false;
            }
            const FDAWorldEventStageDefinition* CompletedStage = State.DefinitionManifest.FindStage(CompletedStageId);
            if (CompletedStage == nullptr || CompletedStage->bResolution)
            {
                return false;
            }
            const FName NextStageId = Index + 1 < State.CompletedStageIds.Num()
                ? State.CompletedStageIds[Index + 1]
                : State.CurrentStageId;
            if (!CompletedStage->AllowedNextStageIds.Contains(NextStageId))
            {
                return false;
            }
            ExpectedStage = NextStageId;
        }
        return ExpectedStage == State.CurrentStageId;
    }

    void AppendText(FString& Canonical, const FString& Text)
    {
        const FTCHARToUTF8 Utf8(*Text);
        Canonical += FString::Printf(TEXT("%d:"), Utf8.Length());
        Canonical += Text;
        Canonical += TEXT("|");
    }

    void AppendName(FString& Canonical, const FName Name)
    {
        AppendText(Canonical, Name.ToString().ToLower());
    }

    void AppendInteger(FString& Canonical, const int64 Value)
    {
        AppendText(Canonical, FString::Printf(TEXT("%lld"), static_cast<long long>(Value)));
    }

    void AppendDoubleBits(FString& Canonical, const double Value)
    {
        const double Normalized = Value == 0.0 ? 0.0 : Value;
        uint64 Bits = 0;
        FMemory::Memcpy(&Bits, &Normalized, sizeof(Bits));
        AppendText(Canonical, FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Bits)));
    }

    FString HashCanonicalUtf8(const FString& Canonical)
    {
        const FTCHARToUTF8 Utf8(*Canonical);
        FMD5 Hash;
        Hash.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
        uint8 Digest[16];
        Hash.Final(Digest);
        FString Result;
        Result.Reserve(32);
        for (const uint8 Byte : Digest)
        {
            Result += FString::Printf(TEXT("%02x"), Byte);
        }
        return Result;
    }

    void AppendLegacyName(FString& Canonical, const FName Name)
    {
        const FString Text = Name.ToString().ToLower();
        Canonical += FString::Printf(TEXT("%d:%s|"), Text.Len(), *Text);
    }

    FName FirstExactTag(const TArray<FName>& CandidateTags, const TArray<FName>& ActionTags)
    {
        FName First = NAME_None;
        for (const FName Candidate : CandidateTags)
        {
            if (ActionTags.Contains(Candidate) && (First.IsNone() || Candidate.LexicalLess(First)))
            {
                First = Candidate;
            }
        }
        return First;
    }
}

const FDAQuestNodeDefinition* FDAQuestDefinitionManifest::FindNode(const FName NodeId) const
{
    return Nodes.FindByPredicate([NodeId](const FDAQuestNodeDefinition& Node) { return Node.NodeId == NodeId; });
}

FString FDAQuestDefinitionManifest::ComputeFingerprint() const
{
    FString Canonical;
    AppendText(Canonical, TEXT("quest-manifest-v3"));
    AppendName(Canonical, QuestId);
    AppendName(Canonical, SourceDefinitionId);
    AppendInteger(Canonical, Version);
    AppendName(Canonical, StartNodeId);
    TArray<FName> Bindings = RequiredWorldAssetBindingIds;
    Bindings.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
    AppendInteger(Canonical, Bindings.Num());
    for (const FName Binding : Bindings) AppendName(Canonical, Binding);
    TArray<FDAQuestNodeDefinition> SortedNodes = Nodes;
    SortedNodes.Sort([](const FDAQuestNodeDefinition& Left, const FDAQuestNodeDefinition& Right)
    {
        return Left.NodeId.LexicalLess(Right.NodeId);
    });
    AppendInteger(Canonical, SortedNodes.Num());
    for (FDAQuestNodeDefinition& Node : SortedNodes)
    {
        AppendName(Canonical, Node.NodeId);
        AppendInteger(Canonical, static_cast<uint8>(Node.Type));
        AppendName(Canonical, Node.SourceDefinitionId);
        AppendInteger(Canonical, static_cast<uint8>(Node.Payload.Variant));
        switch (Node.Payload.Variant)
        {
        case EDAQuestPayloadVariant::Timer:
            AppendInteger(Canonical, Node.Payload.Timer.DurationWorldTicks);
            break;
        case EDAQuestPayloadVariant::World:
        case EDAQuestPayloadVariant::Citizen:
        case EDAQuestPayloadVariant::Faction:
        case EDAQuestPayloadVariant::Economy:
        case EDAQuestPayloadVariant::Relationship:
            AppendInteger(Canonical, static_cast<uint8>(Node.Payload.Condition.Comparison));
            AppendDoubleBits(Canonical, Node.Payload.Condition.ExpectedValue);
            AppendName(Canonical, Node.Payload.Condition.EvaluationKey);
            break;
        case EDAQuestPayloadVariant::WorldAsset:
            AppendName(Canonical, Node.Payload.WorldAssetBindingId);
            break;
        default:
            break;
        }
        Node.Edges.Sort([](const FDAQuestEdgeDefinition& Left, const FDAQuestEdgeDefinition& Right)
        {
            if (Left.BranchTag != Right.BranchTag) return Left.BranchTag.LexicalLess(Right.BranchTag);
            return Left.TargetNodeId.LexicalLess(Right.TargetNodeId);
        });
        AppendInteger(Canonical, Node.Edges.Num());
        for (const FDAQuestEdgeDefinition& Edge : Node.Edges)
        {
            AppendName(Canonical, Edge.BranchTag);
            AppendName(Canonical, Edge.TargetNodeId);
            AppendInteger(Canonical, static_cast<uint8>(Edge.Condition));
            AppendName(Canonical, Edge.WorldAssetBindingId);
        }
    }
    return HashCanonicalUtf8(Canonical);
}

FString FDAQuestDefinitionManifest::ComputeLegacyFingerprintV1() const
{
    FString Canonical(TEXT("quest-manifest-v1|"));
    AppendLegacyName(Canonical, QuestId);
    AppendLegacyName(Canonical, SourceDefinitionId);
    Canonical += FString::Printf(TEXT("%d|"), Version);
    AppendLegacyName(Canonical, StartNodeId);
    TArray<FName> Bindings = RequiredWorldAssetBindingIds;
    Bindings.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
    for (const FName Binding : Bindings) AppendLegacyName(Canonical, Binding);
    TArray<FDAQuestNodeDefinition> SortedNodes = Nodes;
    SortedNodes.Sort([](const FDAQuestNodeDefinition& Left, const FDAQuestNodeDefinition& Right)
    {
        return Left.NodeId.LexicalLess(Right.NodeId);
    });
    for (FDAQuestNodeDefinition& Node : SortedNodes)
    {
        AppendLegacyName(Canonical, Node.NodeId);
        Canonical += FString::Printf(TEXT("%u|"), static_cast<uint8>(Node.Type));
        AppendLegacyName(Canonical, Node.SourceDefinitionId);
        Canonical += FString::Printf(TEXT("%u|%lld|%u|%.17g|"), static_cast<uint8>(Node.Payload.Variant),
            static_cast<long long>(Node.Payload.Timer.DurationWorldTicks),
            static_cast<uint8>(Node.Payload.Condition.Comparison),
            Node.Payload.Condition.ExpectedValue);
        AppendLegacyName(Canonical, Node.Payload.Condition.EvaluationKey);
        AppendLegacyName(Canonical, Node.Payload.WorldAssetBindingId);
        Node.Edges.Sort([](const FDAQuestEdgeDefinition& Left, const FDAQuestEdgeDefinition& Right)
        {
            if (Left.BranchTag != Right.BranchTag) return Left.BranchTag.LexicalLess(Right.BranchTag);
            return Left.TargetNodeId.LexicalLess(Right.TargetNodeId);
        });
        for (const FDAQuestEdgeDefinition& Edge : Node.Edges)
        {
            AppendLegacyName(Canonical, Edge.BranchTag);
            AppendLegacyName(Canonical, Edge.TargetNodeId);
            Canonical += FString::Printf(TEXT("%u|"), static_cast<uint8>(Edge.Condition));
            AppendLegacyName(Canonical, Edge.WorldAssetBindingId);
        }
    }
    return FMD5::HashAnsiString(*Canonical);
}

void FDAQuestDefinitionManifest::RefreshFingerprint() { DefinitionFingerprint = ComputeFingerprint(); }

const FString& FDAFirstHourFrozenPolicy::ManifestFingerprint()
{
    static const FString Value(TEXT("2c82c47966c118441564e717007e451326bfdea4"));
    return Value;
}

const TMap<FName, FString>& FDAFirstHourFrozenPolicy::QuestDefinitionFingerprints()
{
    static const TMap<FName, FString> Values = {
        {TEXT("quest.wake_the_hall"), TEXT("f11762cc9af35a20228653613e3aed94")},
        {TEXT("quest.a_place_to_stay"), TEXT("8a05caf2f105ab31bb803be3087755c1")},
        {TEXT("quest.power_water_people"), TEXT("5f173154f0296e73e636a5e5860a0517")},
        {TEXT("quest.nia_needs_a_job"), TEXT("ac2af15d4cb5bb462095b38548acd312")},
        {TEXT("quest.replacement_model"), TEXT("e8dee46ea3e90d3655bd463bb6cb305c")},
        {TEXT("quest.agency_has_a_price"), TEXT("3d156ebe84358dd8f7446098aaff5d93")},
        {TEXT("quest.signal_in_foundation"), TEXT("032967fff135bf8d0273aadd1529396c")},
        {TEXT("quest.iron_at_border"), TEXT("b386f076cabf81be2198a2d0c2f01f74")},
        {TEXT("quest.basin_speaks"), TEXT("cf218ca8f51d70095353468a56051301")}
    };
    return Values;
}

const TMap<FName, FName>& FDAFirstHourFrozenPolicy::QuestSourceDefinitionIds()
{
    static const TMap<FName, FName> Values = {
        {TEXT("quest.wake_the_hall"), TEXT("quest.wake_the_hall.v2")},
        {TEXT("quest.a_place_to_stay"), TEXT("quest.a_place_to_stay.v2")},
        {TEXT("quest.power_water_people"), TEXT("quest.power_water_people.v2")},
        {TEXT("quest.nia_needs_a_job"), TEXT("quest.nia_needs_a_job.v2")},
        {TEXT("quest.replacement_model"), TEXT("quest.replacement_model.v2")},
        {TEXT("quest.agency_has_a_price"), TEXT("quest.agency_has_a_price.v2")},
        {TEXT("quest.signal_in_foundation"), TEXT("quest.signal_in_foundation.v2")},
        {TEXT("quest.iron_at_border"), TEXT("quest.iron_at_border.v2")},
        {TEXT("quest.basin_speaks"), TEXT("quest.basin_speaks.v2")}
    };
    return Values;
}

bool FDAFirstHourFrozenPolicy::ValidatePinnedQuestDefinition(
    const FDAQuestDefinitionManifest& Definition, FString& OutError)
{
    const FString* Fingerprint = QuestDefinitionFingerprints().Find(Definition.QuestId);
    if (Fingerprint == nullptr) return true;
    const FName* SourceId = QuestSourceDefinitionIds().Find(Definition.QuestId);
    if (SourceId == nullptr || Definition.SourceDefinitionId != *SourceId || Definition.Version != 2
        || Definition.DefinitionFingerprint != *Fingerprint || Definition.ComputeFingerprint() != *Fingerprint)
    {
        OutError = TEXT("First-hour quest definition diverges from the Core frozen identity policy.");
        return false;
    }
    return true;
}

bool FDAQuestDefinitionManifest::Validate(FString& OutError) const
{
    if (QuestId.IsNone() || SourceDefinitionId.IsNone() || Version <= 0 || StartNodeId.IsNone()
        || Nodes.IsEmpty() || DefinitionFingerprint.IsEmpty() || DefinitionFingerprint != ComputeFingerprint())
    {
        OutError = TEXT("Quest manifest identity or deterministic fingerprint is invalid.");
        return false;
    }
    TSet<FName> RequiredBindings;
    if (!HasUniqueValidKeys(RequiredWorldAssetBindingIds, RequiredBindings, [](const FName Name) { return Name; }))
    {
        OutError = TEXT("Quest WorldAsset binding declarations must be unique and non-empty.");
        return false;
    }
    TSet<FName> NodeIds;
    int32 StartCount = 0;
    int32 ResolutionCount = 0;
    for (const FDAQuestNodeDefinition& Node : Nodes)
    {
        const EDAQuestPayloadVariant RequiredVariant = RequiredPayloadVariant(Node.Type);
        const bool bPayloadMatches = RequiredVariant != EDAQuestPayloadVariant::None
            ? Node.Payload.Variant == RequiredVariant
            : (Node.Payload.Variant == EDAQuestPayloadVariant::None
                || (Node.Type == EDAQuestNodeType::Objective && Node.Payload.Variant == EDAQuestPayloadVariant::WorldAsset));
        if (Node.NodeId.IsNone() || NodeIds.Contains(Node.NodeId) || Node.SourceDefinitionId.IsNone()
            || !IsSupportedNodeType(Node.Type) || !IsSupportedPayloadVariant(Node.Payload.Variant) || !bPayloadMatches)
        {
            OutError = TEXT("Quest nodes require unique IDs, exact node enums, and matching tagged payload variants.");
            return false;
        }
        if (Node.Payload.Variant == EDAQuestPayloadVariant::Timer && Node.Payload.Timer.DurationWorldTicks <= 0)
        {
            OutError = TEXT("Timer payload duration must be positive.");
            return false;
        }
        const bool bConditionPayload = Node.Payload.Variant == EDAQuestPayloadVariant::World
            || Node.Payload.Variant == EDAQuestPayloadVariant::Citizen || Node.Payload.Variant == EDAQuestPayloadVariant::Faction
            || Node.Payload.Variant == EDAQuestPayloadVariant::Economy || Node.Payload.Variant == EDAQuestPayloadVariant::Relationship;
        if (bConditionPayload && (Node.Payload.Condition.EvaluationKey.IsNone()
            || !IsSupportedComparison(Node.Payload.Condition.Comparison) || !FMath::IsFinite(Node.Payload.Condition.ExpectedValue)))
        {
            OutError = TEXT("Condition payload requires a key, finite value, and exact comparison enum.");
            return false;
        }
        const bool bDefaultTimer = Node.Payload.Timer.DurationWorldTicks == 1;
        const bool bDefaultCondition = Node.Payload.Condition.EvaluationKey.IsNone()
            && Node.Payload.Condition.Comparison == EDAQuestComparison::GreaterOrEqual
            && Node.Payload.Condition.ExpectedValue == 0.0;
        const bool bInactivePayloadCanonical =
            (Node.Payload.Variant == EDAQuestPayloadVariant::None
                && bDefaultTimer && bDefaultCondition && Node.Payload.WorldAssetBindingId.IsNone())
            || (Node.Payload.Variant == EDAQuestPayloadVariant::Timer
                && bDefaultCondition && Node.Payload.WorldAssetBindingId.IsNone())
            || (bConditionPayload && bDefaultTimer && Node.Payload.WorldAssetBindingId.IsNone())
            || (Node.Payload.Variant == EDAQuestPayloadVariant::WorldAsset
                && bDefaultTimer && bDefaultCondition);
        if (!FMath::IsFinite(Node.Payload.Condition.ExpectedValue) || !bInactivePayloadCanonical)
        {
            OutError = TEXT("Quest payload inactive fields must retain their finite canonical defaults.");
            return false;
        }
        if (Node.Payload.Variant == EDAQuestPayloadVariant::WorldAsset
            && (Node.Payload.WorldAssetBindingId.IsNone() || !RequiredBindings.Contains(Node.Payload.WorldAssetBindingId)))
        {
            OutError = TEXT("WorldAsset payload must reference a required binding.");
            return false;
        }
        NodeIds.Add(Node.NodeId);
        StartCount += Node.Type == EDAQuestNodeType::Start ? 1 : 0;
        ResolutionCount += Node.Type == EDAQuestNodeType::Resolution ? 1 : 0;
        if (IsTerminalNode(Node.Type) != Node.Edges.IsEmpty())
        {
            OutError = TEXT("Terminal consistency requires terminal nodes to have no edges and other nodes to transition.");
            return false;
        }
        TSet<FName> Branches;
        for (const FDAQuestEdgeDefinition& Edge : Node.Edges)
        {
            const bool bAssetCondition = Edge.Condition == EDAQuestEdgeCondition::WorldAssetAvailable
                || Edge.Condition == EDAQuestEdgeCondition::WorldAssetDestroyed;
            if (Edge.BranchTag.IsNone() || Branches.Contains(Edge.BranchTag) || Edge.TargetNodeId.IsNone()
                || !IsSupportedEdgeCondition(Edge.Condition)
                || (bAssetCondition && !RequiredBindings.Contains(Edge.WorldAssetBindingId))
                || (!bAssetCondition && !Edge.WorldAssetBindingId.IsNone()))
            {
                OutError = TEXT("Quest edge identity, enum, target, or binding is invalid.");
                return false;
            }
            Branches.Add(Edge.BranchTag);
        }
    }
    const FDAQuestNodeDefinition* Start = FindNode(StartNodeId);
    if (Start == nullptr || Start->Type != EDAQuestNodeType::Start || StartCount != 1 || ResolutionCount == 0)
    {
        OutError = TEXT("Quest graph requires exactly one referenced Start and at least one Resolution.");
        return false;
    }
    for (const FDAQuestNodeDefinition& Node : Nodes)
    {
        for (const FDAQuestEdgeDefinition& Edge : Node.Edges)
        {
            if (!NodeIds.Contains(Edge.TargetNodeId))
            {
                OutError = TEXT("Quest edge targets a node outside the definition.");
                return false;
            }
        }
    }
    TSet<FName> Visiting;
    TSet<FName> Reachable;
    if (FindQuestCycle(*this, StartNodeId, Visiting, Reachable))
    {
        OutError = TEXT("Quest graph contains a cycle.");
        return false;
    }
    if (Reachable.Num() != Nodes.Num())
    {
        OutError = TEXT("Quest graph contains an unreachable node or Resolution.");
        return false;
    }
    return true;
}

const FDAWorldEventStageDefinition* FDAWorldEventDefinitionManifest::FindStage(const FName StageId) const
{
    return Stages.FindByPredicate([StageId](const FDAWorldEventStageDefinition& Stage) { return Stage.StageId == StageId; });
}

FString FDAWorldEventDefinitionManifest::ComputeFingerprint() const
{
    FString Canonical;
    AppendText(Canonical, TEXT("world-event-manifest-v3"));
    AppendName(Canonical, EventId);
    AppendName(Canonical, SourceDefinitionId);
    AppendInteger(Canonical, Version);
    AppendInteger(Canonical, static_cast<uint8>(Scope));
    AppendName(Canonical, InitialStageId);
    TArray<FDAWorldEventStageDefinition> SortedStages = Stages;
    SortedStages.Sort([](const FDAWorldEventStageDefinition& Left, const FDAWorldEventStageDefinition& Right)
    {
        return Left.StageId.LexicalLess(Right.StageId);
    });
    AppendInteger(Canonical, SortedStages.Num());
    for (FDAWorldEventStageDefinition& Stage : SortedStages)
    {
        AppendName(Canonical, Stage.StageId);
        AppendName(Canonical, Stage.SourceDefinitionId);
        AppendInteger(Canonical, Stage.bResolution ? 1 : 0);
        Stage.AllowedNextStageIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        AppendInteger(Canonical, Stage.AllowedNextStageIds.Num());
        for (const FName Next : Stage.AllowedNextStageIds) AppendName(Canonical, Next);
    }
    return HashCanonicalUtf8(Canonical);
}

FString FDAWorldEventDefinitionManifest::ComputeLegacyFingerprintV1() const
{
    FString Canonical(TEXT("world-event-manifest-v1|"));
    AppendLegacyName(Canonical, EventId);
    AppendLegacyName(Canonical, SourceDefinitionId);
    Canonical += FString::Printf(TEXT("%d|%u|"), Version, static_cast<uint8>(Scope));
    AppendLegacyName(Canonical, InitialStageId);
    TArray<FDAWorldEventStageDefinition> SortedStages = Stages;
    SortedStages.Sort([](const FDAWorldEventStageDefinition& Left, const FDAWorldEventStageDefinition& Right)
    {
        return Left.StageId.LexicalLess(Right.StageId);
    });
    for (FDAWorldEventStageDefinition& Stage : SortedStages)
    {
        AppendLegacyName(Canonical, Stage.StageId);
        AppendLegacyName(Canonical, Stage.SourceDefinitionId);
        Canonical += Stage.bResolution ? TEXT("1|") : TEXT("0|");
        Stage.AllowedNextStageIds.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
        for (const FName Next : Stage.AllowedNextStageIds) AppendLegacyName(Canonical, Next);
    }
    return FMD5::HashAnsiString(*Canonical);
}

void FDAWorldEventDefinitionManifest::RefreshFingerprint() { DefinitionFingerprint = ComputeFingerprint(); }

bool FDAWorldEventDefinitionManifest::Validate(FString& OutError) const
{
    if (EventId.IsNone() || SourceDefinitionId.IsNone() || Version <= 0 || !IsSupportedEventScope(Scope)
        || InitialStageId.IsNone() || Stages.IsEmpty() || DefinitionFingerprint.IsEmpty()
        || DefinitionFingerprint != ComputeFingerprint())
    {
        OutError = TEXT("World-event manifest identity, scope, or deterministic fingerprint is invalid.");
        return false;
    }
    TSet<FName> StageIds;
    int32 ResolutionCount = 0;
    for (const FDAWorldEventStageDefinition& Stage : Stages)
    {
        if (Stage.StageId.IsNone() || Stage.SourceDefinitionId.IsNone() || StageIds.Contains(Stage.StageId)
            || !HasUniqueNames(Stage.AllowedNextStageIds, Stage.bResolution)
            || Stage.bResolution != Stage.AllowedNextStageIds.IsEmpty())
        {
            OutError = TEXT("World-event stage membership, source, transition, or terminal consistency is invalid.");
            return false;
        }
        StageIds.Add(Stage.StageId);
        ResolutionCount += Stage.bResolution ? 1 : 0;
    }
    if (!StageIds.Contains(InitialStageId) || ResolutionCount == 0)
    {
        OutError = TEXT("World-event graph requires its initial stage and a resolution stage.");
        return false;
    }
    for (const FDAWorldEventStageDefinition& Stage : Stages)
    {
        for (const FName Next : Stage.AllowedNextStageIds)
        {
            if (!StageIds.Contains(Next))
            {
                OutError = TEXT("World-event transition targets an unknown stage.");
                return false;
            }
        }
    }
    TSet<FName> Visiting;
    TSet<FName> Reachable;
    if (FindEventCycle(*this, InitialStageId, Visiting, Reachable))
    {
        OutError = TEXT("World-event graph contains a cycle.");
        return false;
    }
    if (Reachable.Num() != Stages.Num())
    {
        OutError = TEXT("World-event graph contains an unreachable stage.");
        return false;
    }
    return true;
}

bool FDAQuestEvaluationContext::SetMetric(const EDAQuestPayloadVariant Variant, const FName Key, const double Value)
{
    if (Key.IsNone() || !FMath::IsFinite(Value)) return false;
    TMap<FName, double>* Target = nullptr;
    switch (Variant)
    {
    case EDAQuestPayloadVariant::World: Target = &WorldValues; break;
    case EDAQuestPayloadVariant::Citizen: Target = &CitizenValues; break;
    case EDAQuestPayloadVariant::Faction: Target = &FactionValues; break;
    case EDAQuestPayloadVariant::Economy: Target = &EconomyValues; break;
    case EDAQuestPayloadVariant::Relationship: Target = &RelationshipValues; break;
    default: return false;
    }
    Target->Add(Key, Value);
    return true;
}

bool FDAQuestEvaluationContext::TryGetMetric(
    const EDAQuestPayloadVariant Variant, const FName Key, double& OutValue) const
{
    const TMap<FName, double>* Source = nullptr;
    switch (Variant)
    {
    case EDAQuestPayloadVariant::World: Source = &WorldValues; break;
    case EDAQuestPayloadVariant::Citizen: Source = &CitizenValues; break;
    case EDAQuestPayloadVariant::Faction: Source = &FactionValues; break;
    case EDAQuestPayloadVariant::Economy: Source = &EconomyValues; break;
    case EDAQuestPayloadVariant::Relationship: Source = &RelationshipValues; break;
    default: return false;
    }
    const double* Value = Source->Find(Key);
    if (Value == nullptr || !FMath::IsFinite(*Value)) return false;
    OutValue = *Value;
    return true;
}

const FDAQuestWorldAssetBinding* FDAQuestSaveState::FindWorldAssetBinding(const FName BindingId) const
{
    return WorldAssetBindings.FindByPredicate(
        [BindingId](const FDAQuestWorldAssetBinding& Binding) { return Binding.BindingId == BindingId; });
}

FDAQuestSaveState* FDANarrativeCampaignState::FindQuestState(const FName QuestId)
{
    return QuestStates.FindByPredicate([QuestId](const FDAQuestSaveState& State) { return State.QuestId == QuestId; });
}
const FDAQuestSaveState* FDANarrativeCampaignState::FindQuestState(const FName QuestId) const
{
    return QuestStates.FindByPredicate([QuestId](const FDAQuestSaveState& State) { return State.QuestId == QuestId; });
}
FDAWorldEventSaveState* FDANarrativeCampaignState::FindEventState(const FName EventId)
{
    return EventStates.FindByPredicate([EventId](const FDAWorldEventSaveState& State) { return State.EventId == EventId; });
}
const FDAWorldEventSaveState* FDANarrativeCampaignState::FindEventState(const FName EventId) const
{
    return EventStates.FindByPredicate([EventId](const FDAWorldEventSaveState& State) { return State.EventId == EventId; });
}
FDAPromiseRecord* FDANarrativeCampaignState::FindPromiseRecord(const FGuid PromiseId)
{
    return PromiseRecords.FindByPredicate([PromiseId](const FDAPromiseRecord& Record) { return Record.PromiseId == PromiseId; });
}
const FDAPromiseRecord* FDANarrativeCampaignState::FindPromiseRecord(const FGuid PromiseId) const
{
    return PromiseRecords.FindByPredicate([PromiseId](const FDAPromiseRecord& Record) { return Record.PromiseId == PromiseId; });
}
const FDANarrativeActionRecord* FDANarrativeCampaignState::FindActionRecord(const FGuid ActionId) const
{
    return ActionRecords.FindByPredicate([ActionId](const FDANarrativeActionRecord& Record) { return Record.ActionId == ActionId; });
}

bool FDANarrativeCampaignState::Validate(
    const TArray<FDAWorldAssetRecord>& WorldAssets,
    const TArray<FName>& CampaignHistoryTags,
    FString& OutError) const
{
    if (MutationRevision < 0)
    {
        OutError = TEXT("Narrative mutation revision cannot be negative.");
        return false;
    }
    TSet<FName> QuestIds;
    if (!HasUniqueValidKeys(QuestStates, QuestIds, [](const FDAQuestSaveState& State) { return State.QuestId; }))
    {
        OutError = TEXT("Quest save records require unique stable QuestIDs.");
        return false;
    }
    for (const FDAQuestSaveState& State : QuestStates)
    {
        if (!State.DefinitionManifest.Validate(OutError)
            || !FDAFirstHourFrozenPolicy::ValidatePinnedQuestDefinition(State.DefinitionManifest, OutError)
            || State.QuestId != State.DefinitionManifest.QuestId
            || State.DefinitionVersion != State.DefinitionManifest.Version || !IsSupportedQuestState(State.ProgressState)
            || State.StartedWorldTick < 0 || State.LastTransitionWorldTick < State.StartedWorldTick
            || State.CurrentNodeEnteredWorldTick < State.StartedWorldTick
            || State.CurrentNodeEnteredWorldTick > State.LastTransitionWorldTick
            || !HasUniqueNames(State.CompletedNodeIds, true)
            || State.NodeTransitionRecords.Num() != State.CompletedNodeIds.Num())
        {
            OutError = TEXT("Quest instance identity, enum, manifest, time, or completed-node state is invalid.");
            return false;
        }
        const FDAQuestNodeDefinition* Current = State.DefinitionManifest.FindNode(State.CurrentNodeId);
        if (Current == nullptr)
        {
            OutError = TEXT("Quest current node is not a member of its immutable definition.");
            return false;
        }
        for (const FName Completed : State.CompletedNodeIds)
        {
            if (State.DefinitionManifest.FindNode(Completed) == nullptr || Completed == State.CurrentNodeId)
            {
                OutError = TEXT("Quest completed-node membership is inconsistent.");
                return false;
            }
        }
        int64 PreviousTransitionTick = State.StartedWorldTick;
        for (int32 Index = 0; Index < State.NodeTransitionRecords.Num(); ++Index)
        {
            const FDAQuestNodeTransitionRecord& Transition = State.NodeTransitionRecords[Index];
            const FName ExpectedEnteredNode = Index + 1 < State.CompletedNodeIds.Num()
                ? State.CompletedNodeIds[Index + 1] : State.CurrentNodeId;
            if (Transition.CompletedNodeId != State.CompletedNodeIds[Index]
                || Transition.EnteredNodeId != ExpectedEnteredNode
                || Transition.WorldTick < PreviousTransitionTick
                || Transition.WorldTick > State.LastTransitionWorldTick
                || (Index + 1 == State.NodeTransitionRecords.Num()
                    && Transition.WorldTick != State.LastTransitionWorldTick))
            {
                OutError = TEXT("Quest node transition records must exactly prove the authored path and transition ticks.");
                return false;
            }
            PreviousTransitionTick = Transition.WorldTick;
        }
        if (!IsLegalCompletedQuestPath(State))
        {
            OutError = TEXT("Quest completed nodes must be the exact authored path from Start to immediately before current.");
            return false;
        }
        const bool bTerminalConsistent = (Current->Type == EDAQuestNodeType::Resolution
                && State.ProgressState == EDAQuestProgressState::Completed)
            || (Current->Type == EDAQuestNodeType::Failure && State.ProgressState == EDAQuestProgressState::Failed)
            || (!IsTerminalNode(Current->Type)
                && (State.ProgressState == EDAQuestProgressState::Active || State.ProgressState == EDAQuestProgressState::Abandoned));
        if (!bTerminalConsistent)
        {
            OutError = TEXT("Quest terminal node and progress state must agree.");
            return false;
        }
        TSet<FName> BindingIds;
        for (const FDAQuestWorldAssetBinding& Binding : State.WorldAssetBindings)
        {
            if (Binding.BindingId.IsNone() || BindingIds.Contains(Binding.BindingId)
                || !Binding.WorldAssetId.IsValid() || !ContainsWorldAsset(WorldAssets, Binding.WorldAssetId))
            {
                OutError = TEXT("Quest WorldAsset bindings require unique keys resolving canonical WorldAssetIDs.");
                return false;
            }
            BindingIds.Add(Binding.BindingId);
        }
        if (BindingIds.Num() != State.DefinitionManifest.RequiredWorldAssetBindingIds.Num())
        {
            OutError = TEXT("Quest instance is missing a required WorldAsset binding.");
            return false;
        }
        for (const FName RequiredBinding : State.DefinitionManifest.RequiredWorldAssetBindingIds)
        {
            if (!BindingIds.Contains(RequiredBinding))
            {
                OutError = TEXT("Quest instance is missing a required WorldAsset binding.");
                return false;
            }
        }
    }

    TSet<FName> EventIds;
    if (!HasUniqueValidKeys(EventStates, EventIds, [](const FDAWorldEventSaveState& State) { return State.EventId; }))
    {
        OutError = TEXT("World-event save records require unique stable EventIDs.");
        return false;
    }
    for (const FDAWorldEventSaveState& State : EventStates)
    {
        if (!State.DefinitionManifest.Validate(OutError) || State.EventId != State.DefinitionManifest.EventId
            || State.DefinitionVersion != State.DefinitionManifest.Version || !IsSupportedEventState(State.ProgressState)
            || State.StartedWorldTick < 0 || State.LastTransitionWorldTick < State.StartedWorldTick
            || !HasUniqueNames(State.CompletedStageIds, true))
        {
            OutError = TEXT("World-event instance identity, enum, manifest, time, or completed-stage state is invalid.");
            return false;
        }
        const FDAWorldEventStageDefinition* Current = State.DefinitionManifest.FindStage(State.CurrentStageId);
        if (Current == nullptr)
        {
            OutError = TEXT("World-event current stage is not a member of its immutable definition.");
            return false;
        }
        for (const FName Completed : State.CompletedStageIds)
        {
            if (State.DefinitionManifest.FindStage(Completed) == nullptr || Completed == State.CurrentStageId)
            {
                OutError = TEXT("World-event completed-stage membership is inconsistent.");
                return false;
            }
        }
        if (!IsLegalCompletedEventPath(State))
        {
            OutError = TEXT("World-event completed stages must be the exact authored path from Initial to immediately before current.");
            return false;
        }
        if ((Current->bResolution && State.ProgressState != EDAWorldEventProgressState::Resolved)
            || (!Current->bResolution && State.ProgressState == EDAWorldEventProgressState::Resolved))
        {
            OutError = TEXT("World-event terminal stage and progress state must agree.");
            return false;
        }
    }

    TSet<FGuid> PromiseIds;
    for (const FDAPromiseRecord& Promise : PromiseRecords)
    {
        if (!Promise.PromiseId.IsValid() || PromiseIds.Contains(Promise.PromiseId)
            || Promise.PromiseDefinitionId.IsNone() || Promise.PromiserId.IsNone() || Promise.CreatedWorldTick < 0
            || !IsSupportedPromiseState(Promise.State) || !HasUniqueNames(Promise.ConflictActionTags, false)
            || !HasUniqueNames(Promise.FulfillmentActionTags, false))
        {
            OutError = TEXT("Promise records require stable identity, exact enums, source, time, and unique action tags.");
            return false;
        }
        PromiseIds.Add(Promise.PromiseId);
        for (const FName Conflict : Promise.ConflictActionTags)
        {
            if (Promise.FulfillmentActionTags.Contains(Conflict))
            {
                OutError = TEXT("A Promise action tag cannot both fulfill and breach the same promise.");
                return false;
            }
        }
        const bool bActiveShape = Promise.State == EDAPromiseState::Active
            && Promise.ResolvedWorldTick == 0 && Promise.ResolutionActionTag.IsNone()
            && !Promise.ResolutionActionId.IsValid() && !Promise.bLegacyResolutionWithoutAction
            && Promise.LegacyResolutionSourceSchemaVersion == 0;
        const bool bFulfilledBase = Promise.State == EDAPromiseState::Fulfilled
            && Promise.ResolvedWorldTick >= Promise.CreatedWorldTick
            && Promise.FulfillmentActionTags.Contains(Promise.ResolutionActionTag);
        const bool bBreachedBase = Promise.State == EDAPromiseState::Breached
            && Promise.ResolvedWorldTick >= Promise.CreatedWorldTick
            && Promise.ConflictActionTags.Contains(Promise.ResolutionActionTag);
        const bool bLegacyResolutionShape = (bFulfilledBase || bBreachedBase)
            && Promise.bLegacyResolutionWithoutAction
            && Promise.LegacyResolutionSourceSchemaVersion == 7
            && !Promise.ResolutionActionId.IsValid();
        const bool bModernResolutionShape = (bFulfilledBase || bBreachedBase)
            && !Promise.bLegacyResolutionWithoutAction
            && Promise.LegacyResolutionSourceSchemaVersion == 0
            && Promise.ResolutionActionId.IsValid();
        if (!bActiveShape && !bLegacyResolutionShape && !bModernResolutionShape)
        {
            OutError = TEXT("Promise state requires an appropriate tag and either an exact ActionID link or explicit schema-v7 provenance.");
            return false;
        }
    }

    TSet<FName> CampaignHistory;
    for (const FName HistoryTag : CampaignHistoryTags)
    {
        CampaignHistory.Add(HistoryTag);
    }
    TMap<FGuid, int32> PromiseResultReferenceCounts;
    TSet<FGuid> ActionIds;
    FString PreviousAction;
    for (const FDANarrativeActionRecord& Action : ActionRecords)
    {
        const FString CurrentAction = GuidKey(Action.ActionId);
        if (!Action.ActionId.IsValid() || ActionIds.Contains(Action.ActionId)
            || (!PreviousAction.IsEmpty() && !(PreviousAction < CurrentAction)))
        {
            OutError = TEXT("Narrative action records require unique stable sorted ActionIDs.");
            return false;
        }
        ActionIds.Add(Action.ActionId);
        PreviousAction = CurrentAction;
        if (Action.bLegacyIdentityOnly)
        {
            if (!Action.NormalizedActionTags.IsEmpty() || !Action.FulfilledPromiseIds.IsEmpty()
                || !Action.BreachedPromiseIds.IsEmpty() || Action.WorldTick != 0)
            {
                OutError = TEXT("Legacy narrative action identity cannot claim missing semantics.");
                return false;
            }
            continue;
        }
        if (Action.WorldTick < 0 || !NamesAreNormalized(Action.NormalizedActionTags)
            || !GuidsAreUniqueAndSorted(Action.FulfilledPromiseIds) || !GuidsAreUniqueAndSorted(Action.BreachedPromiseIds))
        {
            OutError = TEXT("Narrative actions require normalized tags, tick, and stable result identities.");
            return false;
        }
        for (const FName ActionTag : Action.NormalizedActionTags)
        {
            if (!CampaignHistory.Contains(ActionTag))
            {
                OutError = TEXT("Every nonlegacy narrative action tag must exist in canonical campaign history.");
                return false;
            }
        }
        for (const FGuid PromiseId : Action.FulfilledPromiseIds)
        {
            const FDAPromiseRecord* Promise = FindPromiseRecord(PromiseId);
            const FName CanonicalFulfillmentTag = Promise == nullptr
                ? NAME_None
                : FirstExactTag(Promise->FulfillmentActionTags, Action.NormalizedActionTags);
            const FName ConflictingTag = Promise == nullptr
                ? NAME_None
                : FirstExactTag(Promise->ConflictActionTags, Action.NormalizedActionTags);
            if (Promise == nullptr || Promise->State != EDAPromiseState::Fulfilled
                || Promise->bLegacyResolutionWithoutAction
                || Promise->ResolutionActionId != Action.ActionId
                || Promise->ResolvedWorldTick != Action.WorldTick
                || CanonicalFulfillmentTag.IsNone()
                || Promise->ResolutionActionTag != CanonicalFulfillmentTag
                || !ConflictingTag.IsNone())
            {
                OutError = TEXT("Narrative fulfillment must use CommitAction's canonical exact tag and contain no conflict tag.");
                return false;
            }
            ++PromiseResultReferenceCounts.FindOrAdd(PromiseId);
        }
        for (const FGuid PromiseId : Action.BreachedPromiseIds)
        {
            const FDAPromiseRecord* Promise = FindPromiseRecord(PromiseId);
            const FName CanonicalConflictTag = Promise == nullptr
                ? NAME_None
                : FirstExactTag(Promise->ConflictActionTags, Action.NormalizedActionTags);
            const FName FulfillmentTag = Promise == nullptr
                ? NAME_None
                : FirstExactTag(Promise->FulfillmentActionTags, Action.NormalizedActionTags);
            if (Promise == nullptr || Promise->State != EDAPromiseState::Breached
                || Promise->bLegacyResolutionWithoutAction
                || Promise->ResolutionActionId != Action.ActionId
                || Promise->ResolvedWorldTick != Action.WorldTick
                || CanonicalConflictTag.IsNone()
                || Promise->ResolutionActionTag != CanonicalConflictTag
                || !FulfillmentTag.IsNone()
                || Action.FulfilledPromiseIds.Contains(PromiseId))
            {
                OutError = TEXT("Narrative breach must use CommitAction's canonical exact tag and contain no fulfillment tag.");
                return false;
            }
            ++PromiseResultReferenceCounts.FindOrAdd(PromiseId);
        }
    }
    for (const FDAPromiseRecord& Promise : PromiseRecords)
    {
        if (Promise.State == EDAPromiseState::Active || Promise.bLegacyResolutionWithoutAction)
        {
            continue;
        }
        const int32* ReferenceCount = PromiseResultReferenceCounts.Find(Promise.PromiseId);
        const FDANarrativeActionRecord* Action = FindActionRecord(Promise.ResolutionActionId);
        if (ReferenceCount == nullptr || *ReferenceCount != 1 || Action == nullptr || Action->bLegacyIdentityOnly)
        {
            OutError = TEXT("Every nonlegacy resolved promise must map to exactly one semantic action result.");
            return false;
        }
    }
    const TSet<FName> AllowedCitizenIds = {TEXT("citizen.synara.nia_vale")};
    const TSet<FName> AllowedStoryStates = {TEXT("story.nia.replacement_model.automation_accepted"), TEXT("story.nia.replacement_model.jobs_preserved"), TEXT("story.nia.replacement_model.model_audited"), TEXT("story.nia.replacement_model.jobs_protected"), TEXT("story.nia.human_override.supported")};
    for (const TPair<FName, FName>& Story : CitizenStoryStates)
    {
        if (!AllowedCitizenIds.Contains(Story.Key) || !AllowedStoryStates.Contains(Story.Value))
        {
            OutError = TEXT("Citizen story state requires stable citizen and state IDs.");
            return false;
        }
    }
    TMap<FName, FName> LatestStory;
    TMap<FName, int64> LatestStoryTick;
    TSet<FGuid> StoryActions;
    for (const FDACitizenStoryTransitionRecord& Transition : CitizenStoryTransitionRecords)
    {
        const FDANarrativeActionRecord* Action = FindActionRecord(Transition.SourceActionId);
        const FName ExpectedBaseline = LatestStory.FindRef(Transition.CitizenId);
        if (!AllowedCitizenIds.Contains(Transition.CitizenId) || !AllowedStoryStates.Contains(Transition.ResultStoryState)
            || Transition.BaselineStoryState != ExpectedBaseline || Transition.SourceActionId.IsValid() == false
            || StoryActions.Contains(Transition.SourceActionId) || Action == nullptr || Action->bLegacyIdentityOnly
            || Action->NormalizedActionTags != TArray<FName>{Transition.SourceActionTag}
            || Action->WorldTick != Transition.WorldTick
            || (LatestStoryTick.Contains(Transition.CitizenId) && Transition.WorldTick <= LatestStoryTick[Transition.CitizenId]))
        { OutError = TEXT("Citizen story states require a unique causal canonical narrative action transition."); return false; }
        LatestStory.Add(Transition.CitizenId, Transition.ResultStoryState); LatestStoryTick.Add(Transition.CitizenId, Transition.WorldTick);
        StoryActions.Add(Transition.SourceActionId);
    }
    if (!LatestStory.OrderIndependentCompareEqual(CitizenStoryStates))
    { OutError = TEXT("Citizen story aggregate must equal its latest durable transition."); return false; }
    const TSet<FName> AllowedContentQuestIds = {TEXT("quest.wake_the_hall"), TEXT("quest.a_place_to_stay"), TEXT("quest.power_water_people"), TEXT("quest.nia_needs_a_job"), TEXT("quest.replacement_model"), TEXT("quest.agency_has_a_price"), TEXT("quest.signal_in_foundation"), TEXT("quest.iron_at_border"), TEXT("quest.basin_speaks")};
    TSet<FName> UnlockActions;
    const TSet<FName> AllowedUnlockActions = {
        TEXT("reward.wake_the_hall.city_mode"), TEXT("reward.wake_the_hall.adaptive_habitat"),
        TEXT("reward.a_place_to_stay.microgrid_blueprint"), TEXT("reward.a_place_to_stay.water_blueprint"),
        TEXT("reward.power_water_people.utility_systems"), TEXT("reward.nia_needs_a_job.operator_xp"),
        TEXT("reward.nia_needs_a_job.corner_exchange"), TEXT("reward.replacement_model.insight"),
        TEXT("reward.replacement_model.intelligence_auditor"), TEXT("reward.signal_in_foundation.axiom_fragment"),
        TEXT("reward.iron_at_border.forgeweave_contact"), TEXT("reward.basin_speaks.eden_trade_access")
    };
    FName PreviousUnlockAction;
    for (const FDAQuestContentUnlockRecord& Unlock : QuestContentUnlockRecords)
    {
        const FDAQuestSaveState* Quest = FindQuestState(Unlock.QuestId);
        if (!AllowedUnlockActions.Contains(Unlock.ActionId) || !AllowedContentQuestIds.Contains(Unlock.QuestId)
            || Quest == nullptr || Quest->ProgressState != EDAQuestProgressState::Completed
            || Unlock.SourceFingerprint != FDAFirstHourFrozenPolicy::ManifestFingerprint() || Unlock.Quantity < 1 || Unlock.Quantity > 100
            || Unlock.DefinitionId.IsNone() == Unlock.ContentId.IsNone()
            || (!PreviousUnlockAction.IsNone() && !PreviousUnlockAction.LexicalLess(Unlock.ActionId))
            || UnlockActions.Contains(Unlock.ActionId))
        { OutError = TEXT("Content unlocks require a completed canonical quest and a unique full semantic grant record."); return false; }
        if (Unlock.Type == EDAQuestContentUnlockType::CardInstance && Unlock.GrantedCardInstanceIds.Num() != Unlock.Quantity)
        { OutError = TEXT("Card rewards must persist every concrete granted card instance ID."); return false; }
        if (Unlock.Type != EDAQuestContentUnlockType::CardInstance && !Unlock.GrantedCardInstanceIds.IsEmpty())
        { OutError = TEXT("Only CardInstance grants may contain card instance IDs."); return false; }
        UnlockActions.Add(Unlock.ActionId); PreviousUnlockAction = Unlock.ActionId;
    }
    TSet<FString> BindingKeys;
    for (const FDAQuestObjectiveAssetBindingRecord& Binding : QuestObjectiveAssetBindings)
    {
        const FString Key = Binding.QuestId.ToString() + TEXT("|") + Binding.BindingId.ToString();
        const FDAWorldAssetRecord* Asset = WorldAssets.FindByPredicate([&Binding](const FDAWorldAssetRecord& A) { return A.WorldAssetId == Binding.WorldAssetId; });
        if (!AllowedContentQuestIds.Contains(Binding.QuestId) || Binding.BindingId.IsNone() || Binding.DefinitionId.IsNone()
            || !Binding.WorldAssetId.IsValid() || BindingKeys.Contains(Key) || Asset == nullptr || Asset->CardDefinitionId != Binding.DefinitionId)
        { OutError = TEXT("Objective WorldAsset bindings must be unique and resolve to a real matching campaign asset."); return false; }
        BindingKeys.Add(Key);
    }
    TSet<FName> EffectQuests;
    FName PreviousContentAction;
    for (const FDAQuestContentEffectRecord& Effect : QuestContentEffectRecords)
    {
        const FDAQuestSaveState* Quest = FindQuestState(Effect.QuestId);
        static const TMap<FName, TSet<FName>> AllowedBranches = {
            {TEXT("quest.replacement_model"), {TEXT("accept"), TEXT("modify"), TEXT("audit"), TEXT("reject")}},
            {TEXT("quest.agency_has_a_price"), {TEXT("agency_forum"), TEXT("retrain"), TEXT("automation_cap"), TEXT("reject")}},
            {TEXT("quest.iron_at_border"), {TEXT("accept"), TEXT("refuse"), TEXT("favorable_terms"), TEXT("defer")}}
        };
        const TSet<FName>* QuestBranches = AllowedBranches.Find(Effect.QuestId);
        if (Effect.ActionId.IsNone() || !AllowedContentQuestIds.Contains(Effect.QuestId) || Effect.ChoiceBranchTag.IsNone()
            || Effect.SourceFingerprint != FDAFirstHourFrozenPolicy::ManifestFingerprint() || Effect.WorldTick < 0 || Effect.SemanticEffects.IsEmpty()
            || Quest == nullptr || Quest->ProgressState != EDAQuestProgressState::Completed
            || Effect.WorldTick != Quest->LastTransitionWorldTick || EffectQuests.Contains(Effect.QuestId)
            || QuestBranches == nullptr || !QuestBranches->Contains(Effect.ChoiceBranchTag)
            || (!PreviousContentAction.IsNone() && !PreviousContentAction.LexicalLess(Effect.ActionId))
            || !HasUniqueNames(Effect.HistoryTags, true) || !HasUniqueNames(Effect.SemanticEffects, false))
        {
            OutError = TEXT("Quest content effects require immutable, finite, sorted semantic audit records.");
            return false;
        }
        if (!Effect.CitizenStoryState.IsNone() && !AllowedStoryStates.Contains(Effect.CitizenStoryState))
        { OutError = TEXT("Quest outcome story state is not an allowed canonical Nia state."); return false; }
        if ((!Effect.bHasDependencyDelta && Effect.DependencyDelta != 0)
            || (!Effect.bHasHumanAgencySupportDelta && Effect.HumanAgencySupportDelta != 0)
            || (!Effect.bHasCitizenRelationshipDelta && Effect.CitizenRelationshipDelta != 0)
            || !FMath::IsFinite(Effect.BaselineDependency) || !FMath::IsFinite(Effect.ResultDependency)
            || !FMath::IsFinite(Effect.BaselineHumanAgencySupport) || !FMath::IsFinite(Effect.ResultHumanAgencySupport)
            || !FMath::IsFinite(Effect.BaselineNiaTrust) || !FMath::IsFinite(Effect.ResultNiaTrust)
            || Effect.ResultDependency != FMath::Clamp(Effect.BaselineDependency + Effect.DependencyDelta, 0.0, 100.0)
            || Effect.ResultHumanAgencySupport != FMath::Clamp(Effect.BaselineHumanAgencySupport + Effect.HumanAgencySupportDelta, 0.0, 100.0)
            || Effect.ResultNiaTrust != FMath::Clamp(Effect.BaselineNiaTrust + Effect.CitizenRelationshipDelta, -100.0, 100.0))
        { OutError = TEXT("Quest effect baseline, authored delta, and aggregate result must reconcile exactly."); return false; }
        const FDAQuestNodeDefinition* Choice = Quest->DefinitionManifest.Nodes.FindByPredicate(
            [](const FDAQuestNodeDefinition& Node) { return Node.Type == EDAQuestNodeType::Choice; });
        const FDAQuestEdgeDefinition* SelectedEdge = Choice != nullptr ? Choice->Edges.FindByPredicate(
            [&Effect](const FDAQuestEdgeDefinition& Edge) { return Edge.BranchTag == Effect.ChoiceBranchTag; }) : nullptr;
        if (SelectedEdge == nullptr || (Quest->CurrentNodeId != SelectedEdge->TargetNodeId
            && !Quest->CompletedNodeIds.Contains(SelectedEdge->TargetNodeId)))
        { OutError = TEXT("Quest outcome audit must match the actual completed Task18 choice path."); return false; }
        if (Effect.QuestId == TEXT("quest.replacement_model"))
        {
            const FString ExpectedAction = FString(TEXT("action.replacement_model.")) + Effect.ChoiceBranchTag.ToString();
            bool bExact = Effect.ActionId == FName(*ExpectedAction);
            if (Effect.ChoiceBranchTag == TEXT("accept"))
                bExact = bExact && Effect.HistoryTags == TArray<FName>{TEXT("nia_automation_accepted")}
                    && Effect.CitizenStoryState == TEXT("story.nia.replacement_model.automation_accepted")
                    && Effect.SemanticEffects == TArray<FName>{TEXT("effect.capital_efficiency.increased")}
                    && Effect.bHasDependencyDelta && Effect.DependencyDelta == 6
                    && Effect.bHasHumanAgencySupportDelta && Effect.HumanAgencySupportDelta == -8
                    && Effect.bHasCitizenRelationshipDelta && Effect.CitizenRelationshipDelta == -5;
            else if (Effect.ChoiceBranchTag == TEXT("modify"))
                bExact = bExact && Effect.HistoryTags == TArray<FName>{TEXT("nia_jobs_preserved")}
                    && Effect.CitizenStoryState == TEXT("story.nia.replacement_model.jobs_preserved")
                    && Effect.SemanticEffects == TArray<FName>{TEXT("effect.capital_efficiency.smaller_increase")}
                    && Effect.bHasDependencyDelta && Effect.DependencyDelta == 2
                    && !Effect.bHasHumanAgencySupportDelta
                    && Effect.bHasCitizenRelationshipDelta && Effect.CitizenRelationshipDelta == 4;
            else if (Effect.ChoiceBranchTag == TEXT("audit"))
                bExact = bExact && Effect.HistoryTags == TArray<FName>{TEXT("nia_model_audited")}
                    && Effect.CitizenStoryState == TEXT("story.nia.replacement_model.model_audited")
                    && Effect.SemanticEffects == TArray<FName>{TEXT("effect.social_data.incomplete")}
                    && !Effect.bHasDependencyDelta && !Effect.bHasHumanAgencySupportDelta && !Effect.bHasCitizenRelationshipDelta
                    && UnlockActions.Contains(TEXT("reward.replacement_model.insight"))
                    && UnlockActions.Contains(TEXT("reward.replacement_model.intelligence_auditor"));
            else if (Effect.ChoiceBranchTag == TEXT("reject"))
                bExact = bExact && Effect.HistoryTags == TArray<FName>{TEXT("nia_jobs_preserved")}
                    && Effect.CitizenStoryState == TEXT("story.nia.replacement_model.jobs_protected")
                    && Effect.SemanticEffects == TArray<FName>{TEXT("effect.human_agency.supported"), TEXT("effect.ascendants.approval_lost")}
                    && !Effect.bHasDependencyDelta && !Effect.bHasHumanAgencySupportDelta && !Effect.bHasCitizenRelationshipDelta;
            if (!bExact) { OutError = TEXT("Replacement Model audit diverges from its frozen authored semantic transaction."); return false; }
            const FDACitizenStoryTransitionRecord* HistoricalStory = CitizenStoryTransitionRecords.FindByPredicate(
                [&Effect](const FDACitizenStoryTransitionRecord& Transition)
                { return Transition.CitizenId == TEXT("citizen.synara.nia_vale")
                    && Transition.SourceActionTag == Effect.ActionId && Transition.ResultStoryState == Effect.CitizenStoryState
                    && Transition.WorldTick == Effect.WorldTick; });
            if (HistoricalStory == nullptr)
            { OutError = TEXT("Replacement Model story audit must match its durable historical Nia transition."); return false; }
            const EDASynaraCapitalEfficiencyState ExpectedEfficiency = Effect.ChoiceBranchTag == TEXT("accept")
                ? EDASynaraCapitalEfficiencyState::Increased
                : Effect.ChoiceBranchTag == TEXT("modify") ? EDASynaraCapitalEfficiencyState::SmallerIncrease : EDASynaraCapitalEfficiencyState::Baseline;
            (void)ExpectedEfficiency;
        }
        if (Effect.QuestId == TEXT("quest.iron_at_border")
            && !UnlockActions.Contains(TEXT("reward.iron_at_border.forgeweave_contact")))
        { OutError = TEXT("Iron at the Border must grant its exact Forgeweave diplomacy contact."); return false; }
        if (Effect.QuestId == TEXT("quest.agency_has_a_price"))
        {
            const FString ExpectedAction = FString(TEXT("action.agency_petition.")) + Effect.ChoiceBranchTag.ToString();
            const bool bReject = Effect.ChoiceBranchTag == TEXT("reject");
            const FName ExpectedHistory = bReject ? FName(TEXT("agency_petition_rejected")) : FName(TEXT("agency_petition_supported"));
            FName ExpectedSemantic;
            if (Effect.ChoiceBranchTag == TEXT("agency_forum")) ExpectedSemantic = TEXT("effect.agency_forum.committed");
            else if (Effect.ChoiceBranchTag == TEXT("retrain")) ExpectedSemantic = TEXT("effect.displaced_workers.retrained");
            else if (Effect.ChoiceBranchTag == TEXT("automation_cap")) ExpectedSemantic = TEXT("effect.automation_cap.enacted");
            else ExpectedSemantic = TEXT("effect.agency_petition.rejected");
            if (Effect.ActionId != FName(*ExpectedAction) || Effect.HistoryTags != TArray<FName>{ExpectedHistory}
                || Effect.SemanticEffects != TArray<FName>{ExpectedSemantic} || !Effect.CitizenStoryState.IsNone()
                || Effect.bHasDependencyDelta || Effect.bHasHumanAgencySupportDelta || Effect.bHasCitizenRelationshipDelta)
            { OutError = TEXT("Agency petition audit diverges from its frozen authored semantic transaction."); return false; }
            const EDAAgencyPetitionResolution ExpectedResolution = Effect.ChoiceBranchTag == TEXT("agency_forum")
                ? EDAAgencyPetitionResolution::AgencyForum : Effect.ChoiceBranchTag == TEXT("retrain")
                    ? EDAAgencyPetitionResolution::RetrainWorkers : Effect.ChoiceBranchTag == TEXT("automation_cap")
                        ? EDAAgencyPetitionResolution::AutomationCap : EDAAgencyPetitionResolution::Rejected;
            (void)ExpectedResolution;
        }
        if (Effect.QuestId == TEXT("quest.iron_at_border"))
        {
            const FString ExpectedAction = FString(TEXT("action.iron_at_border.")) + Effect.ChoiceBranchTag.ToString();
            TArray<FName> ExpectedHistory;
            if (Effect.ChoiceBranchTag == TEXT("accept") || Effect.ChoiceBranchTag == TEXT("favorable_terms")) ExpectedHistory.Add(TEXT("forge_trade_supported"));
            TArray<FName> ExpectedSemantic;
            if (Effect.ChoiceBranchTag == TEXT("accept")) ExpectedSemantic = {TEXT("effect.forgeweave.trust_up"), TEXT("effect.forgeweave.dependence_up")};
            else if (Effect.ChoiceBranchTag == TEXT("refuse")) ExpectedSemantic = {TEXT("effect.forgeweave.trust_down")};
            else if (Effect.ChoiceBranchTag == TEXT("favorable_terms")) ExpectedSemantic = {TEXT("effect.forgeweave.respect_up"), TEXT("effect.forgeweave.dependence_up")};
            else ExpectedSemantic = {TEXT("effect.forgeweave.request_deferred")};
            if (Effect.ActionId != FName(*ExpectedAction) || Effect.HistoryTags != ExpectedHistory
                || Effect.SemanticEffects != ExpectedSemantic || !Effect.CitizenStoryState.IsNone()
                || Effect.bHasDependencyDelta || Effect.bHasHumanAgencySupportDelta || Effect.bHasCitizenRelationshipDelta)
            { OutError = TEXT("Iron at the Border audit diverges from its frozen authored semantic transaction."); return false; }
            const EDAIronBorderResolution ExpectedResolution = Effect.ChoiceBranchTag == TEXT("accept")
                ? EDAIronBorderResolution::Accepted : Effect.ChoiceBranchTag == TEXT("refuse")
                    ? EDAIronBorderResolution::Refused : Effect.ChoiceBranchTag == TEXT("favorable_terms")
                        ? EDAIronBorderResolution::FavorableTerms : EDAIronBorderResolution::Deferred;
            const EDADiplomaticTrend ExpectedTrust = Effect.ChoiceBranchTag == TEXT("accept") ? EDADiplomaticTrend::Increased
                : Effect.ChoiceBranchTag == TEXT("refuse") ? EDADiplomaticTrend::Decreased : EDADiplomaticTrend::Unchanged;
            const EDADiplomaticTrend ExpectedRespect = Effect.ChoiceBranchTag == TEXT("favorable_terms")
                ? EDADiplomaticTrend::Increased : EDADiplomaticTrend::Unchanged;
            const EDADiplomaticTrend ExpectedDependence = Effect.ChoiceBranchTag == TEXT("accept")
                || Effect.ChoiceBranchTag == TEXT("favorable_terms") ? EDADiplomaticTrend::Increased : EDADiplomaticTrend::Unchanged;
            (void)ExpectedResolution; (void)ExpectedTrust; (void)ExpectedRespect; (void)ExpectedDependence;
        }
        for (const FName HistoryTag : Effect.HistoryTags)
        {
            if (!CampaignHistory.Contains(HistoryTag))
            {
                OutError = TEXT("Quest content effect history must use the canonical campaign history ledger.");
                return false;
            }
        }
        EffectQuests.Add(Effect.QuestId); PreviousContentAction = Effect.ActionId;
    }
    for (const FName BranchingQuestId : {FName(TEXT("quest.replacement_model")), FName(TEXT("quest.agency_has_a_price")), FName(TEXT("quest.iron_at_border"))})
    {
        const FDAQuestSaveState* Quest = FindQuestState(BranchingQuestId);
        if (!bFirstHourTransactionInProgress && Quest != nullptr
            && Quest->ProgressState == EDAQuestProgressState::Completed && !EffectQuests.Contains(BranchingQuestId))
        { OutError = TEXT("Completed branching first-hour quest is missing its exact semantic outcome transaction."); return false; }
    }
    TSet<FName> CrisisQuests;
    TSet<FGuid> CrisisActions;
    for (const FDAQuestCrisisCompletionRecord& Crisis : QuestCrisisCompletionRecords)
    {
        const FDAQuestSaveState* Quest = FindQuestState(Crisis.QuestId);
        const FDANarrativeActionRecord* Action = FindActionRecord(Crisis.NarrativeActionId);
        const FDACitizenStoryTransitionRecord* StoryTransition = CitizenStoryTransitionRecords.FindByPredicate(
            [&Crisis](const FDACitizenStoryTransitionRecord& Transition)
            { return Transition.ResultStoryState == TEXT("story.nia.human_override.supported") && Transition.SourceActionId == Crisis.NarrativeActionId; });
        if (Crisis.QuestId != TEXT("quest.human_override") || Crisis.CompletionActionId != TEXT("action.quest.human_override.completed")
            || Quest == nullptr || Quest->ProgressState != EDAQuestProgressState::Completed
            || Quest->DefinitionManifest.SourceDefinitionId != TEXT("quest.human_override.v1")
            || Quest->DefinitionManifest.DefinitionFingerprint != TEXT("433693a4a1332f736fc8e4a08a565fad")
            || Quest->DefinitionManifest.DefinitionFingerprint != Crisis.QuestDefinitionFingerprint
            || Quest->DefinitionManifest.ComputeFingerprint() != Crisis.QuestDefinitionFingerprint
            || StoryTransition == nullptr
            || Action == nullptr || Action->bLegacyIdentityOnly
            || Action->NormalizedActionTags != TArray<FName>{Crisis.CompletionActionId}
            || Action->WorldTick <= Quest->LastTransitionWorldTick || Crisis.WorldTick <= Action->WorldTick
            || CrisisQuests.Contains(Crisis.QuestId) || CrisisActions.Contains(Crisis.NarrativeActionId))
        { OutError = TEXT("Crisis eligibility records must prove a real completed Task18 Human Override quest and semantic action."); return false; }
        CrisisQuests.Add(Crisis.QuestId);
        CrisisActions.Add(Crisis.NarrativeActionId);
    }
    if (WorldMapAuthorityRecords.Num() > 1)
    { OutError = TEXT("World map unlock is a unique typed transaction."); return false; }
    for (const FDAWorldMapAuthorityRecord& Unlock : WorldMapAuthorityRecords)
    {
        const FDAQuestContentUnlockRecord* Source = QuestContentUnlockRecords.FindByPredicate([&Unlock](const FDAQuestContentUnlockRecord& Candidate)
            { return Candidate.QuestId == Unlock.SourceQuestId && Candidate.ActionId == Unlock.SourceActionId; });
        if (Unlock.SourceQuestId != TEXT("quest.signal_in_foundation")
            || Unlock.SourceActionId != TEXT("reward.signal_in_foundation.axiom_fragment")
            || Unlock.WorldTick < 0 || Source == nullptr
            || Source->Type != EDAQuestContentUnlockType::AxiomArchiveFragment
            || Source->ContentId != TEXT("archive.axiom.fragment.01")
            || Source->WorldTick != Unlock.WorldTick)
        { OutError = TEXT("World map authority requires the canonical Signal in the Foundation archive action."); return false; }
    }
    if (UnlockActions.Contains(TEXT("reward.signal_in_foundation.axiom_fragment")) != (WorldMapAuthorityRecords.Num() == 1))
    { OutError = TEXT("Signal archive reward and typed world-map authority must exist together exactly once."); return false; }
    TSet<FGuid> AuditSourceActions;
    for (const FDAAuditEligibilitySourceRecord& Source : AuditEligibilitySourceRecords)
    {
        const FDANarrativeActionRecord* Action = FindActionRecord(Source.SourceActionId);
        const FName ExpectedTag = Source.EligibilityId == TEXT("Vision") ? FName(TEXT("capability.vision.available"))
            : Source.EligibilityId == TEXT("ResearchAction")
                ? FName(TEXT("action.research.replacement_model.completed")) : NAME_None;
        if (!Source.SourceActionId.IsValid() || Source.WorldTick < 0 || ExpectedTag.IsNone()
            || Source.SourceActionTag != ExpectedTag || AuditSourceActions.Contains(Source.SourceActionId)
            || Action == nullptr || Action->bLegacyIdentityOnly || Action->WorldTick != Source.WorldTick
            || Action->NormalizedActionTags != TArray<FName>{Source.SourceActionTag})
        { OutError = TEXT("Audit eligibility source must be a unique typed Vision or research transaction."); return false; }
        AuditSourceActions.Add(Source.SourceActionId);
    }
    TSet<FName> ProofEligibility;
    for (const FDAQuestEligibilityProofRecord& Proof : QuestEligibilityProofRecords)
    {
        const FDAAuditEligibilitySourceRecord* Source = AuditEligibilitySourceRecords.FindByPredicate(
            [&Proof](const FDAAuditEligibilitySourceRecord& Candidate)
            { return Candidate.EligibilityId == Proof.EligibilityId && Candidate.SourceActionId == Proof.SourceActionId; });
        if (Proof.QuestId != TEXT("quest.replacement_model") || Proof.BranchTag != TEXT("audit")
            || (Proof.EligibilityId != TEXT("Vision") && Proof.EligibilityId != TEXT("ResearchAction"))
            || ProofEligibility.Contains(Proof.EligibilityId)
            || Source == nullptr || Proof.SourceActionTag != Source->SourceActionTag
            || Proof.SourceWorldTick != Source->WorldTick || Source->WorldTick > Proof.WorldTick)
        { OutError = TEXT("Audit eligibility requires durable Vision or research action provenance."); return false; }
        ProofEligibility.Add(Proof.EligibilityId);
    }
    const FDAQuestContentEffectRecord* ReplacementEffect = QuestContentEffectRecords.FindByPredicate([](const FDAQuestContentEffectRecord& Effect)
        { return Effect.QuestId == TEXT("quest.replacement_model"); });
    const bool bAudit = ReplacementEffect != nullptr && ReplacementEffect->ChoiceBranchTag == TEXT("audit");
    const bool bHasAuditInsight = UnlockActions.Contains(TEXT("reward.replacement_model.insight"));
    const bool bHasAuditPath = UnlockActions.Contains(TEXT("reward.replacement_model.intelligence_auditor"));
    if (bAudit != (QuestEligibilityProofRecords.Num() == 1)
        || bAudit != bHasAuditInsight || bAudit != bHasAuditPath
        || (bAudit && QuestEligibilityProofRecords[0].WorldTick != ReplacementEffect->WorldTick))
    { OutError = TEXT("Audit reward provenance must exist only for the canonical audit outcome at its completion tick."); return false; }
    const int64 MinimumContentRevision = QuestContentUnlockRecords.Num() + QuestObjectiveAssetBindings.Num()
        + QuestContentEffectRecords.Num() + QuestCrisisCompletionRecords.Num()
        + CitizenStoryTransitionRecords.Num() + WorldMapAuthorityRecords.Num()
        + AuditEligibilitySourceRecords.Num() + QuestEligibilityProofRecords.Num();
    if (MutationRevision < MinimumContentRevision)
    { OutError = TEXT("Narrative revision cannot precede durable content transactions."); return false; }
    static const TMap<FName, TArray<FName>> RequiredCompletionActions = {
        {TEXT("quest.wake_the_hall"), {TEXT("reward.wake_the_hall.city_mode"), TEXT("reward.wake_the_hall.adaptive_habitat")}},
        {TEXT("quest.a_place_to_stay"), {TEXT("reward.a_place_to_stay.microgrid_blueprint"), TEXT("reward.a_place_to_stay.water_blueprint")}},
        {TEXT("quest.power_water_people"), {TEXT("reward.power_water_people.utility_systems")}},
        {TEXT("quest.nia_needs_a_job"), {TEXT("reward.nia_needs_a_job.operator_xp"), TEXT("reward.nia_needs_a_job.corner_exchange")}},
        {TEXT("quest.signal_in_foundation"), {TEXT("reward.signal_in_foundation.axiom_fragment")}},
        {TEXT("quest.basin_speaks"), {TEXT("reward.basin_speaks.eden_trade_access")}}
    };
    for (const TPair<FName, TArray<FName>>& Required : RequiredCompletionActions)
    {
        const FDAQuestSaveState* Quest = FindQuestState(Required.Key);
        if (!bFirstHourTransactionInProgress && Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed)
            for (const FName Action : Required.Value) if (!UnlockActions.Contains(Action))
            { OutError = TEXT("Completed first-hour quest is missing its exact authored reward transaction."); return false; }
    }
    const TMap<FName, FName> RequiredCompletionHistory = {
        {TEXT("quest.wake_the_hall"), TEXT("founder_hall_awake")},
        {TEXT("quest.basin_speaks"), TEXT("eden_watershed_supported")}
    };
    for (const TPair<FName, FName>& Required : RequiredCompletionHistory)
    {
        const FDAQuestSaveState* Quest = FindQuestState(Required.Key);
        if (!bFirstHourTransactionInProgress && Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed
            && !CampaignHistory.Contains(Required.Value))
        { OutError = TEXT("Completed first-hour quest is missing its canonical campaign history tag."); return false; }
    }
    return true;
}
