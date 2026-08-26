#pragma once

namespace DA::Presentation::Validation
{
    struct FEmitterLifecycle
    {
        bool bHasLifecycleModule = false;
        bool bHasLifeCycleMode = false;
        bool bIsSelfManaged = false;
        bool bHasLoopBehavior = false;
        bool bLoopsOnce = false;

        bool IsExactOneShot() const
        {
            return bHasLifecycleModule && bHasLifeCycleMode && bIsSelfManaged
                && bHasLoopBehavior && bLoopsOnce;
        }
    };

    /**
     * Maps persisted module/input records belonging to exactly one emitter.
     * The reader is keyed by emitter identity, so system-level or sibling-
     * emitter lifecycle state can never contribute to the returned result.
     */
    template <typename TEmitterIdentity, typename TReadModules,
        typename TModuleName, typename TInputName, typename TEnumValue>
    FEmitterLifecycle ReadEmitterLifecycle(
        const TEmitterIdentity& EmitterIdentity,
        TReadModules&& ReadModules,
        const TModuleName& ExpectedLifecycleModule,
        const TInputName& ExpectedLifeCycleModeInput,
        const TInputName& ExpectedLoopBehaviorInput,
        const TEnumValue& ExpectedSelfValue,
        const TEnumValue& ExpectedOnceValue)
    {
        FEmitterLifecycle Result;
        const auto& Modules = ReadModules(EmitterIdentity);
        for (const auto& Module : Modules)
        {
            if (!Module.IsEnabled()
                || !Module.HasIdentity(ExpectedLifecycleModule))
            {
                continue;
            }

            // Multiple enabled lifecycle modules are ambiguous and must fail
            // closed instead of allowing their inputs to be combined.
            if (Result.bHasLifecycleModule) return FEmitterLifecycle{};
            Result.bHasLifecycleModule = true;

            for (const auto& Input : Module.GetInputs())
            {
                if (Input.HasIdentity(ExpectedLifeCycleModeInput))
                {
                    if (Result.bHasLifeCycleMode) return FEmitterLifecycle{};
                    Result.bHasLifeCycleMode = true;
                    Result.bIsSelfManaged = Input.HasEnumValue(ExpectedSelfValue);
                }
                else if (Input.HasIdentity(ExpectedLoopBehaviorInput))
                {
                    if (Result.bHasLoopBehavior) return FEmitterLifecycle{};
                    Result.bHasLoopBehavior = true;
                    Result.bLoopsOnce = Input.HasEnumValue(ExpectedOnceValue);
                }
            }
        }
        return Result;
    }

    /**
     * Accepts only when one emitter satisfies the complete authored contract.
     * Keeping every predicate in this loop prevents identity, lifecycle, target,
     * and renderer evidence from being combined across different emitters.
     */
    template <typename TEmitterRange, typename TEmitterName,
        typename TSimulationTarget, typename TRendererClass>
    bool HasExpectedEmitterContent(const TEmitterRange& Emitters,
        const TEmitterName& ExpectedEmitterName,
        const TSimulationTarget& ExpectedSimulationTarget,
        const TRendererClass& ExpectedRendererClass,
        const bool bRequiresOneShot)
    {
        for (const auto& Emitter : Emitters)
        {
            if (Emitter.IsValid()
                && Emitter.IsEnabled()
                && Emitter.HasIdentity(ExpectedEmitterName)
                && Emitter.HasSimulationTarget(ExpectedSimulationTarget)
                && (!bRequiresOneShot || Emitter.IsOneShot())
                && Emitter.HasEnabledRenderer(ExpectedRendererClass))
            {
                return true;
            }
        }
        return false;
    }
}
