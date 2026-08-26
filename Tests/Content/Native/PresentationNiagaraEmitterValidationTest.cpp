#include "DAPresentationNiagaraEmitterValidation.h"

#include <string_view>
#include <vector>

namespace
{
    struct FFakeLifecycleInput
    {
        std::string_view Name;
        std::string_view EnumValue;

        bool HasIdentity(const std::string_view Expected) const
        {
            return Name == Expected;
        }
        bool HasEnumValue(const std::string_view Expected) const
        {
            return EnumValue == Expected;
        }
    };

    struct FFakeLifecycleModule
    {
        std::string_view ScriptName;
        bool bEnabled = true;
        std::vector<FFakeLifecycleInput> Inputs;

        bool IsEnabled() const { return bEnabled; }
        bool HasIdentity(const std::string_view Expected) const
        {
            return ScriptName == Expected;
        }
        const std::vector<FFakeLifecycleInput>& GetInputs() const { return Inputs; }
    };

    struct FFakeLifecyclePersistence
    {
        std::vector<FFakeLifecycleModule> SystemModules;
        std::vector<FFakeLifecycleModule> SelectedEmitterModules;
        std::vector<FFakeLifecycleModule> OtherEmitterModules;

        const std::vector<FFakeLifecycleModule>& Read(
            const std::string_view Owner) const
        {
            if (Owner == "selected-emitter") return SelectedEmitterModules;
            if (Owner == "other-emitter") return OtherEmitterModules;
            return SystemModules;
        }
    };

    struct FFakeEmitter
    {
        std::string_view Name;
        bool bValid = true;
        bool bEnabled = true;
        bool bCpuTarget = true;
        DA::Presentation::Validation::FEmitterLifecycle Lifecycle;
        std::vector<std::string_view> EnabledRendererClasses;

        bool IsValid() const { return bValid; }
        bool IsEnabled() const { return bEnabled; }
        bool HasIdentity(const std::string_view Expected) const
        {
            return Name == Expected;
        }
        bool HasSimulationTarget(const std::string_view Expected) const
        {
            return bCpuTarget && Expected == "CPUSim";
        }
        bool IsOneShot() const { return Lifecycle.IsExactOneShot(); }
        bool HasEnabledRenderer(const std::string_view Expected) const
        {
            for (const std::string_view Renderer : EnabledRendererClasses)
                if (Renderer == Expected) return true;
            return false;
        }
    };

    bool LegacyCrossEmitterAggregateAccepts(const std::vector<FFakeEmitter>& Emitters)
    {
        bool bExpectedIdentity = false;
        bool bExpectedRenderer = false;
        for (const FFakeEmitter& Emitter : Emitters)
        {
            if (!Emitter.IsValid() || !Emitter.IsEnabled()) continue;
            bExpectedIdentity |= Emitter.HasIdentity("SimpleSpriteBurst");
            bExpectedRenderer |= Emitter.HasEnabledRenderer(
                "NiagaraSpriteRendererProperties");
        }
        return bExpectedIdentity && bExpectedRenderer;
    }
}

