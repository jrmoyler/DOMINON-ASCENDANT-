#include "Commands/DAUIAuthoritativeService.h"

bool UDAUIAuthoritativeFeatureRegistrySubsystem::RegisterAuthoritativeService(
    UObject* Service, FString& OutError)
{
    if (Service == nullptr || Cast<IDAUIAuthoritativeFeatureService>(Service) == nullptr)
    {
        OutError = TEXT("ServiceUnavailable: registered feature authority must implement "
            "IDAUIAuthoritativeFeatureService.");
        return false;
    }
    RegisteredService = Service;
    OutError.Reset();
    return true;
}

void UDAUIAuthoritativeFeatureRegistrySubsystem::UnregisterAuthoritativeService(UObject* Service)
{
    if (RegisteredService.Get() == Service) RegisteredService.Reset();
}

IDAUIAuthoritativeFeatureService* UDAUIAuthoritativeFeatureRegistrySubsystem::Resolve(
    FString& OutError) const
{
    IDAUIAuthoritativeFeatureService* Service = Cast<IDAUIAuthoritativeFeatureService>(RegisteredService.Get());
    if (Service == nullptr)
        OutError = TEXT("ServiceUnavailable: the authoritative Task 23-25 feature service is not registered.");
    return Service;
}

bool UDAUIAuthoritativeFeatureRegistrySubsystem::ExecuteRegisteredCommand(
    const FName CommandId, const FName SourceScreenId, const FString& PayloadJson, FString& OutError) const
{
    IDAUIAuthoritativeFeatureService* Service = Resolve(OutError);
    return Service != nullptr
        && Service->ExecuteAuthoritativeUICommand(CommandId, SourceScreenId, PayloadJson, OutError);
}

bool UDAUIAuthoritativeFeatureRegistrySubsystem::CaptureRegisteredState(
    FDAUIAuthoritativeFeatureSnapshot& OutState, FString& OutError) const
{
    OutState = {};
    IDAUIAuthoritativeFeatureService* Service = Resolve(OutError);
    return Service != nullptr && Service->CaptureAuthoritativeUIState(OutState, OutError);
}
