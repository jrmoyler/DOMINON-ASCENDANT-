#include "Save/DASaveService.h"

#include "Content/DACardDefinition.h"
#include "Content/DAContentManifest.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/DASaveMigration.h"
#include "Save/DASaveJsonFields.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Algo/Reverse.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
    constexpr TCHAR SaveExtension[] = TEXT(".dasave");
    constexpr TCHAR TemporaryExtension[] = TEXT(".tmp");
    constexpr TCHAR BackupExtension[] = TEXT(".bak");
    constexpr TCHAR RecoveryExtension[] = TEXT(".recover");

    bool DeriveLegacyDaxtonRelationshipPrefix(FDACampaignSnapshot& Campaign,
        FString& OutError)
    {
        FDADaxtonCampaignState& Daxton = Campaign.DaxtonState;
        const FDADiplomaticRelationship* Relationship =
            Campaign.WorldState.Diplomacy.FindRelationship(
                TEXT("relationship.synara.forgeweave"));
        const FDADaxtonCanonicalActionRecord* ResolutionProof =
            Daxton.CanonicalActionRecords.FindByPredicate(
                [&Daxton](const FDADaxtonCanonicalActionRecord& Record)
                {
                    return Record.Kind == EDADaxtonCanonicalActionKind::ResolveLeader
                        && Record.ActionId == Daxton.ResolutionActionId
                        && Record.LeaderState == Daxton.LeaderState;
                });
        if (Relationship == nullptr || ResolutionProof == nullptr)
        {
            OutError = TEXT("Resolved schema-v16 Daxton authority has no canonical relationship resolution proof.");
            return false;
        }

        float Replayed[6] = {};
        int32 MatchingBoundary = INDEX_NONE;
        int32 MatchCount = 0;
        for (int32 Boundary = 0; Boundary <= Relationship->ReasonLedger.Num(); ++Boundary)
        {
            const FDADaxtonCanonicalProjection& Terminal = ResolutionProof->After;
            if (FMath::IsNearlyEqual(
                    Replayed[static_cast<int32>(EDADiplomaticMetric::Trust)],
                    Terminal.Trust, 0.001f)
                && FMath::IsNearlyEqual(
                    Replayed[static_cast<int32>(EDADiplomaticMetric::Respect)],
                    Terminal.Respect, 0.001f)
                && FMath::IsNearlyEqual(
                    Replayed[static_cast<int32>(EDADiplomaticMetric::Grievance)],
                    Terminal.Grievance, 0.001f))
            {
                FDACampaignSnapshot ChoiceCampaign = Campaign;
                ChoiceCampaign.DaxtonState.Phase = EDADaxtonEncounterPhase::PhaseThree;
                ChoiceCampaign.DaxtonState.bLeaderResolved = false;
                ChoiceCampaign.DaxtonState.ResolutionActionId.Invalidate();
                ChoiceCampaign.DaxtonState.ResolvedWorldTick = 0;
                FDADiplomaticRelationship* ChoiceRelationship =
                    ChoiceCampaign.WorldState.Diplomacy.FindRelationship(
                        TEXT("relationship.synara.forgeweave"));
                if (ChoiceRelationship != nullptr)
                {
                    ChoiceRelationship->Trust =
                        Replayed[static_cast<int32>(EDADiplomaticMetric::Trust)];
                    ChoiceRelationship->Respect =
                        Replayed[static_cast<int32>(EDADiplomaticMetric::Respect)];
                    ChoiceRelationship->Grievance =
                        Replayed[static_cast<int32>(EDADiplomaticMetric::Grievance)];
                    FString EligibilityError;
                    if (FDADaxtonAuthorityValidator::CanResolveLeaderState(
                        Daxton.LeaderState, ChoiceCampaign, EligibilityError))
                    {
                        MatchingBoundary = Boundary;
                        ++MatchCount;
                    }
                }
            }

            if (Boundary == Relationship->ReasonLedger.Num()) break;
            const FDADiplomaticReason& Reason = Relationship->ReasonLedger[Boundary];
            const int32 MetricIndex = static_cast<int32>(Reason.Metric);
            if (Reason.WorldTick > Daxton.ResolvedWorldTick
                || MetricIndex < 0 || MetricIndex >= UE_ARRAY_COUNT(Replayed)
                || !FMath::IsFinite(Replayed[MetricIndex] + Reason.Magnitude))
            {
                break;
            }
            Replayed[MetricIndex] += Reason.Magnitude;
        }

        if (MatchCount != 1)
        {
            OutError = MatchCount == 0
                ? TEXT("Schema-v16 relationship ledger has no eligible Daxton resolution prefix.")
                : TEXT("Schema-v16 relationship ledger has ambiguous eligible Daxton resolution prefixes.");
            return false;
        }
        Daxton.ResolutionRelationshipReasonCount = MatchingBoundary;
        Daxton.ResolutionRelationshipReasonMutationIds.Reset(MatchingBoundary);
        for (int32 Index = 0; Index < MatchingBoundary; ++Index)
        {
            Daxton.ResolutionRelationshipReasonMutationIds.Add(
                Relationship->ReasonLedger[Index].MutationId);
        }
        return true;
    }

    class FDAJsonDecodedKeyScanner
    {
    public:
        explicit FDAJsonDecodedKeyScanner(const FString& InDocument)
            : Document(InDocument)
        {
        }

        bool Scan(FString& OutError)
        {
            SkipWhitespace();
            if (!ScanValue(OutError)) return false;
            SkipWhitespace();
            if (Cursor != Document.Len())
            {
                OutError = TEXT("JSON document has trailing data after its root value.");
                return false;
            }
            return true;
        }

    private:
        static int32 HexDigit(const TCHAR Character)
        {
            if (Character >= TEXT('0') && Character <= TEXT('9')) return Character - TEXT('0');
            if (Character >= TEXT('a') && Character <= TEXT('f')) return 10 + Character - TEXT('a');
            if (Character >= TEXT('A') && Character <= TEXT('F')) return 10 + Character - TEXT('A');
            return INDEX_NONE;
        }

        void SkipWhitespace()
        {
            while (Cursor < Document.Len() && FChar::IsWhitespace(Document[Cursor])) ++Cursor;
        }

        bool ScanString(FString& OutDecoded, FString& OutError)
        {
            OutDecoded.Reset();
            if (Cursor >= Document.Len() || Document[Cursor++] != TEXT('"'))
            {
                OutError = TEXT("JSON object member name is not a string.");
                return false;
            }
            while (Cursor < Document.Len())
            {
                const TCHAR Character = Document[Cursor++];
                if (Character == TEXT('"')) return true;
                if (Character < 0x20)
                {
                    OutError = TEXT("JSON string contains an unescaped control character.");
                    return false;
                }
                if (Character != TEXT('\\'))
                {
                    OutDecoded.AppendChar(Character);
                    continue;
                }
                if (Cursor >= Document.Len())
                {
                    OutError = TEXT("JSON string ends in an incomplete escape.");
                    return false;
                }
                const TCHAR Escape = Document[Cursor++];
                switch (Escape)
                {
                case TEXT('"'): OutDecoded.AppendChar(TEXT('"')); break;
                case TEXT('\\'): OutDecoded.AppendChar(TEXT('\\')); break;
                case TEXT('/'): OutDecoded.AppendChar(TEXT('/')); break;
                case TEXT('b'): OutDecoded.AppendChar(TEXT('\b')); break;
                case TEXT('f'): OutDecoded.AppendChar(TEXT('\f')); break;
                case TEXT('n'): OutDecoded.AppendChar(TEXT('\n')); break;
                case TEXT('r'): OutDecoded.AppendChar(TEXT('\r')); break;
                case TEXT('t'): OutDecoded.AppendChar(TEXT('\t')); break;
                case TEXT('u'):
                {
                    if (Cursor + 4 > Document.Len())
                    {
                        OutError = TEXT("JSON Unicode escape is incomplete.");
                        return false;
                    }
                    uint32 CodeUnit = 0;
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        const int32 Digit = HexDigit(Document[Cursor++]);
                        if (Digit == INDEX_NONE)
                        {
                            OutError = TEXT("JSON Unicode escape contains a non-hex digit.");
                            return false;
                        }
                        CodeUnit = (CodeUnit << 4) | static_cast<uint32>(Digit);
                    }
                    if (CodeUnit >= 0xD800 && CodeUnit <= 0xDBFF)
                    {
                        if (Cursor + 6 > Document.Len() || Document[Cursor++] != TEXT('\\')
                            || Document[Cursor++] != TEXT('u'))
                        {
                            OutError = TEXT("JSON high surrogate is not followed by a Unicode low surrogate.");
                            return false;
                        }
                        uint32 LowSurrogate = 0;
                        for (int32 Index = 0; Index < 4; ++Index)
                        {
                            const int32 Digit = HexDigit(Document[Cursor++]);
                            if (Digit == INDEX_NONE)
                            {
                                OutError = TEXT("JSON Unicode low surrogate contains a non-hex digit.");
                                return false;
                            }
                            LowSurrogate = (LowSurrogate << 4) | static_cast<uint32>(Digit);
                        }
                        if (LowSurrogate < 0xDC00 || LowSurrogate > 0xDFFF)
                        {
                            OutError = TEXT("JSON Unicode surrogate pair has an invalid low surrogate.");
                            return false;
                        }
                        if constexpr (sizeof(TCHAR) >= sizeof(uint32))
                        {
                            const uint32 CodePoint = 0x10000
                                + ((CodeUnit - 0xD800) << 10) + (LowSurrogate - 0xDC00);
                            OutDecoded.AppendChar(static_cast<TCHAR>(CodePoint));
                        }
                        else
                        {
                            OutDecoded.AppendChar(static_cast<TCHAR>(CodeUnit));
                            OutDecoded.AppendChar(static_cast<TCHAR>(LowSurrogate));
                        }
                    }
                    else if (CodeUnit >= 0xDC00 && CodeUnit <= 0xDFFF)
                    {
                        OutError = TEXT("JSON Unicode string contains an unpaired low surrogate.");
                        return false;
                    }
                    else
                    {
                        OutDecoded.AppendChar(static_cast<TCHAR>(CodeUnit));
                    }
                    break;
                }
                default:
                    OutError = TEXT("JSON string contains an unsupported escape.");
                    return false;
                }
            }
            OutError = TEXT("JSON string is unterminated.");
            return false;
        }

        bool ScanObject(FString& OutError)
        {
            ++Cursor;
            SkipWhitespace();
            TSet<FString> DecodedKeys;
            if (Cursor < Document.Len() && Document[Cursor] == TEXT('}'))
            {
                ++Cursor;
                return true;
            }
            while (Cursor < Document.Len())
            {
                FString Key;
                if (!ScanString(Key, OutError)) return false;
                if (DecodedKeys.Contains(Key))
                {
                    OutError = TEXT("JSON object contains a duplicate decoded member name: ") + Key;
                    return false;
                }
                DecodedKeys.Add(MoveTemp(Key));
                SkipWhitespace();
                if (Cursor >= Document.Len() || Document[Cursor++] != TEXT(':'))
                {
                    OutError = TEXT("JSON object member has no colon.");
                    return false;
                }
                SkipWhitespace();
                if (!ScanValue(OutError)) return false;
                SkipWhitespace();
                if (Cursor < Document.Len() && Document[Cursor] == TEXT('}'))
                {
                    ++Cursor;
                    return true;
                }
                if (Cursor >= Document.Len() || Document[Cursor++] != TEXT(','))
                {
                    OutError = TEXT("JSON object members are not comma-separated.");
                    return false;
                }
                SkipWhitespace();
            }
            OutError = TEXT("JSON object is unterminated.");
            return false;
        }

        bool ScanArray(FString& OutError)
        {
            ++Cursor;
            SkipWhitespace();
            if (Cursor < Document.Len() && Document[Cursor] == TEXT(']'))
            {
                ++Cursor;
                return true;
            }
            while (Cursor < Document.Len())
            {
                if (!ScanValue(OutError)) return false;
                SkipWhitespace();
                if (Cursor < Document.Len() && Document[Cursor] == TEXT(']'))
                {
                    ++Cursor;
                    return true;
                }
                if (Cursor >= Document.Len() || Document[Cursor++] != TEXT(','))
                {
                    OutError = TEXT("JSON array values are not comma-separated.");
                    return false;
                }
                SkipWhitespace();
            }
            OutError = TEXT("JSON array is unterminated.");
            return false;
        }

        bool ScanValue(FString& OutError)
        {
            SkipWhitespace();
            if (Cursor >= Document.Len())
            {
                OutError = TEXT("JSON value is missing.");
                return false;
            }
            if (Document[Cursor] == TEXT('{')) return ScanObject(OutError);
            if (Document[Cursor] == TEXT('[')) return ScanArray(OutError);
            if (Document[Cursor] == TEXT('"'))
            {
                FString Ignored;
                return ScanString(Ignored, OutError);
            }
            const int32 Start = Cursor;
            while (Cursor < Document.Len() && !FChar::IsWhitespace(Document[Cursor])
                && Document[Cursor] != TEXT(',') && Document[Cursor] != TEXT('}')
                && Document[Cursor] != TEXT(']')) ++Cursor;
            if (Cursor == Start)
            {
                OutError = TEXT("JSON scalar value is malformed.");
                return false;
            }
            return true;
        }

        const FString& Document;
        int32 Cursor = 0;
    };

    bool ValidateNoDuplicateDecodedObjectMembers(const FString& Json, FString& OutError)
    {
        FDAJsonDecodedKeyScanner Scanner(Json);
        return Scanner.Scan(OutError);
    }

    void SynthesizeV10NodeTransitions(FDACampaignSnapshot& Campaign);
    bool SynthesizeV19CitySimulationState(FDACampaignSnapshot& Campaign, FString& OutError);

    bool HasJsonFieldRecursive(const TSharedPtr<FJsonObject>& Object, const FString& Field)
    {
        if (!Object.IsValid()) return false;
        if (Object->HasField(Field)) return true;
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            if (!Pair.Value.IsValid()) continue;
            if (Pair.Value->Type == EJson::Object
                && HasJsonFieldRecursive(Pair.Value->AsObject(), Field)) return true;
            if (Pair.Value->Type == EJson::Array)
                for (const TSharedPtr<FJsonValue>& Entry : Pair.Value->AsArray())
                    if (Entry.IsValid() && Entry->Type == EJson::Object
                        && HasJsonFieldRecursive(Entry->AsObject(), Field)) return true;
        }
        return false;
    }

    void RemoveJsonFieldRecursive(const TSharedPtr<FJsonObject>& Object, const FString& Field)
    {
        if (!Object.IsValid()) return;
        Object->RemoveField(Field);
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
        {
            if (!Pair.Value.IsValid()) continue;
            if (Pair.Value->Type == EJson::Object)
                RemoveJsonFieldRecursive(Pair.Value->AsObject(), Field);
            else if (Pair.Value->Type == EJson::Array)
                for (const TSharedPtr<FJsonValue>& Entry : Pair.Value->AsArray())
                    if (Entry.IsValid() && Entry->Type == EJson::Object)
                        RemoveJsonFieldRecursive(Entry->AsObject(), Field);
        }
    }

    class FDAWindowsSaveTransactionPlatform final : public IDASaveTransactionPlatform
    {
    public:
        virtual bool Commit(
            const FString& ActivePath,
            const FString& TemporaryPath,
            const FString& BackupPath,
            const bool bActiveExists,
            FString& OutError) override
        {
#if PLATFORM_WINDOWS
            if (!bActiveExists)
            {
                if (::MoveFileExW(*TemporaryPath, *ActivePath, MOVEFILE_WRITE_THROUGH) != 0)
                {
                    return true;
                }

                OutError = FString::Printf(TEXT("MoveFileExW initial publish failed with Windows error %u."), static_cast<uint32>(::GetLastError()));
                return false;
            }

            if (::ReplaceFileW(
                *ActivePath,
                *TemporaryPath,
                *BackupPath,
                REPLACEFILE_IGNORE_MERGE_ERRORS,
                nullptr,
                nullptr) != 0)
            {
                return true;
            }

            const uint32 ReplaceError = static_cast<uint32>(::GetLastError());
            const DWORD ActiveAttributes = ::GetFileAttributesW(*ActivePath);
            const DWORD BackupAttributes = ::GetFileAttributesW(*BackupPath);
            if (ActiveAttributes == INVALID_FILE_ATTRIBUTES && BackupAttributes != INVALID_FILE_ATTRIBUTES)
            {
                FString RecoveryError;
                RecoverActive(ActivePath, BackupPath, ActivePath + RecoveryExtension, RecoveryError);
            }

            OutError = FString::Printf(TEXT("ReplaceFileW failed with Windows error %u."), ReplaceError);
            return false;
#else
            OutError = TEXT("Transactional campaign replacement is supported only on Windows.");
            return false;
#endif
        }

        virtual bool RecoverActive(
            const FString& ActivePath,
            const FString& BackupPath,
            const FString& RecoveryPath,
            FString& OutError) override
        {
#if PLATFORM_WINDOWS
            if (::CopyFileW(*BackupPath, *RecoveryPath, FALSE) == 0)
            {
                OutError = FString::Printf(TEXT("CopyFileW recovery staging failed with Windows error %u."), static_cast<uint32>(::GetLastError()));
                return false;
            }

            const DWORD ActiveAttributes = ::GetFileAttributesW(*ActivePath);
            const bool bRecovered = ActiveAttributes == INVALID_FILE_ATTRIBUTES
                ? ::MoveFileExW(*RecoveryPath, *ActivePath, MOVEFILE_WRITE_THROUGH) != 0
                : ::ReplaceFileW(
                    *ActivePath,
                    *RecoveryPath,
                    nullptr,
                    REPLACEFILE_IGNORE_MERGE_ERRORS,
                    nullptr,
                    nullptr) != 0;

            if (!bRecovered)
            {
                const uint32 RecoveryError = static_cast<uint32>(::GetLastError());
                ::DeleteFileW(*RecoveryPath);
                OutError = FString::Printf(TEXT("Windows active-slot recovery failed with error %u."), RecoveryError);
                return false;
            }

            return true;
#else
            OutError = TEXT("Campaign recovery is supported only on Windows.");
            return false;
#endif
        }
    };

    bool SerializeJsonObject(const TSharedRef<FJsonObject>& JsonObject, FString& OutJson)
    {
        OutJson.Reset();
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
        return FJsonSerializer::Serialize(JsonObject, Writer);
    }

    bool DeserializeJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutJsonObject)
    {
        FString DuplicateError;
        if (!ValidateNoDuplicateDecodedObjectMembers(Json, DuplicateError)) return false;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
    }

    bool PromoteConflictHistoryAuthority(const TSharedRef<FJsonObject>& Campaign, FString& OutError)
    {
        const TSharedPtr<FJsonObject>* Conflict = nullptr;
        if (!Campaign->TryGetObjectField(FDASaveJsonFields::OperationConflict, Conflict)
            || Conflict == nullptr || !Conflict->IsValid())
        {
            OutError = TEXT("Campaign has no operation-conflict payload to migrate history.");
            return false;
        }
        TSet<FString> UniqueTags;
        const TArray<TSharedPtr<FJsonValue>>* CampaignHistory = nullptr;
        if (Campaign->TryGetArrayField(FDASaveJsonFields::HistoryTags, CampaignHistory) && CampaignHistory != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *CampaignHistory)
            {
                FString Tag;
                if (!Value.IsValid() || !Value->TryGetString(Tag) || Tag.IsEmpty())
                {
                    OutError = TEXT("Campaign history contains a malformed tag.");
                    return false;
                }
                UniqueTags.Add(Tag);
            }
        }
        const TArray<TSharedPtr<FJsonValue>>* ConflictHistory = nullptr;
        if ((*Conflict)->TryGetArrayField(FDASaveJsonFields::HistoryTags, ConflictHistory) && ConflictHistory != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *ConflictHistory)
            {
                FString Tag;
                if (!Value.IsValid() || !Value->TryGetString(Tag) || Tag.IsEmpty())
                {
                    OutError = TEXT("Operation-conflict history contains a malformed tag.");
                    return false;
                }
                UniqueTags.Add(Tag);
            }
        }
        const TCHAR* const AuditRecordFields[] = {TEXT("captureRecords"), TEXT("surrenderRecords")};
        for (const TCHAR* RecordField : AuditRecordFields)
        {
            const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
            if (!(*Conflict)->TryGetArrayField(RecordField, Records) || Records == nullptr)
            {
                continue;
            }
            for (const TSharedPtr<FJsonValue>& RecordValue : *Records)
            {
                if (!RecordValue.IsValid() || RecordValue->Type != EJson::Object)
                {
                    OutError = TEXT("Operation-conflict audit record is malformed.");
                    return false;
                }
                const TArray<TSharedPtr<FJsonValue>>* RecordHistory = nullptr;
                if (!RecordValue->AsObject()->TryGetArrayField(TEXT("history"), RecordHistory) || RecordHistory == nullptr)
                {
                    continue;
                }
                for (const TSharedPtr<FJsonValue>& Value : *RecordHistory)
                {
                    FString Tag;
                    if (!Value.IsValid() || !Value->TryGetString(Tag) || Tag.IsEmpty())
                    {
                        OutError = TEXT("Operation-conflict per-record audit contains a malformed tag.");
                        return false;
                    }
                    UniqueTags.Add(Tag);
                }
            }
        }
        TArray<FString> SortedTags = UniqueTags.Array();
        SortedTags.Sort();
        TArray<TSharedPtr<FJsonValue>> Promoted;
        for (const FString& Tag : SortedTags) Promoted.Add(MakeShared<FJsonValueString>(Tag));
        Campaign->SetArrayField(FDASaveJsonFields::HistoryTags, MoveTemp(Promoted));
        (*Conflict)->RemoveField(FDASaveJsonFields::HistoryTags);
        return true;
    }

    bool MigrateNarrativeActionsV7(const TSharedRef<FJsonObject>& Campaign, FString& OutError)
    {
        const TSharedPtr<FJsonObject>* Narrative = nullptr;
        if (!Campaign->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative)
            || Narrative == nullptr || !Narrative->IsValid())
        {
            OutError = TEXT("Schema-v7 campaign has no narrative payload.");
            return false;
        }
        const TCHAR* const StateFields[] = {TEXT("questStates"), TEXT("eventStates")};
        for (const TCHAR* StateField : StateFields)
        {
            const TArray<TSharedPtr<FJsonValue>>* States = nullptr;
            if ((*Narrative)->TryGetArrayField(StateField, States) && States != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& State : *States)
                {
                    if (!State.IsValid() || State->Type != EJson::Object
                        || !State->AsObject()->HasField(TEXT("definitionManifest")))
                    {
                        OutError = TEXT("Schema-v7 active narrative lacks immutable definition data; a trusted definition registry is required.");
                        return false;
                    }
                }
            }
        }
        if ((*Narrative)->HasField(FDASaveJsonFields::ActionRecords))
        {
            (*Narrative)->RemoveField(FDASaveJsonFields::AppliedActionIds);
            return true;
        }
        TArray<TSharedPtr<FJsonValue>> ActionRecords;
        const TArray<TSharedPtr<FJsonValue>>* AppliedIds = nullptr;
        if ((*Narrative)->TryGetArrayField(FDASaveJsonFields::AppliedActionIds, AppliedIds) && AppliedIds != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *AppliedIds)
            {
                FString ActionId;
                if (!Value.IsValid() || !Value->TryGetString(ActionId) || ActionId.IsEmpty())
                {
                    OutError = TEXT("Schema-v7 narrative contains a malformed applied ActionID.");
                    return false;
                }
                const TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
                Record->SetStringField(TEXT("actionId"), ActionId);
                Record->SetArrayField(TEXT("normalizedActionTags"), TArray<TSharedPtr<FJsonValue>>());
                Record->SetNumberField(TEXT("worldTick"), 0.0);
                Record->SetArrayField(TEXT("fulfilledPromiseIds"), TArray<TSharedPtr<FJsonValue>>());
                Record->SetArrayField(TEXT("breachedPromiseIds"), TArray<TSharedPtr<FJsonValue>>());
                Record->SetBoolField(TEXT("bLegacyIdentityOnly"), true);
                ActionRecords.Add(MakeShared<FJsonValueObject>(Record));
            }
        }
        ActionRecords.Sort([](const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right)
        {
            return Left->AsObject()->GetStringField(TEXT("actionId")) < Right->AsObject()->GetStringField(TEXT("actionId"));
        });
        (*Narrative)->SetArrayField(FDASaveJsonFields::ActionRecords, MoveTemp(ActionRecords));
        (*Narrative)->RemoveField(FDASaveJsonFields::AppliedActionIds);
        const TArray<TSharedPtr<FJsonValue>>* Promises = nullptr;
        if ((*Narrative)->TryGetArrayField(TEXT("promiseRecords"), Promises) && Promises != nullptr)
        {
            for (const TSharedPtr<FJsonValue>& Value : *Promises)
            {
                const TSharedPtr<FJsonObject> Promise = Value.IsValid() ? Value->AsObject() : nullptr;
                FString State;
                if (!Promise.IsValid() || !Promise->TryGetStringField(TEXT("state"), State)
                    || (State != TEXT("Active") && State != TEXT("Fulfilled") && State != TEXT("Breached")))
                {
                    OutError = TEXT("Schema-v7 promise state is malformed.");
                    return false;
                }
                Promise->RemoveField(TEXT("resolutionActionId"));
                if (State == TEXT("Active"))
                {
                    Promise->SetBoolField(TEXT("bLegacyResolutionWithoutAction"), false);
                    Promise->SetNumberField(TEXT("legacyResolutionSourceSchemaVersion"), 0.0);
                }
                else
                {
                    Promise->SetBoolField(TEXT("bLegacyResolutionWithoutAction"), true);
                    Promise->SetNumberField(TEXT("legacyResolutionSourceSchemaVersion"), 7.0);
                }
            }
        }
        return true;
    }

    bool PromoteNarrativeIntegrityV8(
        FDACampaignSnapshot& Snapshot,
        const bool bDirectSchemaV7Migration,
        FString& OutError)
    {
        for (FDAQuestSaveState& Quest : Snapshot.NarrativeState.QuestStates)
        {
            const FString StoredFingerprint = Quest.DefinitionManifest.DefinitionFingerprint;
            FDAQuestDefinitionManifest ValidatedManifest = Quest.DefinitionManifest;
            ValidatedManifest.RefreshFingerprint();
            if (!ValidatedManifest.Validate(OutError))
            {
                OutError = TEXT("Schema-v8 quest definition semantics are invalid.");
                return false;
            }
            const bool bStoredFingerprintValid = StoredFingerprint == Quest.DefinitionManifest.ComputeFingerprint()
                || StoredFingerprint == Quest.DefinitionManifest.ComputeLegacyFingerprintV1();
            if (!bStoredFingerprintValid)
            {
                OutError = TEXT("Schema-v8 quest definition fingerprint is stale or invalid.");
                return false;
            }
            Quest.DefinitionManifest = MoveTemp(ValidatedManifest);
        }
        for (FDAWorldEventSaveState& Event : Snapshot.NarrativeState.EventStates)
        {
            const FString StoredFingerprint = Event.DefinitionManifest.DefinitionFingerprint;
            FDAWorldEventDefinitionManifest ValidatedManifest = Event.DefinitionManifest;
            ValidatedManifest.RefreshFingerprint();
            if (!ValidatedManifest.Validate(OutError))
            {
                OutError = TEXT("Schema-v8 world-event definition semantics are invalid.");
                return false;
            }
            const bool bStoredFingerprintValid = StoredFingerprint == Event.DefinitionManifest.ComputeFingerprint()
                || StoredFingerprint == Event.DefinitionManifest.ComputeLegacyFingerprintV1();
            if (!bStoredFingerprintValid)
            {
                OutError = TEXT("Schema-v8 world-event definition fingerprint is stale or invalid.");
                return false;
            }
            Event.DefinitionManifest = MoveTemp(ValidatedManifest);
        }
        for (FDAPromiseRecord& Promise : Snapshot.NarrativeState.PromiseRecords)
        {
            if (Promise.State == EDAPromiseState::Active)
            {
                Promise.ResolutionActionId = FGuid();
                Promise.bLegacyResolutionWithoutAction = false;
                Promise.LegacyResolutionSourceSchemaVersion = 0;
                continue;
            }
            TArray<const FDANarrativeActionRecord*> MatchingActions;
            for (const FDANarrativeActionRecord& Action : Snapshot.NarrativeState.ActionRecords)
            {
                const bool bMatches = Promise.State == EDAPromiseState::Fulfilled
                    ? Action.FulfilledPromiseIds.Contains(Promise.PromiseId)
                    : Action.BreachedPromiseIds.Contains(Promise.PromiseId);
                if (!Action.bLegacyIdentityOnly && bMatches)
                {
                    MatchingActions.Add(&Action);
                }
            }
            if (MatchingActions.Num() == 1)
            {
                Promise.ResolutionActionId = MatchingActions[0]->ActionId;
                Promise.bLegacyResolutionWithoutAction = false;
                Promise.LegacyResolutionSourceSchemaVersion = 0;
            }
            else if (MatchingActions.IsEmpty()
                && bDirectSchemaV7Migration
                && Promise.bLegacyResolutionWithoutAction
                && Promise.LegacyResolutionSourceSchemaVersion == 7
                && !Promise.ResolutionActionId.IsValid())
            {
                // The schema-v7 migration stamped this exact resolved promise before entering v8.
            }
            else
            {
                OutError = TEXT("Schema-v8 resolved promise has no unambiguous semantic action or per-promise schema-v7 provenance.");
                return false;
            }
        }
        if (!SynthesizeV19CitySimulationState(Snapshot, OutError)) return false;
        return Snapshot.Validate(OutError);
    }

    bool TryReadExactV4WorldTickLexeme(const FString& SaveDocument, int64& OutWorldTick)
    {
        const FString WorldStateToken = FString::Printf(TEXT("\"%s\""), FDASaveJsonFields::WorldState);
        const FString WorldTickToken = FString::Printf(TEXT("\"%s\""), FDASaveJsonFields::CurrentWorldTick);
        const int32 WorldStateIndex = SaveDocument.Find(WorldStateToken, ESearchCase::CaseSensitive);
        const int32 WorldTickIndex = WorldStateIndex != INDEX_NONE
            ? SaveDocument.Find(WorldTickToken, ESearchCase::CaseSensitive, ESearchDir::FromStart, WorldStateIndex + WorldStateToken.Len())
            : INDEX_NONE;
        if (WorldTickIndex == INDEX_NONE)
        {
            return false;
        }
        if (SaveDocument.Find(
                WorldTickToken,
                ESearchCase::CaseSensitive,
                ESearchDir::FromStart,
                WorldTickIndex + WorldTickToken.Len()) != INDEX_NONE)
        {
            return false;
        }

        int32 Cursor = WorldTickIndex + WorldTickToken.Len();
        while (Cursor < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[Cursor]))
        {
            ++Cursor;
        }
        if (Cursor >= SaveDocument.Len() || SaveDocument[Cursor++] != TEXT(':'))
        {
            return false;
        }
        while (Cursor < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[Cursor]))
        {
            ++Cursor;
        }

        const int32 DigitStart = Cursor;
        uint64 Value = 0;
        constexpr uint64 LargestExactJsonInteger = 9007199254740992ULL;
        constexpr uint64 MaximumClockRestorableWorldTick = static_cast<uint64>(MAX_int64 / 5);
        while (Cursor < SaveDocument.Len() && FChar::IsDigit(SaveDocument[Cursor]))
        {
            const uint64 Digit = static_cast<uint64>(SaveDocument[Cursor] - TEXT('0'));
            if (Value > (LargestExactJsonInteger - Digit) / 10ULL)
            {
                return false;
            }
            Value = Value * 10ULL + Digit;
            ++Cursor;
        }
        if (Cursor == DigitStart
            || (Cursor - DigitStart > 1 && SaveDocument[DigitStart] == TEXT('0'))
            || Value > MaximumClockRestorableWorldTick)
        {
            return false;
        }
        while (Cursor < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[Cursor]))
        {
            ++Cursor;
        }
        if (Cursor >= SaveDocument.Len()
            || (SaveDocument[Cursor] != TEXT(',') && SaveDocument[Cursor] != TEXT('}')))
        {
            return false;
        }

        OutWorldTick = static_cast<int64>(Value);
        return true;
    }

    bool PromoteEmbeddedForgeweaveV5(const TSharedRef<FJsonObject>& Campaign, FString& OutError)
    {
        const TSharedPtr<FJsonObject>* World = nullptr;
        const TSharedPtr<FJsonObject>* Forgeweave = nullptr;
        const TSharedPtr<FJsonObject>* Conflict = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* DamageRecords = nullptr;
        if (!Campaign->TryGetObjectField(FDASaveJsonFields::WorldState, World)
            || World == nullptr
            || !(*World)->TryGetObjectField(FDASaveJsonFields::Forgeweave, Forgeweave)
            || Forgeweave == nullptr
            || !(*Forgeweave)->TryGetArrayField(TEXT("buildings"), Buildings)
            || !Campaign->TryGetArrayField(FDASaveJsonFields::WorldAssets, Assets)
            || !Campaign->TryGetObjectField(FDASaveJsonFields::OperationConflict, Conflict)
            || Conflict == nullptr
            || !(*Conflict)->TryGetArrayField(FDASaveJsonFields::StructuralDamageRecords, DamageRecords))
        {
            OutError = TEXT("Schema-v5 campaign lacks embedded Forgeweave authority containers.");
            return false;
        }

        struct FV5ForgeweavePromotion
        {
            TSharedPtr<FJsonObject> Building;
            TSharedPtr<FJsonObject> Asset;
            TSharedPtr<FJsonObject> Damage;
            FString Id;
            bool bHasDamage = false;
        };
        const auto HasCompleteAssetShape = [](const FJsonObject& Asset)
        {
            static const TCHAR* RequiredFields[] = {
                TEXT("worldAssetId"), TEXT("cardInstanceId"), TEXT("cardDefinitionId"), TEXT("cityId"),
                TEXT("gridOrigin"), TEXT("rotation"), TEXT("constructionState"),
                TEXT("constructionCyclesCompleted"), TEXT("constructionCyclesRequired"),
                TEXT("structuralIntegrity"), TEXT("ownerCivilizationId")
            };
            for (const TCHAR* Field : RequiredFields)
            {
                if (!Asset.HasField(Field))
                {
                    return false;
                }
            }
            return true;
        };
        const auto HasCompleteDamageShape = [](const FJsonObject& Damage)
        {
            static const TCHAR* RequiredFields[] = {
                TEXT("worldAssetId"), TEXT("cardDefinitionId"), TEXT("modules"), TEXT("bProductionDisabled")
            };
            for (const TCHAR* Field : RequiredFields)
            {
                if (!Damage.HasField(Field))
                {
                    return false;
                }
            }
            const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
            if (!Damage.TryGetArrayField(TEXT("modules"), Modules) || Modules == nullptr)
            {
                return false;
            }
            for (const TSharedPtr<FJsonValue>& ModuleValue : *Modules)
            {
                const TSharedPtr<FJsonObject> Module = ModuleValue->AsObject();
                if (!Module.IsValid()
                    || !Module->HasField(TEXT("moduleId"))
                    || !Module->HasField(TEXT("maximumHealth"))
                    || !Module->HasField(TEXT("currentHealth"))
                    || !Module->HasField(TEXT("bDisablesProduction"))
                    || !Module->HasField(TEXT("state")))
                {
                    return false;
                }
            }
            return true;
        };
        const auto IsCanonicalEmptyDamage = [](const FDAStructuralDamageRecord& Damage)
        {
            return !Damage.WorldAssetId.IsValid()
                && Damage.CardDefinitionId.IsNone()
                && Damage.Modules.IsEmpty()
                && !Damage.bProductionDisabled;
        };
        const auto ConstructionStateForIntegrity = [](const float Integrity)
        {
            if (Integrity <= 0.f)
            {
                return EDAConstructionState::Ruined;
            }
            if (Integrity <= 25.f)
            {
                return EDAConstructionState::Disabled;
            }
            if (Integrity <= 50.f)
            {
                return EDAConstructionState::Damaged;
            }
            return EDAConstructionState::Operational;
        };

        TArray<TSharedPtr<FJsonValue>> PromotedAssets = *Assets;
        TArray<TSharedPtr<FJsonValue>> PromotedDamage = *DamageRecords;
        TArray<FV5ForgeweavePromotion> PendingPromotions;
        TSet<FGuid> PromotedIds;
        for (const TSharedPtr<FJsonValue>& BuildingValue : *Buildings)
        {
            const TSharedPtr<FJsonObject> Building = BuildingValue->AsObject();
            const TSharedPtr<FJsonObject>* Asset = nullptr;
            if (!Building.IsValid()
                || !Building->TryGetObjectField(TEXT("assetRecord"), Asset)
                || Asset == nullptr)
            {
                OutError = TEXT("Schema-v5 Forgeweave building has no embedded assetRecord.");
                return false;
            }
            FString Id;
            bool bHasDamage = false;
            if (!(*Asset)->TryGetStringField(TEXT("worldAssetId"), Id)
                || Id.IsEmpty()
                || Building->HasField(TEXT("worldAssetId"))
                || !Building->TryGetBoolField(TEXT("bHasStructuralDamageRecord"), bHasDamage))
            {
                OutError = TEXT("Schema-v5 embedded Forgeweave ids and damage flags must be unique and explicit.");
                return false;
            }
            FDAWorldAssetRecord ParsedAsset;
            FDAForgeweaveBuildingState ParsedBuilding;
            FGuid ParsedId;
            if (!FGuid::Parse(Id, ParsedId)
                || !ParsedId.IsValid()
                || PromotedIds.Contains(ParsedId)
                || !HasCompleteAssetShape(**Asset)
                || !FJsonObjectConverter::JsonObjectToUStruct(
                    (*Asset)->ToSharedRef(), FDAWorldAssetRecord::StaticStruct(), &ParsedAsset, 0, 0)
                || !FJsonObjectConverter::JsonObjectToUStruct(
                    Building.ToSharedRef(), FDAForgeweaveBuildingState::StaticStruct(), &ParsedBuilding, 0, 0)
                || ParsedAsset.WorldAssetId != ParsedId
                || ParsedAsset.CardInstanceId.IsValid()
                || ParsedBuilding.BuildingId.IsNone()
                || !FDAForgeweaveCityState::IsVerticalSliceBuildCard(ParsedAsset.CardDefinitionId)
                || ParsedAsset.CityId != TEXT("settlement.ore_station_7")
                || ParsedAsset.OwnerCivilizationId != TEXT("civilization.forgeweave")
                || ParsedAsset.Rotation != 0
                || ParsedAsset.ConstructionCyclesCompleted != 0
                || ParsedAsset.ConstructionCyclesRequired != 0
                || !FMath::IsFinite(ParsedAsset.StructuralIntegrity)
                || ParsedAsset.StructuralIntegrity < 0.f
                || ParsedAsset.StructuralIntegrity > 100.f
                || ParsedAsset.ConstructionState != ConstructionStateForIntegrity(ParsedAsset.StructuralIntegrity)
                || ParsedBuilding.Footprint.X <= 0
                || ParsedBuilding.Footprint.Y <= 0
                || !FMath::IsFinite(ParsedBuilding.DeploymentCapital)
                || ParsedBuilding.DeploymentCapital < 0.f
                || !FMath::IsFinite(ParsedBuilding.UtilityDemand)
                || ParsedBuilding.UtilityDemand < 0.f
                || ParsedAsset.GridOrigin.X < 0
                || ParsedAsset.GridOrigin.Y < 0
                || ParsedAsset.GridOrigin.X > FDAForgeweaveCityState::IronheartGridWidth - ParsedBuilding.Footprint.X
                || ParsedAsset.GridOrigin.Y > FDAForgeweaveCityState::IronheartGridHeight - ParsedBuilding.Footprint.Y)
            {
                OutError = TEXT("Schema-v5 embedded Forgeweave asset shape is incomplete or incoherent.");
                return false;
            }
            const auto HasId = [ParsedId](const TSharedPtr<FJsonValue>& Value)
            {
                FString Candidate;
                FGuid CandidateId;
                return Value->AsObject().IsValid()
                    && Value->AsObject()->TryGetStringField(TEXT("worldAssetId"), Candidate)
                    && FGuid::Parse(Candidate, CandidateId)
                    && CandidateId == ParsedId;
            };
            if (PromotedAssets.ContainsByPredicate(HasId))
            {
                OutError = TEXT("Schema-v5 embedded Forgeweave asset collides with canonical WorldAssets.");
                return false;
            }
            const TSharedPtr<FJsonObject>* Damage = nullptr;
            FDAStructuralDamageRecord ParsedDamage;
            if (!Building->TryGetObjectField(TEXT("structuralDamage"), Damage)
                || Damage == nullptr
                || !FJsonObjectConverter::JsonObjectToUStruct(
                    (*Damage)->ToSharedRef(), FDAStructuralDamageRecord::StaticStruct(), &ParsedDamage, 0, 0))
            {
                OutError = TEXT("Schema-v5 embedded structural-damage presence must be explicit.");
                return false;
            }
            if (bHasDamage)
            {
                FDAOperationConflictSnapshot HistoricalConflict;
                HistoricalConflict.StructuralDamageRecords.Add(ParsedDamage);
                TArray<FDAWorldAssetRecord> HistoricalAssets;
                HistoricalAssets.Add(ParsedAsset);
                FString HistoricalError;
                if (!HasCompleteDamageShape(**Damage)
                    || ParsedDamage.WorldAssetId != ParsedAsset.WorldAssetId
                    || ParsedDamage.CardDefinitionId != ParsedAsset.CardDefinitionId
                    || PromotedDamage.ContainsByPredicate(HasId))
                {
                    OutError = TEXT("Schema-v5 embedded structural damage is absent or duplicates canonical authority.");
                    return false;
                }
                if (!HistoricalConflict.Validate(HistoricalAssets, HistoricalError))
                {
                    OutError = TEXT("Schema-v5 embedded structural damage is historically incoherent: ") + HistoricalError;
                    return false;
                }
            }
            else if (!IsCanonicalEmptyDamage(ParsedDamage))
            {
                OutError = TEXT("Schema-v5 absent structural damage must retain the canonical empty record.");
                return false;
            }

            FV5ForgeweavePromotion& Pending = PendingPromotions.AddDefaulted_GetRef();
            Pending.Building = Building;
            Pending.Asset = *Asset;
            Pending.Damage = *Damage;
            Pending.Id = Id;
            Pending.bHasDamage = bHasDamage;
            PromotedIds.Add(ParsedId);
        }

        for (const FV5ForgeweavePromotion& Pending : PendingPromotions)
        {
            PromotedAssets.Add(MakeShared<FJsonValueObject>(Pending.Asset));
            if (Pending.bHasDamage)
            {
                PromotedDamage.Add(MakeShared<FJsonValueObject>(Pending.Damage));
            }
            Pending.Building->SetStringField(TEXT("worldAssetId"), Pending.Id);
            Pending.Building->RemoveField(TEXT("assetRecord"));
            Pending.Building->RemoveField(TEXT("bHasStructuralDamageRecord"));
            Pending.Building->RemoveField(TEXT("structuralDamage"));
        }
        TArray<TSharedPtr<FJsonValue>> EmptyTransactions;
        (*Forgeweave)->SetArrayField(TEXT("actionTransactions"), MoveTemp(EmptyTransactions));
        Campaign->SetArrayField(FDASaveJsonFields::WorldAssets, MoveTemp(PromotedAssets));
        (*Conflict)->SetArrayField(FDASaveJsonFields::StructuralDamageRecords, MoveTemp(PromotedDamage));
        return true;
    }

    bool SynthesizeV5ActionTransactions(FDACampaignSnapshot& Snapshot, FString& OutError)
    {
        FDAForgeweaveCityState& City = Snapshot.WorldState.Forgeweave;
        TMap<FName, TPair<int64, int64>> TradeBalances;
        struct FRepairCursor
        {
            float Integrity = 0.f;
            TMap<FName, float> ModuleHealth;
        };
        TMap<FGuid, FRepairCursor> RepairCursors;
        TArray<FDAForgeweaveActionTransaction> ReverseTransactions;
        for (int32 Index = City.DecisionHistory.Num() - 1; Index >= 0; --Index)
        {
            const FDAForgeweaveDecisionRecord& Decision = City.DecisionHistory[Index];
            if (Decision.Type != EDARivalDecisionType::Trade
                && Decision.Type != EDARivalDecisionType::Repair
                && Decision.Type != EDARivalDecisionType::Fortify)
            {
                continue;
            }
            FDAForgeweaveActionTransaction Transaction;
            Transaction.WorldTick = Decision.WorldTick;
            Transaction.Type = Decision.Type;
            Transaction.AuthorityId = Decision.TargetBuildingId;
            Transaction.CapitalBefore = Decision.CapitalSpent;
            Transaction.ProductionBefore = Decision.ProductionSpent;
            if (Decision.Type == EDARivalDecisionType::Trade)
            {
                const FDATradeSpotOrderState* Order = Snapshot.WorldState.Trade.FindSpotOrder(Decision.TargetBuildingId);
                const FDATradeRouteState* Route = Order != nullptr ? Snapshot.WorldState.Trade.FindRoute(Order->RouteId) : nullptr;
                const FDARegionalTradeInventory* Source = Route != nullptr ? Snapshot.WorldState.Trade.FindInventory(Route->SourceRegionId) : nullptr;
                const FDARegionalTradeInventory* Destination = Route != nullptr ? Snapshot.WorldState.Trade.FindInventory(Route->DestinationRegionId) : nullptr;
                if (Order == nullptr || Route == nullptr || Source == nullptr || Destination == nullptr)
                {
                    OutError = TEXT("Schema-v5 Trade decision cannot resolve its durable spot order and inventories.");
                    return false;
                }
                TPair<int64, int64>& Closing = TradeBalances.FindOrAdd(
                    Route->RouteId,
                    TPair<int64, int64>(
                        Source->Stock.FindRef(Order->GoodId),
                        Destination->Stock.FindRef(Order->GoodId)));
                if (Closing.Value < Order->Quantity || Closing.Key > MAX_int64 - Order->Quantity)
                {
                    OutError = TEXT("Schema-v5 Trade opening balances cannot be reconstructed safely.");
                    return false;
                }
                Transaction.SourceQuantityAfter = Closing.Key;
                Transaction.DestinationQuantityAfter = Closing.Value;
                Transaction.SourceQuantityBefore = Closing.Key + Order->Quantity;
                Transaction.DestinationQuantityBefore = Closing.Value - Order->Quantity;
                Closing = TPair<int64, int64>(Transaction.SourceQuantityBefore, Transaction.DestinationQuantityBefore);
            }
            else if (Decision.Type == EDARivalDecisionType::Repair)
            {
                const FDAForgeweaveBuildingState* Building = City.Buildings.FindByPredicate(
                    [&Decision](const FDAForgeweaveBuildingState& Candidate) { return Candidate.BuildingId == Decision.TargetBuildingId; });
                FDAWorldAssetRecord* Asset = Building != nullptr ? Snapshot.FindWorldAssetRecord(Building->WorldAssetId) : nullptr;
                FDAStructuralDamageRecord* Damage = Asset != nullptr
                    ? Snapshot.OperationConflict.FindStructuralDamageRecord(Asset->WorldAssetId)
                    : nullptr;
                if (Building == nullptr || Asset == nullptr || Damage == nullptr || Building->DeploymentCapital <= 0.f)
                {
                    OutError = TEXT("Schema-v5 Repair decision cannot resolve embedded structural authority.");
                    return false;
                }
                FRepairCursor* ExistingCursor = RepairCursors.Find(Asset->WorldAssetId);
                if (ExistingCursor == nullptr)
                {
                    FRepairCursor Cursor;
                    Cursor.Integrity = Asset->StructuralIntegrity;
                    for (const FDAStructureModuleHealthRecord& Module : Damage->Modules)
                    {
                        Cursor.ModuleHealth.Add(Module.ModuleId, Module.CurrentHealth);
                    }
                    RepairCursors.Add(Asset->WorldAssetId, MoveTemp(Cursor));
                    ExistingCursor = RepairCursors.Find(Asset->WorldAssetId);
                }
                const float RepairPercent = Decision.CapitalSpent * 200.f / Building->DeploymentCapital;
                if (ExistingCursor == nullptr || !FMath::IsFinite(RepairPercent) || RepairPercent <= 0.f || RepairPercent > 25.f + 0.001f)
                {
                    OutError = TEXT("Schema-v5 Repair cost cannot reconstruct a bounded repair delta.");
                    return false;
                }
                Transaction.IntegrityAfter = ExistingCursor->Integrity;
                Transaction.IntegrityBefore = FMath::Max(0.f, Transaction.IntegrityAfter - RepairPercent);
                ExistingCursor->Integrity = Transaction.IntegrityBefore;
                for (const FDAStructureModuleHealthRecord& Module : Damage->Modules)
                {
                    float& ClosingHealth = ExistingCursor->ModuleHealth.FindChecked(Module.ModuleId);
                    FDAForgeweaveModuleRepairDelta Delta;
                    Delta.ModuleId = Module.ModuleId;
                    Delta.HealthAfter = ClosingHealth;
                    Delta.HealthBefore = FMath::Max(0.f, ClosingHealth - RepairPercent);
                    ClosingHealth = Delta.HealthBefore;
                    Transaction.ModuleDeltas.Add(Delta);
                }
            }
            else
            {
                const FDARegionState* Ironheart = Snapshot.WorldState.FindRegion(TEXT("region.ironheart"));
                const FDARegionActorState* Actor = Ironheart != nullptr
                    ? Ironheart->PersistentDelta.LocalActors.FindByPredicate(
                        [&Decision](const FDARegionActorState& Candidate) { return Candidate.ActorId == Decision.TargetBuildingId; })
                    : nullptr;
                if (Actor == nullptr || Actor->DefinitionId != TEXT("forgeweave.defense_cover"))
                {
                    OutError = TEXT("Schema-v5 Fortify decision cannot resolve its durable defense actor.");
                    return false;
                }
                Transaction.ActorTransform = Actor->Transform;
                Transaction.CoverTypeId = TEXT("cover.hardened");
            }
            ReverseTransactions.Add(MoveTemp(Transaction));
        }
        Algo::Reverse(ReverseTransactions);
        City.ActionTransactions = MoveTemp(ReverseTransactions);
        return true;
    }

    FString CalculateChecksum(
        const double SchemaVersion,
        const TSharedRef<FJsonObject>& Campaign,
        const double ContentVersion = -1.0,
        const double BuildVersion = -1.0)
    {
        const TSharedRef<FJsonObject> ChecksumMaterial = MakeShared<FJsonObject>();
        ChecksumMaterial->SetNumberField(FDASaveJsonFields::SchemaVersion, SchemaVersion);
        if (ContentVersion >= 0.0)
        {
            ChecksumMaterial->SetNumberField(FDASaveJsonFields::ContentVersion, ContentVersion);
        }
        if (BuildVersion >= 0.0)
        {
            ChecksumMaterial->SetNumberField(FDASaveJsonFields::BuildVersion, BuildVersion);
        }
        ChecksumMaterial->SetObjectField(FDASaveJsonFields::Campaign, Campaign);

        FString MaterialJson;
        if (!SerializeJsonObject(ChecksumMaterial, MaterialJson))
        {
            return {};
        }

        const FTCHARToUTF8 Utf8Material(*MaterialJson);
        const uint32 Checksum = FCrc::MemCrc32(Utf8Material.Get(), Utf8Material.Length());
        return FString::Printf(TEXT("%08X"), Checksum);
    }

    bool IsValidSlotName(const FString& Slot)
    {
        return !Slot.IsEmpty()
            && Slot == FPaths::MakeValidFileName(Slot)
            && !Slot.Contains(TEXT("/"))
            && !Slot.Contains(TEXT("\\"));
    }

    FString MakeSavePath(const FString& Directory, const FString& Slot)
    {
        return FPaths::Combine(Directory, Slot + SaveExtension);
    }

    FDASaveError MakeError(const EDASaveErrorCode Code, FString Message)
    {
        FDASaveError Error;
        Error.Code = Code;
        Error.Message = MoveTemp(Message);
        return Error;
    }

    FDASaveResult ValidateEnvelopeChecksum(const TSharedRef<FJsonObject>& Root)
    {
        FString StoredChecksum;
        double SchemaVersion = 0.0;
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root->TryGetStringField(FDASaveJsonFields::Checksum, StoredChecksum)
            || !Root->TryGetNumberField(FDASaveJsonFields::SchemaVersion, SchemaVersion)
            || !Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr
            || !Campaign->IsValid())
        {
            return FDASaveResult::Failure(EDASaveErrorCode::InvalidDocument, TEXT("Save envelope is missing its checksum or campaign payload."));
        }

        double ContentVersion = -1.0;
        double BuildVersion = -1.0;
        if (SchemaVersion >= 19.0)
        {
            if (!Root->TryGetNumberField(FDASaveJsonFields::ContentVersion, ContentVersion)
                || !Root->TryGetNumberField(FDASaveJsonFields::BuildVersion, BuildVersion)
                || !FMath::IsFinite(ContentVersion) || !FMath::IsFinite(BuildVersion)
                || ContentVersion != static_cast<double>(static_cast<int32>(ContentVersion))
                || BuildVersion != static_cast<double>(static_cast<int32>(BuildVersion)))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::InvalidDocument,
                    TEXT("Current save envelope requires exact integer contentVersion and buildVersion fields."));
            }
        }
        else if (Root->HasField(FDASaveJsonFields::ContentVersion)
            || Root->HasField(FDASaveJsonFields::BuildVersion))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::InvalidDocument,
                TEXT("Historical save envelope contains injected future content/build version authority."));
        }

        const FString ExpectedChecksum = CalculateChecksum(
            SchemaVersion, Campaign->ToSharedRef(), ContentVersion, BuildVersion);
        if (ExpectedChecksum.IsEmpty())
        {
            return FDASaveResult::Failure(EDASaveErrorCode::SerializationFailed, TEXT("Could not serialize save checksum material."));
        }

        if (!StoredChecksum.Equals(ExpectedChecksum, ESearchCase::IgnoreCase))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::ChecksumMismatch, TEXT("Save envelope checksum does not match its schema version and campaign payload."));
        }

        return FDASaveResult::Success();
    }

    FDASaveResult ValidateEnvelopeCompatibility(const TSharedRef<FJsonObject>& Root)
    {
        double SchemaVersion = 0.0;
        if (!Root->TryGetNumberField(FDASaveJsonFields::SchemaVersion, SchemaVersion))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::InvalidDocument,
                TEXT("Save envelope has no numeric schema version."));
        }
        if (SchemaVersion < 19.0)
        {
            return FDASaveResult::Success();
        }

        double ContentVersion = 0.0;
        double BuildVersion = 0.0;
        if (!Root->TryGetNumberField(FDASaveJsonFields::ContentVersion, ContentVersion)
            || !Root->TryGetNumberField(FDASaveJsonFields::BuildVersion, BuildVersion))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::InvalidDocument,
                TEXT("Current save envelope has no content/build compatibility fields."));
        }
        if (ContentVersion != FDASaveSchema::CurrentContentVersion)
        {
            return FDASaveResult::Failure(EDASaveErrorCode::UnsupportedSchema,
                FString::Printf(TEXT("Unsupported campaign contentVersion %d (runtime requires %d)."),
                    static_cast<int32>(ContentVersion), FDASaveSchema::CurrentContentVersion));
        }
        if (BuildVersion != FDASaveSchema::CurrentBuildVersion)
        {
            return FDASaveResult::Failure(EDASaveErrorCode::UnsupportedSchema,
                FString::Printf(TEXT("Incompatible campaign buildVersion %d (runtime requires %d)."),
                    static_cast<int32>(BuildVersion), FDASaveSchema::CurrentBuildVersion));
        }
        return FDASaveResult::Success();
    }

    bool ValidateCurrentCrisisCausalProofs(
        const FString& SaveDocument,
        const TSharedRef<FJsonObject>& Root,
        FString& OutError)
    {
        constexpr TCHAR ProofField[] = TEXT("jointCrisisHistoryRevisionAtResolution");
        constexpr uint64 LargestExactJsonInteger = 9007199254740992ULL;
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        const TSharedPtr<FJsonObject>* Crisis = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Records = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
            || Campaign == nullptr || !Campaign->IsValid()
            || !(*Campaign)->TryGetObjectField(TEXT("regionalCrisis"), Crisis)
            || Crisis == nullptr || !Crisis->IsValid()
            || !(*Crisis)->TryGetArrayField(TEXT("resolutionRecords"), Records)
            || Records == nullptr)
        {
            OutError = TEXT("Current campaign has no unambiguous regional-crisis resolution record array.");
            return false;
        }

        for (const TSharedPtr<FJsonValue>& Value : *Records)
        {
            if (!Value.IsValid() || Value->Type != EJson::Object
                || !Value->AsObject()->HasTypedField<EJson::Number>(ProofField))
            {
                OutError = TEXT("Every current crisis resolution must contain a numeric causal proof.");
                return false;
            }
        }

        const FString ProofToken = FString::Printf(TEXT("\"%s\""), ProofField);
        int32 PropertyCount = 0;
        int32 SearchFrom = 0;
        while (SearchFrom < SaveDocument.Len())
        {
            const int32 PropertyIndex = SaveDocument.Find(
                ProofToken, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
            if (PropertyIndex == INDEX_NONE)
            {
                break;
            }
            SearchFrom = PropertyIndex + ProofToken.Len();
            int32 Cursor = SearchFrom;
            while (Cursor < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[Cursor]))
            {
                ++Cursor;
            }
            if (Cursor >= SaveDocument.Len() || SaveDocument[Cursor] != TEXT(':'))
            {
                continue;
            }
            ++PropertyCount;
            ++Cursor;
            while (Cursor < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[Cursor]))
            {
                ++Cursor;
            }

            const int32 DigitStart = Cursor;
            uint64 Proof = 0;
            while (Cursor < SaveDocument.Len() && FChar::IsDigit(SaveDocument[Cursor]))
            {
                const uint64 Digit = static_cast<uint64>(SaveDocument[Cursor] - TEXT('0'));
                if (Proof > (LargestExactJsonInteger - Digit) / 10ULL)
                {
                    OutError = TEXT("Current crisis causal proof exceeds exact JSON integer bounds.");
                    return false;
                }
                Proof = Proof * 10ULL + Digit;
                ++Cursor;
            }
            if (Cursor == DigitStart
                || (Cursor - DigitStart > 1 && SaveDocument[DigitStart] == TEXT('0')))
            {
                OutError = TEXT("Current crisis causal proof is not an exact nonnegative integer.");
                return false;
            }
            while (Cursor < SaveDocument.Len() && FChar::IsWhitespace(SaveDocument[Cursor]))
            {
                ++Cursor;
            }
            if (Cursor >= SaveDocument.Len()
                || (SaveDocument[Cursor] != TEXT(',') && SaveDocument[Cursor] != TEXT('}')))
            {
                OutError = TEXT("Current crisis causal proof is fractional or has an ambiguous JSON representation.");
                return false;
            }
        }

        if (PropertyCount != Records->Num())
        {
            OutError = TEXT("Current crisis causal proof fields are missing, duplicated, or structurally ambiguous.");
            return false;
        }
        return true;
    }

    TResult<FDACampaignSnapshot, FDASaveError> LoadSnapshotFromPath(const FString& SavePath)
    {
        using FLoadResult = TResult<FDACampaignSnapshot, FDASaveError>;

        FString SaveDocument;
        if (!FFileHelper::LoadFileToString(SaveDocument, *SavePath))
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::ReadFailed, TEXT("Could not read campaign save slot.")));
        }

        TSharedPtr<FJsonObject> Root;
        if (!DeserializeJsonObject(SaveDocument, Root))
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument, TEXT("Campaign save slot is not valid JSON.")));
        }

        const FDASaveResult ChecksumResult = ValidateEnvelopeChecksum(Root.ToSharedRef());
        if (!ChecksumResult.IsSuccess())
        {
            return FLoadResult::Failure(ChecksumResult.Error);
        }

        const FDASaveResult CompatibilityResult = ValidateEnvelopeCompatibility(Root.ToSharedRef());
        if (!CompatibilityResult.IsSuccess())
        {
            return FLoadResult::Failure(CompatibilityResult.Error);
        }

        double SourceSchemaVersion = 0.0;
        FString CausalProofError;
        if (Root->TryGetNumberField(FDASaveJsonFields::SchemaVersion, SourceSchemaVersion)
            && SourceSchemaVersion >= 15.0
            && !ValidateCurrentCrisisCausalProofs(SaveDocument, Root.ToSharedRef(), CausalProofError))
        {
            return FLoadResult::Failure(MakeError(
                EDASaveErrorCode::InvalidDocument, MoveTemp(CausalProofError)));
        }

        const FDASaveResult MigrationResult = FDASaveMigration::MigrateToCurrent(SaveDocument);
        if (!MigrationResult.IsSuccess())
        {
            return FLoadResult::Failure(MigrationResult.Error);
        }

        if (!DeserializeJsonObject(SaveDocument, Root))
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::MigrationFailed, TEXT("Migrated campaign document is not valid JSON.")));
        }
        const FDASaveResult MigratedCompatibility = ValidateEnvelopeCompatibility(Root.ToSharedRef());
        if (!MigratedCompatibility.IsSuccess())
        {
            return FLoadResult::Failure(MigratedCompatibility.Error);
        }

        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr || !Campaign->IsValid())
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument, TEXT("Campaign save slot has no campaign payload.")));
        }
        const TSharedPtr<FJsonObject>* WorldState = nullptr;
        if (!(*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, WorldState)
            || WorldState == nullptr
            || !WorldState->IsValid())
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument, TEXT("Current campaign payload has no canonical regional-world state.")));
        }
        const TSharedPtr<FJsonObject>* ConquestState = nullptr;
        if (!(*Campaign)->TryGetObjectField(FDASaveJsonFields::ConquestState, ConquestState)
            || ConquestState == nullptr || !ConquestState->IsValid())
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument,
                TEXT("Current campaign payload has no canonical conquest state.")));
        }
        const TSharedPtr<FJsonObject>* DaxtonState = nullptr;
        if (!(*Campaign)->TryGetObjectField(FDASaveJsonFields::DaxtonState, DaxtonState)
            || DaxtonState == nullptr || !DaxtonState->IsValid())
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument,
                TEXT("Current campaign payload has no canonical Daxton encounter and Leader state.")));
        }
        const TSharedPtr<FJsonObject>* AscensionState = nullptr;
        if (!(*Campaign)->TryGetObjectField(FDASaveJsonFields::AscensionState, AscensionState)
            || AscensionState == nullptr || !AscensionState->IsValid())
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument,
                TEXT("Current campaign payload has no canonical First Ascension state.")));
        }
        const TSharedPtr<FJsonObject>* CitySimulationState = nullptr;
        if (!(*Campaign)->TryGetObjectField(FDASaveJsonFields::CitySimulationState, CitySimulationState)
            || CitySimulationState == nullptr || !CitySimulationState->IsValid())
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument,
                TEXT("Current campaign payload has no canonical persisted city simulation state.")));
        }

        FDACampaignSnapshot Snapshot;
        if (!FJsonObjectConverter::JsonObjectToUStruct(Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &Snapshot, 0, 0))
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument, TEXT("Campaign payload does not match the campaign snapshot schema.")));
        }

        FString ValidationError;
        if (!Snapshot.Validate(ValidationError))
        {
            return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidDocument, MoveTemp(ValidationError)));
        }

        return FLoadResult::Success(MoveTemp(Snapshot));
    }

    /** Keep the case-8 output an authentic historical v9 document before the v9->v10 step. */
    void StripFutureFieldsForHistoricalV9(const TSharedRef<FJsonObject>& Campaign)
    {
        Campaign->RemoveField(TEXT("synaraState"));
        Campaign->RemoveField(TEXT("liveSignals"));
        Campaign->RemoveField(TEXT("cityGridClaims"));
        Campaign->RemoveField(TEXT("regionalCrisis"));
        Campaign->RemoveField(FDASaveJsonFields::ConquestState);
        Campaign->RemoveField(FDASaveJsonFields::DaxtonState);
        Campaign->RemoveField(FDASaveJsonFields::AscensionState);
        RemoveJsonFieldRecursive(Campaign, TEXT("sourceCardInstanceId"));
        const TSharedPtr<FJsonObject>* WorldState = nullptr;
        if (Campaign->TryGetObjectField(FDASaveJsonFields::WorldState, WorldState)
            && WorldState != nullptr && WorldState->IsValid())
        {
            (*WorldState)->RemoveField(TEXT("clockAuthority"));
            (*WorldState)->RemoveField(TEXT("ecology"));
            const TSharedPtr<FJsonObject>* Trade = nullptr;
            if ((*WorldState)->TryGetObjectField(TEXT("trade"), Trade)
                && Trade != nullptr && Trade->IsValid())
                (*Trade)->RemoveField(TEXT("marketPriceModifiers"));
        }
        const TSharedPtr<FJsonObject>* Narrative = nullptr;
        if (!Campaign->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative)
            || Narrative == nullptr || !Narrative->IsValid()) return;
        static const TCHAR* FutureNarrativeFields[] = {
            TEXT("questContentUnlockRecords"), TEXT("questObjectiveAssetBindings"),
            TEXT("questCrisisCompletionRecords"), TEXT("questContentEffectRecords"),
            TEXT("citizenStoryStates"), TEXT("citizenStoryTransitionRecords"),
            TEXT("worldMapUnlockRecords"), TEXT("worldMapAuthorityRecords"),
            TEXT("auditEligibilitySourceRecords"), TEXT("questEligibilityProofRecords")
        };
        for (const TCHAR* Field : FutureNarrativeFields) (*Narrative)->RemoveField(Field);
        const TArray<TSharedPtr<FJsonValue>>* QuestStates = nullptr;
        if ((*Narrative)->TryGetArrayField(TEXT("questStates"), QuestStates) && QuestStates != nullptr)
            for (const TSharedPtr<FJsonValue>& Value : *QuestStates)
                if (Value.IsValid() && Value->Type == EJson::Object)
                {
                    Value->AsObject()->RemoveField(TEXT("nodeTransitionRecords"));
                    const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
                    if (Value->AsObject()->TryGetArrayField(TEXT("worldAssetBindings"), Bindings) && Bindings != nullptr)
                        for (const TSharedPtr<FJsonValue>& Binding : *Bindings)
                            if (Binding.IsValid() && Binding->Type == EJson::Object)
                            {
                                Binding->AsObject()->RemoveField(TEXT("bindWorldTick"));
                                Binding->AsObject()->RemoveField(TEXT("questDefinitionFingerprint"));
                            }
                }
    }

    void SynthesizeV10NodeTransitions(FDACampaignSnapshot& Campaign)
    {
        for (FDAQuestSaveState& Quest : Campaign.NarrativeState.QuestStates)
        {
            Quest.NodeTransitionRecords.Reset();
            for (int32 Index = 0; Index < Quest.CompletedNodeIds.Num(); ++Index)
            {
                FDAQuestNodeTransitionRecord& Transition = Quest.NodeTransitionRecords.Emplace_GetRef();
                Transition.CompletedNodeId = Quest.CompletedNodeIds[Index];
                Transition.EnteredNodeId = Index + 1 < Quest.CompletedNodeIds.Num()
                    ? Quest.CompletedNodeIds[Index + 1] : Quest.CurrentNodeId;
                Transition.WorldTick = Quest.LastTransitionWorldTick;
            }
            for (FDAQuestWorldAssetBinding& Binding : Quest.WorldAssetBindings)
            {
                Binding.BindWorldTick = Quest.StartedWorldTick;
                Binding.QuestDefinitionFingerprint = Quest.DefinitionManifest.DefinitionFingerprint;
            }
        }
    }

    void SynthesizeV11LiveSignals(FDACampaignSnapshot& Campaign)
    {
        if (Campaign.WorldState.bInitialized
            && Campaign.WorldState.CurrentWorldTick >= 0
            && Campaign.WorldState.CurrentWorldTick <= MAX_int64 / 5)
        {
            Campaign.WorldState.ClockAuthority.bCaptured = true;
            Campaign.WorldState.ClockAuthority.CurrentWorldTick = Campaign.WorldState.CurrentWorldTick;
            Campaign.WorldState.ClockAuthority.CurrentDevelopmentCycle = Campaign.WorldState.CurrentWorldTick * 5;
            Campaign.WorldState.ClockAuthority.AccumulatedSimulationSeconds = 0.0;
        }
        bool bChanged = false;
        for (const FDASynaraCitizenEmployment& Employment : Campaign.SynaraState.CitizenEmployment)
        {
            FDACampaignCitizenSignal& Citizen = Campaign.LiveSignals.Citizens.Emplace_GetRef();
            Citizen.CitizenId = Employment.CitizenId; Citizen.CityId = Employment.CityId;
            Citizen.JobId = Employment.JobId;
            FDACampaignJobOpeningSignal& Opening = Campaign.LiveSignals.JobOpenings.Emplace_GetRef();
            Opening.JobId = Employment.JobId; Opening.CityId = Employment.CityId;
            Opening.FacilityWorldAssetId = Employment.FacilityWorldAssetId; Opening.OpenPositions = 1;
            FDACampaignJobAssignmentSignal& Assignment = Campaign.LiveSignals.JobAssignments.Emplace_GetRef();
            Assignment.CitizenId = Employment.CitizenId; Assignment.JobId = Employment.JobId;
            Assignment.FacilityWorldAssetId = Employment.FacilityWorldAssetId;
            bChanged = true;
        }
        if (bChanged) ++Campaign.LiveSignals.MutationRevision;
    }

    bool SynthesizeV19CitySimulationState(FDACampaignSnapshot& Campaign, FString& OutError)
    {
        FDAVerticalSliceContentManifest Manifest;
        FDABuiltManifestContent RuntimeContent;
        TArray<FText> ContentErrors;
        if (Campaign.WorldState.bInitialized
            && (!FDAContentManifestPipeline::LoadCanonical(Manifest, ContentErrors)
                || !FDAContentManifestPipeline::BuildRuntimeContent(
                    Manifest, RuntimeContent, ContentErrors)))
        {
            OutError = TEXT("Schema-v19 migration could not resolve canonical runtime card definitions.");
            return false;
        }
        int32 StarterInstanceCount = 0;
        for (const TPair<FGuid, FCardInstance>& Pair : Campaign.CollectionState.Instances)
        {
            StarterInstanceCount += Pair.Value.AcquisitionSource == EDAAcquisitionSource::StarterDeck ? 1 : 0;
        }
        if (Campaign.WorldState.bInitialized
            && (Campaign.DeckState.GetInstanceIds().Num() != FDADeckState::RequiredDeckSize
                || StarterInstanceCount != FDADeckState::RequiredDeckSize))
        {
            if (RuntimeContent.Deck.GetInstanceIds().Num() != FDADeckState::RequiredDeckSize)
            {
                OutError = TEXT("Schema-v19 migration resolved an incomplete canonical starter deck.");
                return false;
            }
            FDACollectionState Existing = Campaign.CollectionState;
            Campaign.CollectionState = RuntimeContent.Collection;
            for (const TPair<FGuid, FCardInstance>& Pair : Existing.Instances)
            {
                if (Campaign.CollectionState.Instances.Contains(Pair.Key))
                {
                    continue;
                }
                FCardInstance ExistingCard = Pair.Value;
                if (ExistingCard.AcquisitionSource == EDAAcquisitionSource::StarterDeck)
                {
                    // A historical partial starter fixture is superseded by the exact
                    // authored partition. Preserve only deployed legacy identity as a
                    // non-starter discovery so its WorldAsset link and mastery survive.
                    if (!ExistingCard.WorldAssetId.IsValid()) continue;
                    ExistingCard.AcquisitionSource = EDAAcquisitionSource::WorldDiscovery;
                }
                Campaign.CollectionState.Instances.Add(Pair.Key, MoveTemp(ExistingCard));
            }
            Campaign.DeckState = RuntimeContent.Deck;
            Campaign.RebindRuntimeReferences();
        }

        const auto RegionMapPath = [](const FName RegionId) -> FSoftObjectPath
        {
            if (RegionId == TEXT("region.synara_frontier"))
                return FSoftObjectPath(TEXT("/Game/Maps/Regions/L_SynaraFrontier.L_SynaraFrontier"));
            if (RegionId == TEXT("region.ironheart"))
                return FSoftObjectPath(TEXT("/Game/Maps/Regions/L_Ironheart.L_Ironheart"));
            if (RegionId == TEXT("region.eden_basin"))
                return FSoftObjectPath(TEXT("/Game/Maps/Regions/L_EdenBasin.L_EdenBasin"));
            return FSoftObjectPath();
        };
        for (FDARegionState& Region : Campaign.WorldState.Regions)
        {
            if (Region.MapAssetPath.IsNull()) Region.MapAssetPath = RegionMapPath(Region.RegionId);
            if (Region.MapAssetPath.IsNull())
            {
                OutError = TEXT("Schema-v19 migration cannot infer an authored map for a persisted region.");
                return false;
            }
        }

        for (FDAForgeweaveBuildingState& Building : Campaign.WorldState.Forgeweave.Buildings)
        {
            FDAWorldAssetRecord* Asset = Campaign.FindWorldAssetRecord(Building.WorldAssetId);
            if (Asset == nullptr || Asset->OwnerCivilizationId != TEXT("civilization.forgeweave")
                || !FDAForgeweaveCityState::IsVerticalSliceBuildCard(Asset->CardDefinitionId))
            {
                OutError = TEXT("Schema-v19 migration found ambiguous Forgeweave building provenance.");
                return false;
            }
            Building.CardDefinitionId = Asset->CardDefinitionId;
            const FString ProvenanceSeed = TEXT("DA.Forgeweave.PlannerProvenance|")
                + Building.WorldAssetId.ToString(EGuidFormats::Digits);
            Building.ProvenanceId = FGuid(
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|A"))),
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|B"))),
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|C"))),
                FCrc::StrCrc32(*(ProvenanceSeed + TEXT("|D"))));
        }
        for (FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
        {
            const FDAForgeweaveBuildingState* RivalBuilding =
                Campaign.WorldState.Forgeweave.Buildings.FindByPredicate(
                    [&Asset](const FDAForgeweaveBuildingState& Building)
                    {
                        return Building.WorldAssetId == Asset.WorldAssetId;
                    });
            if (RivalBuilding != nullptr)
            {
                if (Campaign.DeckState.GetInstanceIds().Contains(Asset.CardInstanceId))
                {
                    OutError = TEXT("Schema-v19 migration refuses a rival card embedded in the player deck.");
                    return false;
                }
                Campaign.CollectionState.Instances.Remove(Asset.CardInstanceId);
                Asset.CardInstanceId.Invalidate();
                continue;
            }
            FCardInstance* Card = Campaign.CollectionState.FindInstance(Asset.CardInstanceId);
            if (Card == nullptr)
            {
                const FString Seed = TEXT("DA.Schema19.WorldCard|")
                    + Asset.WorldAssetId.ToString(EGuidFormats::Digits)
                    + TEXT("|") + Asset.CardDefinitionId.ToString();
                Asset.CardInstanceId = FGuid(
                    FCrc::StrCrc32(*(Seed + TEXT("|A"))),
                    FCrc::StrCrc32(*(Seed + TEXT("|B"))),
                    FCrc::StrCrc32(*(Seed + TEXT("|C"))),
                    FCrc::StrCrc32(*(Seed + TEXT("|D"))));
                Campaign.CollectionState.AddInstanceWithId(Asset.CardInstanceId,
                    Asset.CardDefinitionId, EDAAcquisitionSource::Conquest,
                    Campaign.WorldState.CurrentWorldTick);
                Card = Campaign.CollectionState.FindInstance(Asset.CardInstanceId);
            }
            if (Card != nullptr)
            {
                Card->DefinitionId = Asset.CardDefinitionId;
                Card->WorldAssetId = Asset.WorldAssetId;
                Card->RecoveryState = Asset.ConstructionState == EDAConstructionState::Ruined
                    ? EDARecoveryState::Ruined : EDARecoveryState::Deployed;
                if (Campaign.DeckState.GetInstanceIds().Contains(Card->InstanceId)
                    && !Campaign.DeckState.GetDeployed().Contains(Card->InstanceId)
                    && !Campaign.DeckState.TryRestoreDeployedInstance(Card->InstanceId, OutError))
                {
                    return false;
                }
            }
        }
        Campaign.RebindRuntimeReferences();

        FDACitySimulationState& City = Campaign.CitySimulationState;
        City = FDACitySimulationState{};
        City.bInitialized = Campaign.WorldState.bInitialized;
        City.Wallet = FDAWalletValues(
            static_cast<float>(Campaign.LiveSignals.Capital),
            static_cast<float>(Campaign.LiveSignals.Insight),
            static_cast<float>(Campaign.LiveSignals.Influence));
        City.Population = Campaign.LiveSignals.Population;
        City.ResolvedDevelopmentCycles = Campaign.LiveSignals.ResolvedDevelopmentCycles;
        City.ResolvedWorldTicks = Campaign.WorldState.CurrentWorldTick;
        City.UtilitySignals = Campaign.LiveSignals.UtilitySignals;

        for (const FDACampaignCitizenSignal& Signal : Campaign.LiveSignals.Citizens)
        {
            FDACitizenRecord& Citizen = City.Citizens.Emplace_GetRef();
            Citizen.CitizenId = Signal.CitizenId;
            Citizen.CityId = Signal.CityId;
            Citizen.HomeAssetId = Signal.HomeWorldAssetId;
            Citizen.JobId = Signal.JobId;
        }
        for (const FDACampaignJobOpeningSignal& Signal : Campaign.LiveSignals.JobOpenings)
        {
            FDAJobOpening& Opening = City.JobOpenings.Emplace_GetRef();
            Opening.JobId = Signal.JobId;
            Opening.CityId = Signal.CityId;
            Opening.FacilityWorldAssetId = Signal.FacilityWorldAssetId;
            Opening.OpenPositions = Signal.OpenPositions;
        }
        for (const FDACampaignJobAssignmentSignal& Signal : Campaign.LiveSignals.JobAssignments)
        {
            FDAJobAssignment& Assignment = City.JobAssignments.Emplace_GetRef();
            Assignment.CitizenId = Signal.CitizenId;
            Assignment.JobId = Signal.JobId;
            Assignment.FacilityWorldAssetId = Signal.FacilityWorldAssetId;
            Assignment.MatchQuality = EDAJobMatchQuality::Acceptable;
            Assignment.OutputMultiplier = 1.f;
        }
        for (const FDAWorldAssetRecord& Asset : Campaign.WorldAssets)
        {
            if (Asset.OwnerCivilizationId != TEXT("civilization.synara")) continue;
            const UDA_CardDefinition* Definition = RuntimeContent.FindDefinition(
                Asset.CardDefinitionId);
            if (Definition == nullptr)
            {
                OutError = TEXT("Schema-v19 migration cannot resolve a player facility definition unambiguously.");
                return false;
            }
            FDAFacilityContext& Facility = City.Facilities.Emplace_GetRef();
            Facility.AssetRecord = Asset;
            switch (Definition->CardType)
            {
            case EDACardType::Residential: Facility.FacilityType = EDAFacilityType::Residential; break;
            case EDACardType::Retail: Facility.FacilityType = EDAFacilityType::Retail; break;
            case EDACardType::Office: Facility.FacilityType = EDAFacilityType::Office; break;
            case EDACardType::Research: Facility.FacilityType = EDAFacilityType::Research; break;
            case EDACardType::Industrial: Facility.FacilityType = EDAFacilityType::Industrial; break;
            case EDACardType::Defense: Facility.FacilityType = EDAFacilityType::Defense; break;
            case EDACardType::Wonder: Facility.FacilityType = EDAFacilityType::Wonder; break;
            default: Facility.FacilityType = EDAFacilityType::Infrastructure; break;
            }
            int32 DeploymentCapital = 0;
            Definition->TryGetDeploymentCapital(DeploymentCapital);
            Facility.DeploymentCapital = static_cast<float>(DeploymentCapital);
            float Maintenance = 0.f;
            Definition->TryGetMaintenanceCapitalPerCycle(Maintenance);
            Facility.AuthoredMaintenanceCapitalPerCycle = Maintenance;
            Definition->TryGetBaseCapitalPerCycle(Facility.BaseOutput.Capital);
            Definition->TryGetBaseInsightPerCycle(Facility.BaseOutput.Insight);
            Definition->TryGetBaseInfluencePerCycle(Facility.BaseOutput.Influence);
            Facility.StaffingPercent = 100.f;
            int32 OpenPositions = 0;
            int32 AssignedPositions = 0;
            for (const FDACampaignJobOpeningSignal& Opening : Campaign.LiveSignals.JobOpenings)
                if (Opening.FacilityWorldAssetId == Asset.WorldAssetId)
                    OpenPositions += Opening.OpenPositions;
            for (const FDACampaignJobAssignmentSignal& Assignment : Campaign.LiveSignals.JobAssignments)
                AssignedPositions += Assignment.FacilityWorldAssetId == Asset.WorldAssetId ? 1 : 0;
            if (OpenPositions > 0)
                Facility.StaffingPercent = FMath::Clamp(
                    static_cast<float>(AssignedPositions) * 100.f / static_cast<float>(OpenPositions),
                    0.f, 100.f);
            Facility.bAutomated = Asset.CardDefinitionId == TEXT("fusion.autonomous_factory");
            Facility.DemandMultiplier = 1.f;
            Facility.StandardModifiers.Reset();
            float IndustrialModifier = 0.f;
            if (Facility.bAutomated
                && Definition->TryGetIndustrialThroughputModifier(IndustrialModifier))
                Facility.StandardModifiers.Emplace(
                    TEXT("AutonomousFactory.IndustrialThroughput"), IndustrialModifier);

            bool RequiredUtilitySeen[3] = {false, false, false};
            int32 RequiredUtilityDemand[3] = {0, 0, 0};
            Definition->TryGetUtilityPower(RequiredUtilityDemand[0]);
            Definition->TryGetUtilityWater(RequiredUtilityDemand[1]);
            Definition->TryGetUtilityData(RequiredUtilityDemand[2]);
            uint8 WorstSupply = static_cast<uint8>(EDACampaignUtilitySupply::FullySupplied);
            for (const FDACampaignUtilitySignal& Signal : City.UtilitySignals)
            {
                if (Signal.WorldAssetId != Asset.WorldAssetId) continue;
                WorstSupply = FMath::Max(WorstSupply, static_cast<uint8>(Signal.Supply));
                const int32 UtilityIndex = static_cast<int32>(Signal.Utility);
                if (UtilityIndex >= 0 && UtilityIndex < 3) RequiredUtilitySeen[UtilityIndex] = true;
            }
            for (int32 UtilityIndex = 0; UtilityIndex < 3; ++UtilityIndex)
            {
                if (RequiredUtilityDemand[UtilityIndex] > 0 && !RequiredUtilitySeen[UtilityIndex])
                {
                    OutError = TEXT("Schema-v19 migration cannot infer a required persisted facility utility signal.");
                    return false;
                }
            }
            Facility.UtilityState = static_cast<EDAUtilityState>(WorstSupply);
            if (Asset.StructuralIntegrity <= 25.f
                || Asset.ConstructionState == EDAConstructionState::Ruined)
            {
                Facility.MaintenanceCondition = EDAMaintenanceCondition::CriticallyDamaged;
                Facility.ConditionOutputMultiplier = 0.25f;
            }
            else if (Asset.StructuralIntegrity <= 50.f
                || Asset.ConstructionState == EDAConstructionState::Disabled
                || Asset.ConstructionState == EDAConstructionState::Damaged)
            {
                Facility.MaintenanceCondition = EDAMaintenanceCondition::Damaged;
                Facility.ConditionOutputMultiplier = 0.5f;
            }
            else if (Asset.StructuralIntegrity <= 75.f)
            {
                Facility.MaintenanceCondition = EDAMaintenanceCondition::Worn;
                Facility.ConditionOutputMultiplier = 0.75f;
            }
            else
            {
                Facility.MaintenanceCondition = EDAMaintenanceCondition::Healthy;
                Facility.ConditionOutputMultiplier = 1.f;
            }
            Facility.BespokeWonderMaintenanceRate = Facility.FacilityType == EDAFacilityType::Wonder
                && Facility.DeploymentCapital > 0.f
                ? Facility.AuthoredMaintenanceCapitalPerCycle / Facility.DeploymentCapital : 0.f;
        }
        OutError.Reset();
        return true;
    }

    void SynthesizeV12RegionalAuthorities(FDACampaignSnapshot& Campaign, const bool bSeedCitizens)
    {
        Campaign.CityGridClaims.Reset();
        if (Campaign.WorldState.bInitialized)
            Campaign.CityGridClaims.Add(FDACityGridClaimState::MakeCanonicalPlayerCapital());
        Campaign.RegionalCrisis = FDARegionalCrisisCampaignState{};
        if (!bSeedCitizens || !Campaign.WorldState.bInitialized) return;
        const auto AddCitizen = [&Campaign](const FName CitizenId, const FName CityId)
        {
            if (Campaign.LiveSignals.FindCitizen(CitizenId) != nullptr) return;
            FDACampaignCitizenSignal& Citizen = Campaign.LiveSignals.Citizens.Emplace_GetRef();
            Citizen.CitizenId = CitizenId; Citizen.CityId = CityId;
        };
        AddCitizen(TEXT("citizen.neutral.tal_arden"), TEXT("settlement.arden_reservoir"));
        AddCitizen(TEXT("citizen.forgeweave.mara_kest"), TEXT("city.ironheart"));
        AddCitizen(TEXT("citizen.eden.ori_sen"), TEXT("settlement.river_crossing"));
    }

    bool PromoteV13RegionalGraphIntegrity(FDACampaignSnapshot& Campaign, FString& OutError)
    {
        constexpr const TCHAR* OldFingerprint = TEXT("b7a7c0e15121b4db0a06514d0b06d37e7edf16bc");
        constexpr const TCHAR* NewFingerprint = TEXT("1bc31247330a8bc0af7103aaa8b70b51d8cd5d7a");
        FDARegionalCrisisCampaignState& Crisis = Campaign.RegionalCrisis;
        if (!Crisis.bTriggered) return Campaign.Validate(OutError);
        if (Crisis.ManifestFingerprint != OldFingerprint)
        { OutError = TEXT("Schema-v12 regional authority has a foreign or prematurely injected manifest fingerprint."); return false; }
        Crisis.ManifestFingerprint = NewFingerprint;
        for (FDAFoundryShortageResolutionRecord& Record : Crisis.ResolutionRecords)
        {
            if (Record.ManifestFingerprint != OldFingerprint)
            { OutError = TEXT("Schema-v12 crisis audit has a foreign manifest fingerprint."); return false; }
            Record.ManifestFingerprint = NewFingerprint;
        }
        FDAQuestSaveState* Quest = Campaign.NarrativeState.FindQuestState(TEXT("quest.foundry_shortage"));
        if (Quest != nullptr && Quest->ProgressState == EDAQuestProgressState::Completed)
        {
            if (Crisis.Resolution == EDAFoundryShortageResolution::Collapse)
            {
                Campaign.NarrativeState.QuestStates.RemoveAll([](const FDAQuestSaveState& Row)
                    { return Row.QuestId == TEXT("quest.foundry_shortage"); });
                ++Campaign.NarrativeState.MutationRevision;
            }
            else
            {
                const FName ChoiceId = Crisis.Resolution == EDAFoundryShortageResolution::IndustrialSupport
                    ? FName(TEXT("industrial_support"))
                    : Crisis.Resolution == EDAFoundryShortageResolution::EdenRestriction
                        ? FName(TEXT("eden_restriction"))
                    : Crisis.Resolution == EDAFoundryShortageResolution::BrokeredCompact
                        ? FName(TEXT("brokered_compact"))
                    : Crisis.Resolution == EDAFoundryShortageResolution::MarketExploitation
                        ? FName(TEXT("market_exploitation")) : NAME_None;
                if (ChoiceId.IsNone() || Crisis.ResolutionRecords.Num() != 1)
                { OutError = TEXT("Schema-v12 completed Foundry quest lacks one exact player crisis action."); return false; }
                const FGuid ActionId = Crisis.ResolutionRecords[0].ActionId;
                FDANarrativeActionRecord* Existing = Campaign.NarrativeState.ActionRecords.FindByPredicate(
                    [ActionId](const FDANarrativeActionRecord& Record){ return Record.ActionId == ActionId; });
                if (Existing == nullptr)
                {
                    FDANarrativeActionRecord& Action = Campaign.NarrativeState.ActionRecords.Emplace_GetRef();
                    Action.ActionId = ActionId; Action.WorldTick = Crisis.ResolvedWorldTick;
                    Action.NormalizedActionTags = {TEXT("regional.foundry_shortage"), ChoiceId};
                    Action.NormalizedActionTags.Sort([](FName A, FName B){ return A.LexicalLess(B); });
                    Campaign.NarrativeState.ActionRecords.Sort(
                        [](const FDANarrativeActionRecord& A, const FDANarrativeActionRecord& B)
                        { return A.ActionId.ToString(EGuidFormats::Digits) < B.ActionId.ToString(EGuidFormats::Digits); });
                    ++Campaign.NarrativeState.MutationRevision;
                }
                else if (!Existing->NormalizedActionTags.Contains(TEXT("regional.foundry_shortage"))
                    || !Existing->NormalizedActionTags.Contains(ChoiceId))
                { OutError = TEXT("Schema-v12 Foundry action ID collides with foreign narrative semantics."); return false; }
            }
        }
        return Campaign.Validate(OutError);
    }
}