int main()
{
    const FFakeLifecyclePersistence Persistence = {
        {{{"EmitterState", true,
            {{"LifeCycleMode", "Self"}, {"LoopBehavior", "Once"}}}}},
        {{{"EmitterState", true,
            {{"LifeCycleMode", "Self"}, {"LoopBehavior", "Infinite"}}}}},
        {{{"EmitterState", true,
            {{"LifeCycleMode", "Self"}, {"LoopBehavior", "Once"}}}}},
    };
    const auto ReadPersistedModules = [&Persistence](const std::string_view Owner)
        -> const std::vector<FFakeLifecycleModule>&
    {
        return Persistence.Read(Owner);
    };
    const auto SelectedLifecycle =
        DA::Presentation::Validation::ReadEmitterLifecycle(
            std::string_view("selected-emitter"), ReadPersistedModules,
            std::string_view("EmitterState"), std::string_view("LifeCycleMode"),
            std::string_view("LoopBehavior"), std::string_view("Self"),
            std::string_view("Once"));
    const auto OtherLifecycle = DA::Presentation::Validation::ReadEmitterLifecycle(
        std::string_view("other-emitter"), ReadPersistedModules,
        std::string_view("EmitterState"), std::string_view("LifeCycleMode"),
        std::string_view("LoopBehavior"), std::string_view("Self"),
        std::string_view("Once"));
    const auto SystemLifecycle = DA::Presentation::Validation::ReadEmitterLifecycle(
        std::string_view("system"), ReadPersistedModules,
        std::string_view("EmitterState"), std::string_view("LifeCycleMode"),
        std::string_view("LoopBehavior"), std::string_view("Self"),
        std::string_view("Once"));
    if (SelectedLifecycle.IsExactOneShot()) return 1;
    if (!OtherLifecycle.IsExactOneShot()) return 2;
    if (!SystemLifecycle.IsExactOneShot()) return 3;

    // This is the old adapter failure mode: injecting system lifecycle into
    // the selected-emitter row makes a genuinely looping emitter appear valid.
    const std::vector<FFakeEmitter> WronglySystemScopedLifecycle = {
        {"SimpleSpriteBurst", true, true, true, SystemLifecycle,
            {"NiagaraSpriteRendererProperties"}},
    };
    if (!DA::Presentation::Validation::HasExpectedEmitterContent(
            WronglySystemScopedLifecycle, std::string_view("SimpleSpriteBurst"),
            std::string_view("CPUSim"),
            std::string_view("NiagaraSpriteRendererProperties"), true)) return 4;

    const std::vector<FFakeEmitter> CrossEmitterBypass = {
        {"SimpleSpriteBurst", true, true, true, SelectedLifecycle, {}},
        {"DecoyRendererEmitter", true, true, true, OtherLifecycle,
            {"NiagaraSpriteRendererProperties"}},
    };
    if (!LegacyCrossEmitterAggregateAccepts(CrossEmitterBypass)) return 5;
    if (DA::Presentation::Validation::HasExpectedEmitterContent(
            CrossEmitterBypass, std::string_view("SimpleSpriteBurst"),
            std::string_view("CPUSim"),
            std::string_view("NiagaraSpriteRendererProperties"), true)) return 6;

    const FFakeLifecyclePersistence ExactPersistence = {
        Persistence.SystemModules,
        Persistence.OtherEmitterModules,
        Persistence.OtherEmitterModules,
    };
    const auto ReadExactModules = [&ExactPersistence](const std::string_view Owner)
        -> const std::vector<FFakeLifecycleModule>&
    {
        return ExactPersistence.Read(Owner);
    };
    const auto ExactSelectedLifecycle =
        DA::Presentation::Validation::ReadEmitterLifecycle(
            std::string_view("selected-emitter"), ReadExactModules,
            std::string_view("EmitterState"), std::string_view("LifeCycleMode"),
            std::string_view("LoopBehavior"), std::string_view("Self"),
            std::string_view("Once"));

    const std::vector<FFakeEmitter> ExactSourceEmitter = {
        {"SimpleSpriteBurst", true, true, true, ExactSelectedLifecycle,
            {"NiagaraSpriteRendererProperties"}},
        {"DecoyRendererEmitter", true, true, false, SelectedLifecycle,
            {"NiagaraMeshRendererProperties"}},
    };
    if (!DA::Presentation::Validation::HasExpectedEmitterContent(
            ExactSourceEmitter, std::string_view("SimpleSpriteBurst"),
            std::string_view("CPUSim"),
            std::string_view("NiagaraSpriteRendererProperties"), true)) return 7;

    for (const FFakeEmitter& InvalidExpectedEmitter : {
        FFakeEmitter{"SimpleSpriteBurst", true, false, true,
            ExactSourceEmitter[0].Lifecycle,
            {"NiagaraSpriteRendererProperties"}},
        FFakeEmitter{"SimpleSpriteBurst", true, true, false,
            ExactSourceEmitter[0].Lifecycle,
            {"NiagaraSpriteRendererProperties"}},
        FFakeEmitter{"SimpleSpriteBurst", true, true, true, SelectedLifecycle,
            {"NiagaraSpriteRendererProperties"}},
        FFakeEmitter{"SimpleSpriteBurst", true, true, true,
            ExactSourceEmitter[0].Lifecycle,
            {"NiagaraMeshRendererProperties"}},
    })
    {
        if (DA::Presentation::Validation::HasExpectedEmitterContent(
                std::vector<FFakeEmitter>{InvalidExpectedEmitter},
                std::string_view("SimpleSpriteBurst"),
                std::string_view("CPUSim"),
                std::string_view("NiagaraSpriteRendererProperties"), true)) return 8;
    }
    return 0;
}
