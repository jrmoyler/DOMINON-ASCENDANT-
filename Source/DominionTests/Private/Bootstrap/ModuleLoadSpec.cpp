#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"

BEGIN_DEFINE_SPEC(FDAModuleLoadSpec, "Dominion.Bootstrap.Modules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
END_DEFINE_SPEC(FDAModuleLoadSpec)

void FDAModuleLoadSpec::Define()
{
    It("loads every runtime module", [this]()
    {
        FModuleManager& Modules = FModuleManager::Get();
        TestTrue("DominionCore", Modules.IsModuleLoaded("DominionCore"));
        TestTrue("DominionSimulation", Modules.IsModuleLoaded("DominionSimulation"));
        TestTrue("DominionGameplay", Modules.IsModuleLoaded("DominionGameplay"));
        TestTrue("DominionWorld", Modules.IsModuleLoaded("DominionWorld"));
        TestTrue("DominionUI", Modules.IsModuleLoaded("DominionUI"));
    });
}