FDASaveService::FDASaveService()
    : SaveDirectory(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaigns")))
    , TransactionPlatform(MakeShared<FDAWindowsSaveTransactionPlatform>())
{
}

FDASaveService::FDASaveService(FString InSaveDirectory)
    : SaveDirectory(MoveTemp(InSaveDirectory))
    , TransactionPlatform(MakeShared<FDAWindowsSaveTransactionPlatform>())
{
}

FDASaveService::FDASaveService(
    FString InSaveDirectory,
    TSharedRef<IDASaveTransactionPlatform> InTransactionPlatform)
    : SaveDirectory(MoveTemp(InSaveDirectory))
    , TransactionPlatform(MoveTemp(InTransactionPlatform))
{
}

FDASaveResult FDASaveService::SaveCampaign(const FDACampaignSnapshot& Snapshot, FString Slot) const
{
    if (!IsValidSlotName(Slot))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::InvalidSlot, TEXT("Save slot must be a non-empty file name without path components."));
    }

    FString ValidationError;
    if (!Snapshot.Validate(ValidationError))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::SerializationFailed, MoveTemp(ValidationError));
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.CreateDirectoryTree(*SaveDirectory))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::DirectoryCreationFailed, TEXT("Could not create the campaign save directory."));
    }

    const TSharedRef<FJsonObject> Campaign = MakeShared<FJsonObject>();
    if (!FJsonObjectConverter::UStructToJsonObject(FDACampaignSnapshot::StaticStruct(), &Snapshot, Campaign, 0, 0))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::SerializationFailed, TEXT("Could not serialize campaign snapshot records."));
    }

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(FDASaveJsonFields::SchemaVersion, CurrentSchemaVersion);
    Root->SetNumberField(FDASaveJsonFields::ContentVersion, CurrentContentVersion);
    Root->SetNumberField(FDASaveJsonFields::BuildVersion, CurrentBuildVersion);
    const FString Checksum = CalculateChecksum(
        CurrentSchemaVersion, Campaign, CurrentContentVersion, CurrentBuildVersion);
    if (Checksum.IsEmpty())
    {
        return FDASaveResult::Failure(EDASaveErrorCode::SerializationFailed, TEXT("Could not serialize save checksum material."));
    }
    Root->SetStringField(FDASaveJsonFields::Checksum, Checksum);
    Root->SetObjectField(FDASaveJsonFields::Campaign, Campaign);

    FString SaveDocument;
    if (!SerializeJsonObject(Root, SaveDocument))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::SerializationFailed, TEXT("Could not serialize save envelope."));
    }

    const FString ActivePath = MakeSavePath(SaveDirectory, Slot);
    const FString TemporaryPath = ActivePath + TemporaryExtension;
    const FString BackupPath = ActivePath + BackupExtension;

    if (!FFileHelper::SaveStringToFile(SaveDocument, *TemporaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::TemporaryWriteFailed, TEXT("Could not write the temporary campaign slot."));
    }

    FString WrittenDocument;
    TSharedPtr<FJsonObject> WrittenRoot;
    if (!FFileHelper::LoadFileToString(WrittenDocument, *TemporaryPath)
        || !DeserializeJsonObject(WrittenDocument, WrittenRoot))
    {
        PlatformFile.DeleteFile(*TemporaryPath);
        return FDASaveResult::Failure(EDASaveErrorCode::TemporaryWriteFailed, TEXT("Temporary campaign slot could not be read back."));
    }

    const FDASaveResult ChecksumResult = ValidateEnvelopeChecksum(WrittenRoot.ToSharedRef());
    if (!ChecksumResult.IsSuccess())
    {
        PlatformFile.DeleteFile(*TemporaryPath);
        return ChecksumResult;
    }

    const bool bHadActiveSave = PlatformFile.FileExists(*ActivePath);
    FString CommitError;
    if (!TransactionPlatform->Commit(ActivePath, TemporaryPath, BackupPath, bHadActiveSave, CommitError))
    {
        PlatformFile.DeleteFile(*TemporaryPath);
        return FDASaveResult::Failure(EDASaveErrorCode::AtomicReplaceFailed, MoveTemp(CommitError));
    }

    return FDASaveResult::Success();
}

