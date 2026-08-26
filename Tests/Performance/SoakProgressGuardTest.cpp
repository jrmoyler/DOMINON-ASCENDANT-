#include "Performance/DASoakProgressGuard.h"

#include <cassert>
#include <vector>

int main()
{
    using DA::Soak::EventSnapshot;
    using DA::Soak::ProgressGuard;
    using DA::Soak::QuestSnapshot;

    ProgressGuard Guard(6, 2);
    assert(Guard.ObserveCycle(1, {EventSnapshot{"event.grid", "warning", true, 2}},
        {QuestSnapshot{"quest.required", "start", true, true}}));
    assert(Guard.EventEmissionCount() == 1);
    assert(Guard.EventTransitionCount() == 0);
    assert(Guard.ObserveCycle(2, {EventSnapshot{"event.grid", "brownout", true, 2}},
        {QuestSnapshot{"quest.required", "choice", true, true}}));
    assert(Guard.EventEmissionCount() == 1);
    assert(Guard.EventTransitionCount() == 1);
    assert(Guard.ObserveCycle(3, {EventSnapshot{"event.grid", "resolved", false, 2}},
        {QuestSnapshot{"quest.required", "resolved", false, true}}));
    assert(!Guard.EventRunawayDetected());
    assert(!Guard.StaleRequiredQuestDetected());

    ProgressGuard Runaway(6, 5);
    assert(Runaway.ObserveCycle(1, {EventSnapshot{"event.loop", "a", true, 1}}, {}));
    assert(Runaway.ObserveCycle(2, {EventSnapshot{"event.loop", "b", true, 1}}, {}));
    assert(!Runaway.ObserveCycle(3, {EventSnapshot{"event.loop", "a", true, 1}}, {}));
    assert(Runaway.EventRunawayDetected());

    ProgressGuard StaleQuest(6, 2);
    assert(StaleQuest.ObserveCycle(10, {},
        {QuestSnapshot{"quest.stale", "objective", true, true}}));
    assert(StaleQuest.ObserveCycle(11, {},
        {QuestSnapshot{"quest.stale", "objective", true, true}}));
    assert(StaleQuest.ObserveCycle(12, {},
        {QuestSnapshot{"quest.stale", "objective", true, true}}));
    assert(!StaleQuest.ObserveCycle(13, {},
        {QuestSnapshot{"quest.stale", "objective", true, true}}));
    assert(StaleQuest.StaleRequiredQuestDetected());

    ProgressGuard TooManyEvents(1, 5);
    assert(!TooManyEvents.ObserveCycle(1,
        {EventSnapshot{"event.one", "a", true, 1}, EventSnapshot{"event.two", "a", true, 1}}, {}));
    assert(TooManyEvents.EventRunawayDetected());
    return 0;
}
