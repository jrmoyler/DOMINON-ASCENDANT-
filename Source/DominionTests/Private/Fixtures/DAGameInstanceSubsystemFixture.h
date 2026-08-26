#pragma once

#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"

class FDAGameInstanceSubsystemFixture final
{
public:
    FDAGameInstanceSubsystemFixture()
    {
        GameInstance = NewObject<UGameInstance>(GetTransientPackage());
        GameInstance->AddToRoot();
        GameInstance->Init();
    }

    ~FDAGameInstanceSubsystemFixture()
    {
        if (GameInstance != nullptr)
        {
            GameInstance->Shutdown();
            GameInstance->RemoveFromRoot();
        }
    }

    FDAGameInstanceSubsystemFixture(const FDAGameInstanceSubsystemFixture&) = delete;
    FDAGameInstanceSubsystemFixture& operator=(const FDAGameInstanceSubsystemFixture&) = delete;

    template <typename TSubsystem>
    TSubsystem* GetSubsystem() const
    {
        return GameInstance != nullptr ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
    }
    UGameInstance* GetGameInstance() const { return GameInstance; }

private:
    UGameInstance* GameInstance = nullptr;
};