TResult<FDACampaignSnapshot, FDASaveError> FDASaveService::LoadCampaign(FString Slot) const
{
    using FLoadResult = TResult<FDACampaignSnapshot, FDASaveError>;

    if (!IsValidSlotName(Slot))
    {
        return FLoadResult::Failure(MakeError(EDASaveErrorCode::InvalidSlot, TEXT("Save slot must be a non-empty file name without path components.")));
    }

    const FString ActivePath = MakeSavePath(SaveDirectory, Slot);
    const FString BackupPath = ActivePath + BackupExtension;
    const FString RecoveryPath = ActivePath + RecoveryExtension;
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    TOptional<FDASaveError> ActiveError;
    if (PlatformFile.FileExists(*ActivePath))
    {
        FLoadResult ActiveResult = LoadSnapshotFromPath(ActivePath);
        if (ActiveResult.HasValue())
        {
            return ActiveResult;
        }
        ActiveError.Emplace(ActiveResult.GetError());
    }

    if (PlatformFile.FileExists(*BackupPath))
    {
        FLoadResult BackupResult = LoadSnapshotFromPath(BackupPath);
        if (BackupResult.HasValue())
        {
            FString RecoveryError;
            TransactionPlatform->RecoverActive(ActivePath, BackupPath, RecoveryPath, RecoveryError);
            return BackupResult;
        }

        if (!ActiveError.IsSet())
        {
            return FLoadResult::Failure(BackupResult.GetError());
        }
    }

    if (ActiveError.IsSet())
    {
        return FLoadResult::Failure(ActiveError.GetValue());
    }

    return FLoadResult::Failure(MakeError(EDASaveErrorCode::SlotNotFound, TEXT("Campaign save slot and backup do not exist.")));
}

