#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"

#include "DAFounderCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UDAAbilitySystemComponent;
class UDACombatAttributeSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDAFounderInputRequest);

UCLASS(BlueprintType)
class DOMINIONGAMEPLAY_API ADAFounderCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ADAFounderCharacter();

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    void HandleMoveInput(const FInputActionValue& Value);

    void HandleLookInput(const FInputActionValue& Value);

    UFUNCTION(BlueprintCallable, Category = "Founder|Input")
    void HandleSprintStarted();

    UFUNCTION(BlueprintCallable, Category = "Founder|Input")
    void HandleSprintCompleted();

    UFUNCTION(BlueprintCallable, Category = "Founder|Input")
    void HandleDodgeInput();

    UFUNCTION(BlueprintCallable, Category = "Founder|Input")
    void HandleInteractInput();

    UFUNCTION(BlueprintCallable, Category = "Founder|Input")
    void HandleTraversalInput();

    UFUNCTION(BlueprintPure, Category = "Founder|Movement")
    bool IsSprinting() const;

    USpringArmComponent* GetCameraBoom() const;
    UCameraComponent* GetFounderCamera() const;

    UPROPERTY(BlueprintAssignable, Category = "Founder|Input")
    FDAFounderInputRequest OnDodgeRequested;

    UPROPERTY(BlueprintAssignable, Category = "Founder|Input")
    FDAFounderInputRequest OnInteractRequested;

    UPROPERTY(BlueprintAssignable, Category = "Founder|Input")
    FDAFounderInputRequest OnTraversalRequested;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Founder|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Founder|Camera")
    TObjectPtr<UCameraComponent> FounderCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Founder|Abilities")
    TObjectPtr<UDAAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Founder|Abilities")
    TObjectPtr<UDACombatAttributeSet> CombatAttributes;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Input")
    TSoftObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Input")
    TSoftObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Input")
    TSoftObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Input")
    TSoftObjectPtr<UInputAction> DodgeAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Input")
    TSoftObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Input")
    TSoftObjectPtr<UInputAction> TraversalAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Movement", meta = (ClampMin = "0.0"))
    float WalkSpeed = 600.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Movement", meta = (ClampMin = "0.0"))
    float SprintSpeed = 900.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Movement", meta = (ClampMin = "0.0"))
    float DodgeHorizontalImpulse = 900.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Founder|Movement", meta = (ClampMin = "0.0"))
    float DodgeVerticalImpulse = 120.f;

private:
    void InitializeAbilityActorInfo();
    void GrantFounderAbilities();

    bool bSprinting = false;
    bool bCombatAttributesRegistered = false;
    bool bFounderAttributesInitialized = false;
    bool bFounderAbilitiesGranted = false;
};
