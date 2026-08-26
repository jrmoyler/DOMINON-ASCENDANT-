#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DA::Soak
{
    struct EventSnapshot
    {
        std::string EventId;
        std::string StageId;
        bool Active = false;
        std::size_t MaximumTransitions = 0;
    };

    struct QuestSnapshot
    {
        std::string QuestId;
        std::string NodeId;
        bool Active = false;
        bool Required = false;
    };

    class ProgressGuard final
    {
    public:
        ProgressGuard(const std::size_t InMaximumEnabledEvents,
                      const std::int64_t InMaximumStaleCycles)
            : MaximumEnabledEvents(InMaximumEnabledEvents),
              MaximumStaleCycles(InMaximumStaleCycles)
        {
        }

        bool ObserveCycle(const std::int64_t Cycle,
                          const std::vector<EventSnapshot>& Events,
                          const std::vector<QuestSnapshot>& Quests)
        {
            if (Cycle < 0 || (LastObservedCycle >= 0 && Cycle <= LastObservedCycle))
            {
                return false;
            }
            LastObservedCycle = Cycle;

            std::unordered_set<std::string> EventIds;
            for (const EventSnapshot& Event : Events)
            {
                if (Event.EventId.empty() || Event.StageId.empty()
                    || !EventIds.emplace(Event.EventId).second)
                {
                    EventRunaway = true;
                    continue;
                }
                const std::string State = Event.StageId + (Event.Active ? "|active" : "|terminal");
                auto Existing = EventStates.find(Event.EventId);
                if (Existing == EventStates.end())
                {
                    ++Emissions;
                    EventStates.emplace(Event.EventId, EventState{State, 0});
                }
                else if (Existing->second.State != State)
                {
                    Existing->second.State = State;
                    ++Existing->second.Transitions;
                    ++Transitions;
                    if (Existing->second.Transitions > Event.MaximumTransitions)
                    {
                        EventRunaway = true;
                    }
                }
            }
            if (Events.size() > MaximumEnabledEvents || Emissions > MaximumEnabledEvents)
            {
                EventRunaway = true;
            }

            std::unordered_set<std::string> QuestIds;
            for (const QuestSnapshot& Quest : Quests)
            {
                if (Quest.QuestId.empty() || Quest.NodeId.empty()
                    || !QuestIds.emplace(Quest.QuestId).second)
                {
                    StaleRequiredQuest = true;
                    continue;
                }
                if (!Quest.Required || !Quest.Active)
                {
                    QuestStates.erase(Quest.QuestId);
                    continue;
                }
                auto Existing = QuestStates.find(Quest.QuestId);
                if (Existing == QuestStates.end() || Existing->second.NodeId != Quest.NodeId)
                {
                    QuestStates[Quest.QuestId] = QuestState{Quest.NodeId, Cycle};
                }
                else if (Cycle - Existing->second.EnteredCycle > MaximumStaleCycles)
                {
                    StaleRequiredQuest = true;
                }
            }
            return !EventRunaway && !StaleRequiredQuest;
        }

        std::size_t EventEmissionCount() const { return Emissions; }
        std::size_t EventTransitionCount() const { return Transitions; }
        bool EventRunawayDetected() const { return EventRunaway; }
        bool StaleRequiredQuestDetected() const { return StaleRequiredQuest; }

    private:
        struct EventState
        {
            std::string State;
            std::size_t Transitions = 0;
        };

        struct QuestState
        {
            std::string NodeId;
            std::int64_t EnteredCycle = 0;
        };

        std::size_t MaximumEnabledEvents = 0;
        std::int64_t MaximumStaleCycles = 0;
        std::int64_t LastObservedCycle = -1;
        std::size_t Emissions = 0;
        std::size_t Transitions = 0;
        bool EventRunaway = false;
        bool StaleRequiredQuest = false;
        std::unordered_map<std::string, EventState> EventStates;
        std::unordered_map<std::string, QuestState> QuestStates;
    };
}