FDASaveResult FDASaveMigration::MigrateToCurrent(FString& SaveDocument)
{
    int64 RawExactWorldTick = 0;
    const bool bHasExactRawWorldTick = TryReadExactV4WorldTickLexeme(SaveDocument, RawExactWorldTick);
    TSharedPtr<FJsonObject> Root;
    if (!DeserializeJsonObject(SaveDocument, Root) || !Root->HasTypedField<EJson::Number>(FDASaveJsonFields::SchemaVersion))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::InvalidDocument, TEXT("Save document has no numeric schema version."));
    }

    const double NumericVersion = Root->GetNumberField(FDASaveJsonFields::SchemaVersion);
    if (!FMath::IsFinite(NumericVersion)
        || NumericVersion < FDASaveSchema::InitialSchemaVersion
        || NumericVersion > FDASaveSchema::CurrentSchemaVersion
        || NumericVersion != static_cast<double>(static_cast<int32>(NumericVersion)))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::UnsupportedSchema, FString::Printf(TEXT("Unsupported campaign schema version %s."), *FString::SanitizeFloat(NumericVersion)));
    }
    int32 Version = static_cast<int32>(NumericVersion);
    const int32 SourceVersion = Version;
    if (SourceVersion < 19)
    {
        const TSharedPtr<FJsonObject>* HistoricalCampaign = nullptr;
        if (Root->HasField(FDASaveJsonFields::ContentVersion)
            || Root->HasField(FDASaveJsonFields::BuildVersion)
            || (Root->TryGetObjectField(FDASaveJsonFields::Campaign, HistoricalCampaign)
                && HistoricalCampaign != nullptr && HistoricalCampaign->IsValid()
                && (*HistoricalCampaign)->HasField(FDASaveJsonFields::CitySimulationState)))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                TEXT("Historical save contains injected schema-v19 content/build or city-simulation authority."));
        }
    }
    else
    {
        double ContentVersion = 0.0;
        double BuildVersion = 0.0;
        if (!Root->TryGetNumberField(FDASaveJsonFields::ContentVersion, ContentVersion)
            || !Root->TryGetNumberField(FDASaveJsonFields::BuildVersion, BuildVersion)
            || ContentVersion != FDASaveSchema::CurrentContentVersion
            || BuildVersion != FDASaveSchema::CurrentBuildVersion)
        {
            return FDASaveResult::Failure(EDASaveErrorCode::UnsupportedSchema,
                TEXT("Schema-v19 save has incompatible contentVersion or buildVersion authority."));
        }
    }
    if (SourceVersion < 14)
    {
        const TSharedPtr<FJsonObject>* HistoricalCampaign = nullptr;
        if (Root->TryGetObjectField(FDASaveJsonFields::Campaign, HistoricalCampaign)
            && HistoricalCampaign != nullptr && HistoricalCampaign->IsValid()
            && (*HistoricalCampaign)->HasField(FDASaveJsonFields::ConquestState))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                TEXT("Historical save contains injected schema-v14 conquest authority."));
        }
    }
    if (SourceVersion < 16)
    {
        const TSharedPtr<FJsonObject>* HistoricalCampaign = nullptr;
        if (Root->TryGetObjectField(FDASaveJsonFields::Campaign, HistoricalCampaign)
            && HistoricalCampaign != nullptr && HistoricalCampaign->IsValid()
            && (*HistoricalCampaign)->HasField(FDASaveJsonFields::DaxtonState))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                TEXT("Historical save contains injected schema-v16 Daxton Leader authority."));
        }
    }
    if (SourceVersion < 17)
    {
        const TSharedPtr<FJsonObject>* HistoricalCampaign = nullptr;
        const TSharedPtr<FJsonObject>* HistoricalDaxton = nullptr;
        if (Root->TryGetObjectField(FDASaveJsonFields::Campaign, HistoricalCampaign)
            && HistoricalCampaign != nullptr && HistoricalCampaign->IsValid()
            && (*HistoricalCampaign)->TryGetObjectField(
                FDASaveJsonFields::DaxtonState, HistoricalDaxton)
            && HistoricalDaxton != nullptr && HistoricalDaxton->IsValid()
            && ((*HistoricalDaxton)->HasField(
                    TEXT("resolutionRelationshipReasonCount"))
                || (*HistoricalDaxton)->HasField(
                    TEXT("resolutionRelationshipReasonMutationIds"))))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                TEXT("Historical save contains injected schema-v17 Daxton relationship provenance."));
        }
    }
    if (SourceVersion < 18)
    {
        const TSharedPtr<FJsonObject>* HistoricalCampaign = nullptr;
        if (Root->TryGetObjectField(FDASaveJsonFields::Campaign, HistoricalCampaign)
            && HistoricalCampaign != nullptr && HistoricalCampaign->IsValid()
            && ((*HistoricalCampaign)->HasField(FDASaveJsonFields::AscensionState)
                || HasJsonFieldRecursive(*HistoricalCampaign, TEXT("sourceCardInstanceId"))))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                TEXT("Historical save contains injected schema-v18 First Ascension authority."));
        }
    }
    if (SourceVersion == 14)
    {
        const TSharedPtr<FJsonObject>* HistoricalCampaign = nullptr;
        const TSharedPtr<FJsonObject>* HistoricalCrisis = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* HistoricalRecords = nullptr;
        if (Root->TryGetObjectField(FDASaveJsonFields::Campaign, HistoricalCampaign)
            && HistoricalCampaign != nullptr && HistoricalCampaign->IsValid()
            && (*HistoricalCampaign)->TryGetObjectField(TEXT("regionalCrisis"), HistoricalCrisis)
            && HistoricalCrisis != nullptr && HistoricalCrisis->IsValid()
            && (*HistoricalCrisis)->TryGetArrayField(TEXT("resolutionRecords"), HistoricalRecords)
            && HistoricalRecords != nullptr
            && HistoricalRecords->ContainsByPredicate([](const TSharedPtr<FJsonValue>& Value)
            {
                return Value.IsValid() && Value->Type == EJson::Object
                    && Value->AsObject()->HasField(TEXT("jointCrisisHistoryRevisionAtResolution"));
            }))
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                TEXT("Schema-v14 save contains injected schema-v15 crisis causality."));
        }
    }

    while (Version < FDASaveSchema::CurrentSchemaVersion)
    {
        switch (Version)
        {
        case 1:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr || !Campaign->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Schema-v1 save has no campaign payload."));
            }

            if (!(*Campaign)->HasField(FDASaveJsonFields::HistoryTags))
            {
                TArray<TSharedPtr<FJsonValue>> EmptyHistoryTags;
                (*Campaign)->SetArrayField(FDASaveJsonFields::HistoryTags, MoveTemp(EmptyHistoryTags));
            }
            Version = 2;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 2:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr || !Campaign->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Schema-v2 save has no campaign payload."));
            }

            FDAOperationConflictSnapshot DefaultConflict;
            const TSharedRef<FJsonObject> ConflictJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDAOperationConflictSnapshot::StaticStruct(),
                &DefaultConflict,
                ConflictJson,
                0,
                0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not create default operation-conflict state."));
            }
            (*Campaign)->SetObjectField(FDASaveJsonFields::OperationConflict, ConflictJson);
            Version = 3;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 3:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr || !Campaign->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Schema-v3 save has no campaign payload."));
            }

            FDAWorldCampaignState DefaultWorldState;
            const TSharedRef<FJsonObject> WorldStateJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDAWorldCampaignState::StaticStruct(),
                &DefaultWorldState,
                WorldStateJson,
                0,
                0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not create default regional-world state."));
            }
            (*Campaign)->SetObjectField(FDASaveJsonFields::WorldState, WorldStateJson);
            Version = 4;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 4:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            const TSharedPtr<FJsonObject>* WorldState = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid()
                || !(*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, WorldState)
                || WorldState == nullptr || !WorldState->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Schema-v4 save has no regional-world payload."));
            }

            bool bWorldInitialized = false;
            const int64 ExactWorldTick = SourceVersion == 4 ? RawExactWorldTick : 0;
            if (!(*WorldState)->TryGetBoolField(FDASaveJsonFields::Initialized, bWorldInitialized)
                || (SourceVersion == 4 && !bHasExactRawWorldTick))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Schema-v4 world has invalid strategic time."));
            }

            FDAForgeweaveCityState ForgeweaveState;
            if (bWorldInitialized)
            {
                ForgeweaveState = FDAForgeweaveCityState::MakeVerticalSliceInitialState(
                    1701,
                    ExactWorldTick);
            }
            const TSharedRef<FJsonObject> ForgeweaveJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDAForgeweaveCityState::StaticStruct(),
                &ForgeweaveState,
                ForgeweaveJson,
                0,
                0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not create default Forgeweave strategic state."));
            }
            (*WorldState)->SetObjectField(FDASaveJsonFields::Forgeweave, ForgeweaveJson);
            Version = 5;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 5:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            FString Error;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr
                || !PromoteEmbeddedForgeweaveV5(Campaign->ToSharedRef(), Error)
                || !PromoteConflictHistoryAuthority(Campaign->ToSharedRef(), Error))
            {
                return FDASaveResult::Failure(
                    EDASaveErrorCode::MigrationFailed,
                    Error.IsEmpty() ? TEXT("Schema-v5 save has no promotable Forgeweave campaign.") : MoveTemp(Error));
            }
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                    Campaign->ToSharedRef(),
                    FDACampaignSnapshot::StaticStruct(),
                    &PromotedSnapshot,
                    0,
                    0))
            {
                return FDASaveResult::Failure(
                    EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v5 Forgeweave authorities could not be promoted canonically."));
            }
            SynthesizeV12RegionalAuthorities(PromotedSnapshot, false);
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, Error)
                || !SynthesizeV5ActionTransactions(PromotedSnapshot, Error)
                || !PromotedSnapshot.Validate(Error))
            {
                return FDASaveResult::Failure(
                    EDASaveErrorCode::MigrationFailed,
                    Error.IsEmpty() ? TEXT("Schema-v5 Forgeweave authorities could not be promoted canonically.") : MoveTemp(Error));
            }
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(),
                &PromotedSnapshot,
                PromotedCampaignJson,
                0,
                0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not serialize promoted schema-v6 campaign."));
            }
            PromotedCampaignJson->RemoveField(TEXT("cityGridClaims"));
            PromotedCampaignJson->RemoveField(TEXT("regionalCrisis"));
            PromotedCampaignJson->RemoveField(FDASaveJsonFields::ConquestState);
            const TSharedPtr<FJsonObject>* HistoricalWorld = nullptr;
            const TSharedPtr<FJsonObject>* HistoricalTrade = nullptr;
            if (PromotedCampaignJson->TryGetObjectField(FDASaveJsonFields::WorldState, HistoricalWorld)
                && HistoricalWorld != nullptr && HistoricalWorld->IsValid())
            {
                (*HistoricalWorld)->RemoveField(TEXT("ecology"));
                if ((*HistoricalWorld)->TryGetObjectField(TEXT("trade"), HistoricalTrade)
                    && HistoricalTrade != nullptr && HistoricalTrade->IsValid())
                    (*HistoricalTrade)->RemoveField(TEXT("marketPriceModifiers"));
            }
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 6;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 6:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr
                || !Campaign->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Schema-v6 save has no campaign payload."));
            }

            if (!(*Campaign)->HasField(FDASaveJsonFields::NarrativeState))
            {
                FDANarrativeCampaignState DefaultNarrativeState;
                const TSharedRef<FJsonObject> NarrativeJson = MakeShared<FJsonObject>();
                if (!FJsonObjectConverter::UStructToJsonObject(
                        FDANarrativeCampaignState::StaticStruct(),
                        &DefaultNarrativeState,
                        NarrativeJson,
                        0,
                        0))
                {
                    return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not create default narrative campaign state."));
                }
                (*Campaign)->SetObjectField(FDASaveJsonFields::NarrativeState, NarrativeJson);
            }
            Version = 7;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 7:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            FString Error;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid()
                || !PromoteConflictHistoryAuthority(Campaign->ToSharedRef(), Error)
                || !MigrateNarrativeActionsV7(Campaign->ToSharedRef(), Error))
            {
                return FDASaveResult::Failure(
                    EDASaveErrorCode::MigrationFailed,
                    Error.IsEmpty() ? TEXT("Schema-v7 campaign could not migrate narrative/history authorities.") : MoveTemp(Error));
            }
            Version = 8;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 8:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            FString Error;
            FDACampaignSnapshot HistoricalV9Dto;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid()
                || (*Campaign)->HasField(TEXT("liveSignals"))
                || !FJsonObjectConverter::JsonObjectToUStruct(
                    Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &HistoricalV9Dto, 0, 0))
            {
                return FDASaveResult::Failure(
                    EDASaveErrorCode::MigrationFailed,
                    Error.IsEmpty() ? TEXT("Schema-v8 narrative integrity could not be promoted.") : MoveTemp(Error));
            }
            SynthesizeV12RegionalAuthorities(HistoricalV9Dto, false);
            // The current validator needs timing evidence while checking the historical DTO;
            // the field is stripped from the v9 JSON and recreated only by case 9.
            SynthesizeV10NodeTransitions(HistoricalV9Dto);
            // Likewise, current validation requires canonical completed employment projection;
            // schema-v9 JSON strips it and case 10 recreates it deterministically.
            SynthesizeV11LiveSignals(HistoricalV9Dto);
            if (!PromoteNarrativeIntegrityV8(HistoricalV9Dto, SourceVersion <= 7, Error))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    Error.IsEmpty() ? TEXT("Schema-v8 narrative integrity could not be promoted.") : MoveTemp(Error));
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &HistoricalV9Dto, PromotedCampaignJson, 0, 0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Could not serialize promoted schema-v9 campaign."));
            }
            StripFutureFieldsForHistoricalV9(PromotedCampaignJson);
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 9;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 9:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v9 save has no campaign payload."));
            }
            if ((*Campaign)->HasField(TEXT("liveSignals")))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v9 save contains injected schema-v11 live campaign signals."));
            }
            if ((*Campaign)->HasField(TEXT("synaraState")))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v9 save contains injected schema-v10 campaign authority."));
            }
            const TSharedPtr<FJsonObject>* Narrative = nullptr;
            if ((*Campaign)->TryGetObjectField(FDASaveJsonFields::NarrativeState, Narrative)
                && Narrative != nullptr && Narrative->IsValid())
            {
                static const TMap<FString, EJson> V10OnlyFields = {
                    {TEXT("questContentUnlockRecords"), EJson::Array}, {TEXT("questObjectiveAssetBindings"), EJson::Array},
                    {TEXT("questCrisisCompletionRecords"), EJson::Array}, {TEXT("questContentEffectRecords"), EJson::Array},
                    {TEXT("citizenStoryStates"), EJson::Object}, {TEXT("citizenStoryTransitionRecords"), EJson::Array},
                    {TEXT("worldMapUnlockRecords"), EJson::Array}, {TEXT("worldMapAuthorityRecords"), EJson::Array},
                    {TEXT("auditEligibilitySourceRecords"), EJson::Array}, {TEXT("questEligibilityProofRecords"), EJson::Array}
                };
                for (const TPair<FString, EJson>& FieldSpec : V10OnlyFields)
                {
                    const TSharedPtr<FJsonValue>* Value = (*Narrative)->Values.Find(FieldSpec.Key);
                    if (Value == nullptr) continue;
                    const bool bCanonicalDefault = Value->IsValid() && (*Value)->Type == FieldSpec.Value
                        && ((FieldSpec.Value == EJson::Array && (*Value)->AsArray().IsEmpty())
                            || (FieldSpec.Value == EJson::Object && (*Value)->AsObject()->Values.IsEmpty())
                            || (FieldSpec.Value == EJson::Number && (*Value)->AsNumber() == 0.0));
                    if (!bCanonicalDefault)
                    {
                        return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                            TEXT("Schema-v9 save contains injected schema-v10 narrative content fields."));
                    }
                    (*Narrative)->RemoveField(FieldSpec.Key);
                }
                const TArray<TSharedPtr<FJsonValue>>* QuestStates = nullptr;
                if ((*Narrative)->TryGetArrayField(TEXT("questStates"), QuestStates) && QuestStates != nullptr)
                {
                    for (const TSharedPtr<FJsonValue>& QuestValue : *QuestStates)
                    {
                        if (!QuestValue.IsValid() || QuestValue->Type != EJson::Object) continue;
                        const TSharedRef<FJsonObject> QuestObject = QuestValue->AsObject().ToSharedRef();
                        const TSharedPtr<FJsonValue>* TransitionValue = QuestObject->Values.Find(TEXT("nodeTransitionRecords"));
                        if (TransitionValue != nullptr)
                        {
                            if (!TransitionValue->IsValid() || (*TransitionValue)->Type != EJson::Array
                                || !(*TransitionValue)->AsArray().IsEmpty())
                                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                                    TEXT("Schema-v9 save contains injected schema-v10 node transition records."));
                            QuestObject->RemoveField(TEXT("nodeTransitionRecords"));
                        }
                        const TArray<TSharedPtr<FJsonValue>>* Bindings = nullptr;
                        if (QuestObject->TryGetArrayField(TEXT("worldAssetBindings"), Bindings) && Bindings != nullptr)
                        {
                            for (const TSharedPtr<FJsonValue>& BindingValue : *Bindings)
                            {
                                if (!BindingValue.IsValid() || BindingValue->Type != EJson::Object) continue;
                                const TSharedPtr<FJsonObject> Binding = BindingValue->AsObject();
                                const TSharedPtr<FJsonValue>* BindTick = Binding->Values.Find(TEXT("bindWorldTick"));
                                const TSharedPtr<FJsonValue>* Fingerprint = Binding->Values.Find(TEXT("questDefinitionFingerprint"));
                                const bool bDefaultTick = BindTick == nullptr || (BindTick->IsValid()
                                    && (*BindTick)->Type == EJson::Number && (*BindTick)->AsNumber() == 0.0);
                                const bool bDefaultFingerprint = Fingerprint == nullptr || (Fingerprint->IsValid()
                                    && (*Fingerprint)->Type == EJson::String && (*Fingerprint)->AsString().IsEmpty());
                                if (!bDefaultTick || !bDefaultFingerprint)
                                    return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                                        TEXT("Schema-v9 save contains injected schema-v10 quest binding integrity fields."));
                                Binding->RemoveField(TEXT("bindWorldTick"));
                                Binding->RemoveField(TEXT("questDefinitionFingerprint"));
                            }
                        }
                    }
                }
            }
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v9 campaign could not be promoted to schema-v10 defaults."));
            }
            SynthesizeV10NodeTransitions(PromotedSnapshot);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v10 campaign defaults could not be materialized."));
            }
            PromotedCampaignJson->RemoveField(TEXT("liveSignals"));
            PromotedCampaignJson->RemoveField(TEXT("cityGridClaims"));
            PromotedCampaignJson->RemoveField(TEXT("regionalCrisis"));
            PromotedCampaignJson->RemoveField(FDASaveJsonFields::ConquestState);
            const TSharedPtr<FJsonObject>* PromotedWorld = nullptr;
            if (PromotedCampaignJson->TryGetObjectField(FDASaveJsonFields::WorldState, PromotedWorld)
                && PromotedWorld != nullptr && PromotedWorld->IsValid())
            {
                (*PromotedWorld)->RemoveField(TEXT("clockAuthority"));
                (*PromotedWorld)->RemoveField(TEXT("ecology"));
                const TSharedPtr<FJsonObject>* PromotedTrade = nullptr;
                if ((*PromotedWorld)->TryGetObjectField(TEXT("trade"), PromotedTrade)
                    && PromotedTrade != nullptr && PromotedTrade->IsValid())
                    (*PromotedTrade)->RemoveField(TEXT("marketPriceModifiers"));
            }
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 10;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 10:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v10 save has no campaign payload."));
            const TSharedPtr<FJsonValue>* LiveValue = (*Campaign)->Values.Find(TEXT("liveSignals"));
            if (LiveValue != nullptr)
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v10 save contains injected schema-v11 live campaign signals."));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v10 campaign could not be promoted to schema-v11 live signals."));
            SynthesizeV11LiveSignals(PromotedSnapshot);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v11 live campaign defaults could not be materialized."));
            PromotedCampaignJson->RemoveField(TEXT("cityGridClaims"));
            PromotedCampaignJson->RemoveField(TEXT("regionalCrisis"));
            PromotedCampaignJson->RemoveField(FDASaveJsonFields::ConquestState);
            const TSharedPtr<FJsonObject>* PromotedWorld = nullptr;
            const TSharedPtr<FJsonObject>* PromotedTrade = nullptr;
            if (PromotedCampaignJson->TryGetObjectField(FDASaveJsonFields::WorldState, PromotedWorld)
                && PromotedWorld != nullptr && PromotedWorld->IsValid())
            {
                (*PromotedWorld)->RemoveField(TEXT("ecology"));
                if ((*PromotedWorld)->TryGetObjectField(TEXT("trade"), PromotedTrade)
                    && PromotedTrade != nullptr && PromotedTrade->IsValid())
                    (*PromotedTrade)->RemoveField(TEXT("marketPriceModifiers"));
            }
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 11;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 11:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v11 save has no campaign payload."));
            if ((*Campaign)->HasField(TEXT("cityGridClaims"))
                || (*Campaign)->HasField(TEXT("regionalCrisis")))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v11 save contains injected schema-v12 regional campaign authority."));
            const TSharedPtr<FJsonObject>* HistoricalWorld = nullptr;
            const TSharedPtr<FJsonObject>* HistoricalTrade = nullptr;
            if ((*Campaign)->TryGetObjectField(FDASaveJsonFields::WorldState, HistoricalWorld)
                && HistoricalWorld != nullptr && HistoricalWorld->IsValid()
                && ((*HistoricalWorld)->HasField(TEXT("ecology"))
                    || ((*HistoricalWorld)->TryGetObjectField(TEXT("trade"), HistoricalTrade)
                        && HistoricalTrade != nullptr && HistoricalTrade->IsValid()
                        && (*HistoricalTrade)->HasField(TEXT("marketPriceModifiers")))))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v11 save contains injected schema-v12 market/ecology authority."));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v11 campaign could not be promoted to schema-v12 regional authorities."));
            SynthesizeV12RegionalAuthorities(PromotedSnapshot, true);
            FString ValidationError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, ValidationError)
                || !PromotedSnapshot.Validate(ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v12 regional defaults are invalid: ") + ValidationError);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v12 regional campaign defaults could not be materialized."));
            PromotedCampaignJson->RemoveField(FDASaveJsonFields::ConquestState);
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 12;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 12:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v12 save has no campaign payload."));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v12 campaign could not be promoted to regional graph integrity."));
            FString ValidationError;
            if (!PromoteV13RegionalGraphIntegrity(PromotedSnapshot, ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    ValidationError.IsEmpty() ? TEXT("Schema-v13 regional graph integrity could not be proven.")
                        : MoveTemp(ValidationError));
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v13 regional graph integrity could not be materialized."));
            PromotedCampaignJson->RemoveField(FDASaveJsonFields::ConquestState);
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 13;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 13:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v13 save has no campaign payload."));
            if ((*Campaign)->HasField(FDASaveJsonFields::ConquestState))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v13 save contains injected schema-v14 conquest authority."));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v13 campaign could not be promoted to schema-v14 conquest authority."));
            FString ValidationError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, ValidationError)
                || !PromotedSnapshot.ConquestState.Validate(ValidationError)
                || !PromotedSnapshot.Validate(ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v14 conquest defaults are invalid: ") + ValidationError);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v14 conquest authority could not be materialized."));
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 14;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 14:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v14 save has no campaign payload."));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v14 campaign could not be promoted to schema-v15 crisis causality."));
            const FDAConquestMeterMutation* JointHistory = PromotedSnapshot.ConquestState.FindMutation(
                TEXT("conquest.alliance.joint_crisis_success"));
            const int64 JointHistoryRevision = PromotedSnapshot.ConquestState.FindMutationRevision(
                TEXT("conquest.alliance.joint_crisis_success"));
            for (FDAFoundryShortageResolutionRecord& Record : PromotedSnapshot.RegionalCrisis.ResolutionRecords)
                Record.JointCrisisHistoryRevisionAtResolution = JointHistory != nullptr
                    && JointHistory->WorldTick < Record.WorldTick ? JointHistoryRevision : 0;
            FString ValidationError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, ValidationError)
                || !PromotedSnapshot.Validate(ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v15 crisis causality is invalid: ") + ValidationError);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v15 crisis causality could not be materialized."));
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 15;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 15:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v15 save has no campaign payload."));
            // External v15 future-authority injection was rejected before the migration loop.
            // Strip any current-struct default introduced by an earlier in-process migration step
            // so schema v16 remains the sole authority that materializes this field.
            (*Campaign)->RemoveField(FDASaveJsonFields::DaxtonState);
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v15 campaign could not be promoted to schema-v16 Daxton authority."));
            FString ValidationError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, ValidationError)
                || !PromotedSnapshot.Validate(ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v16 Daxton defaults are invalid: ") + ValidationError);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v16 Daxton authority could not be materialized."));
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 16;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 16:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v16 save has no campaign payload."));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(
                Campaign->ToSharedRef(), FDACampaignSnapshot::StaticStruct(),
                &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v16 campaign could not be promoted to schema-v17 Daxton relationship provenance."));

            FDADaxtonCampaignState& Daxton = PromotedSnapshot.DaxtonState;
            if (Daxton.bLeaderResolved)
            {
                FString PrefixError;
                if (!DeriveLegacyDaxtonRelationshipPrefix(
                    PromotedSnapshot, PrefixError))
                {
                    return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                        PrefixError);
                }
            }
            FString ValidationError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, ValidationError)
                || !PromotedSnapshot.Validate(ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v17 Daxton relationship provenance is invalid: ")
                        + ValidationError);
            const TSharedRef<FJsonObject> PromotedCampaignJson =
                MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot,
                PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v17 Daxton relationship provenance could not be materialized."));
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 17;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 17:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v17 save has no campaign payload."));
            // Earlier in-process promotion serializes the current reflected struct. Remove
            // that default before schema v18 becomes the sole authority to materialize it.
            (*Campaign)->RemoveField(FDASaveJsonFields::AscensionState);
            RemoveJsonFieldRecursive(*Campaign, TEXT("sourceCardInstanceId"));
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(Campaign->ToSharedRef(),
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v17 campaign could not be promoted to schema-v18 First Ascension authority."));
            FString ValidationError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, ValidationError)
                || !PromotedSnapshot.Validate(ValidationError))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v18 First Ascension defaults are invalid: ") + ValidationError);
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(FDACampaignSnapshot::StaticStruct(),
                &PromotedSnapshot, PromotedCampaignJson, 0, 0))
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v18 First Ascension authority could not be materialized."));
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Version = 18;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        case 18:
        {
            const TSharedPtr<FJsonObject>* Campaign = nullptr;
            if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign)
                || Campaign == nullptr || !Campaign->IsValid())
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v18 save has no campaign payload."));
            }
            // Earlier in-process migrations serialize the current reflected struct. Remove
            // that default so only this explicit schema step can author the persisted city DTO.
            (*Campaign)->RemoveField(FDASaveJsonFields::CitySimulationState);
            FDACampaignSnapshot PromotedSnapshot;
            if (!FJsonObjectConverter::JsonObjectToUStruct(Campaign->ToSharedRef(),
                FDACampaignSnapshot::StaticStruct(), &PromotedSnapshot, 0, 0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v18 campaign could not be promoted to schema-v19 city authority."));
            }
            FString SynthesisError;
            if (!SynthesizeV19CitySimulationState(PromotedSnapshot, SynthesisError))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    SynthesisError.IsEmpty()
                        ? TEXT("Schema-v19 city authority could not be reconstructed losslessly.")
                        : MoveTemp(SynthesisError));
            }
            const TSharedRef<FJsonObject> PromotedCampaignJson = MakeShared<FJsonObject>();
            if (!FJsonObjectConverter::UStructToJsonObject(FDACampaignSnapshot::StaticStruct(),
                &PromotedSnapshot, PromotedCampaignJson, 0, 0))
            {
                return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed,
                    TEXT("Schema-v19 persisted city authority could not be materialized."));
            }
            Root->SetObjectField(FDASaveJsonFields::Campaign, PromotedCampaignJson);
            Root->SetNumberField(FDASaveJsonFields::ContentVersion,
                FDASaveSchema::CurrentContentVersion);
            Root->SetNumberField(FDASaveJsonFields::BuildVersion,
                FDASaveSchema::CurrentBuildVersion);
            Version = 19;
            Root->SetNumberField(FDASaveJsonFields::SchemaVersion, Version);
            break;
        }
        default:
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, FString::Printf(TEXT("No migration is registered for schema version %d."), Version));
        }
    }

    if (Root->HasField(FDASaveJsonFields::Checksum))
    {
        const TSharedPtr<FJsonObject>* Campaign = nullptr;
        if (!Root->TryGetObjectField(FDASaveJsonFields::Campaign, Campaign) || Campaign == nullptr || !Campaign->IsValid())
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Migrated save has no campaign payload for checksum refresh."));
        }

        const FString MigratedChecksum = CalculateChecksum(
            Version,
            Campaign->ToSharedRef(),
            Version >= 19 ? FDASaveSchema::CurrentContentVersion : -1,
            Version >= 19 ? FDASaveSchema::CurrentBuildVersion : -1);
        if (MigratedChecksum.IsEmpty())
        {
            return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not refresh the migrated save checksum."));
        }
        Root->SetStringField(FDASaveJsonFields::Checksum, MigratedChecksum);
    }

    if (!SerializeJsonObject(Root.ToSharedRef(), SaveDocument))
    {
        return FDASaveResult::Failure(EDASaveErrorCode::MigrationFailed, TEXT("Could not serialize migrated campaign document."));
    }

    return FDASaveResult::Success();
}
