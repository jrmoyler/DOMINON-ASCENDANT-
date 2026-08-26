#include "Founder/DAFounderCharacter.h"

#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/DAAbilitySystemComponent.h"
#include "Combat/DACombatAttributeSet.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "Founder/DAFounderGameplayAbilities.h"

ADAFounderCharacter::ADAFounderCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 350.f;
    CameraBoom->SetRelativeRotation(FRotator(-15.f, 0.f, 0.f));
    CameraBoom->bUsePawnControlRotation = true;

    FounderCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FounderCamera"));
    FounderCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FounderCamera->bUsePawnControlRotation = false;
    FounderCamera->SetFieldOfView(80.f);

    AbilitySystemComponent = CreateDefaultSubobject<UDAAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    CombatAttributes = CreateDefaultSubobject<UDACombatAttributeSet>(TEXT("CombatAttributes"));
    Tags.AddUnique(TEXT("civilization.synara"));

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

    MoveAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_FounderMove.IA_FounderMove")));
    LookAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_FounderLook.IA_FounderLook")));
    SprintAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_FounderSprint.IA_FounderSprint")));
    DodgeAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_FounderDodge.IA_FounderDodge")));
    InteractAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_FounderInteract.IA_FounderInteract")));
    TraversalAction = TSoftObjectPtr<UInputAction>(FSoftObjectPath(TEXT("/Game/Input/Actions/IA_FounderTraversal.IA_FounderTraversal")));
}

void ADAFounderCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeAbilityActorInfo();
}

void ADAFounderCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitializeAbilityActorInfo();
}

void ADAFounderCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitializeAbilityActorInfo();
}

UAbilitySystemComponent* ADAFounderCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ADAFounderCharacter::InitializeAbilityActorInfo()
{
    if (AbilitySystemComponent == nullptr) return;
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    if (!bCombatAttributesRegistered && CombatAttributes != nullptr)
    {
        AbilitySystemComponent->AddAttributeSetSubobject(CombatAttributes);
        bCombatAttributesRegistered = true;
    }
    if (CombatAttributes != nullptr && HasAuthority()
        && !bFounderAttributesInitialized)
    {
        AbilitySystemComponent->SetNumericAttributeBase(
            UDACombatAttributeSet::GetTacticalChargeAttribute(), 100.f);
        bFounderAttributesInitialized = true;
    }
    GrantFounderAbilities();
}

void ADAFounderCharacter::GrantFounderAbilities()
{
    if (bFounderAbilitiesGranted || AbilitySystemComponent == nullptr || !HasAuthority()) return;
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UDAFounderPrecisionScanAbility::StaticClass(), 1));
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UDAFounderDroneBarrierAbility::StaticClass(), 1));
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UDAFounderOrchestrationMarkAbility::StaticClass(), 1));
    AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(UDAFounderCoordinatedOverrideAbility::StaticClass(), 1));
    bFounderAbilitiesGranted = true;
}

void ADAFounderCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput)
    {
        return;
    }

    if (const UInputAction* Action = MoveAction.LoadSynchronous())
    {
        EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &ADAFounderCharacter::HandleMoveInput);
    }
    if (const UInputAction* Action = LookAction.LoadSynchronous())
    {
        EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &ADAFounderCharacter::HandleLookInput);
    }
    if (const UInputAction* Action = SprintAction.LoadSynchronous())
    {
        EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &ADAFounderCharacter::HandleSprintStarted);
        EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, &ADAFounderCharacter::HandleSprintCompleted);
        EnhancedInput->BindAction(Action, ETriggerEvent::Canceled, this, &ADAFounderCharacter::HandleSprintCompleted);
    }
    if (const UInputAction* Action = DodgeAction.LoadSynchronous())
    {
        EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &ADAFounderCharacter::HandleDodgeInput);
    }
    if (const UInputAction* Action = InteractAction.LoadSynchronous())
    {
        EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &ADAFounderCharacter::HandleInteractInput);
    }
    if (const UInputAction* Action = TraversalAction.LoadSynchronous())
    {
        EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &ADAFounderCharacter::HandleTraversalInput);
    }
}

void ADAFounderCharacter::HandleMoveInput(const FInputActionValue& Value)
{
    const FVector2D Movement = Value.Get<FVector2D>();
    const FRotator ViewRotation = Controller ? Controller->GetControlRotation() : GetActorRotation();
    const FRotator YawRotation(0.f, ViewRotation.Yaw, 0.f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Movement.Y, true);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Movement.X, true);
}

void ADAFounderCharacter::HandleLookInput(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>();
    AddControllerYawInput(Look.X);
    AddControllerPitchInput(Look.Y);
}

void ADAFounderCharacter::HandleSprintStarted()
{
    bSprinting = true;
    GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
}

void ADAFounderCharacter::HandleSprintCompleted()
{
    bSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ADAFounderCharacter::HandleDodgeInput()
{
    OnDodgeRequested.Broadcast();
    FVector DodgeDirection = GetLastMovementInputVector().GetSafeNormal();
    if (DodgeDirection.IsNearlyZero())
    {
        DodgeDirection = GetActorForwardVector();
    }
    LaunchCharacter((DodgeDirection * DodgeHorizontalImpulse) + (FVector::UpVector * DodgeVerticalImpulse), true, true);
}

void ADAFounderCharacter::HandleInteractInput()
{
    OnInteractRequested.Broadcast();
}

void ADAFounderCharacter::HandleTraversalInput()
{
    OnTraversalRequested.Broadcast();
}

bool ADAFounderCharacter::IsSprinting() const
{
    return bSprinting;
}

USpringArmComponent* ADAFounderCharacter::GetCameraBoom() const
{
    return CameraBoom;
}

UCameraComponent* ADAFounderCharacter::GetFounderCamera() const
{
    return FounderCamera;
}
