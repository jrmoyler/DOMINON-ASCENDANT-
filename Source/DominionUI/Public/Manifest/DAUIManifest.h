#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

#include "DAUIManifest.generated.h"

UENUM(BlueprintType)
enum class EDAUIScreenInputMode : uint8
{
    GameOnly,
    UIOnly,
    GameAndUI
};

UENUM(BlueprintType)
enum class EDAUIScreenLayer : uint8
{
    Menu,
    HUD,
    Panel,
    Modal
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIInputContextDescriptor
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Priority = 0;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIScreenDescriptor
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftClassPath WidgetClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAUIScreenLayer Layer = EDAUIScreenLayer::Panel;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName InputContextId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDAUIScreenInputMode InputMode = EDAUIScreenInputMode::UIOnly;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName EntryFocusTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName BackTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> DataChannels;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> CampaignCriticalActions;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIOverlayDescriptor
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftClassPath WidgetClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MarkerShape;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUICampaignActionDescriptor
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName CommandId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> Surfaces;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString KeyboardInput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ControllerInput;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRequiresHover = false;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIAccessibilityOptionDescriptor
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Type;
    /** Canonical JSON preserves the heterogeneous frozen default without lossy coercion. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString DefaultValueJson;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString MinimumJson;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString MaximumJson;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FName> Values;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIVisualDirection
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Tone;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bLowChromeHUD = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bKeepCenterAndLowerMiddleClear = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName MotionSettingId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName FlashSettingId;
};

USTRUCT(BlueprintType)
struct DOMINIONUI_API FDAUIManifest
{
    GENERATED_BODY()

    static FString GetCanonicalPath();
    static bool LoadCanonical(FDAUIManifest& OutManifest, TArray<FText>& OutErrors);
    static bool LoadFile(const FString& Path, FDAUIManifest& OutManifest, TArray<FText>& OutErrors);
    bool Validate(TArray<FText>& OutErrors) const;

    const FDAUIScreenDescriptor* FindScreen(FName Id) const;
    const FDAUIInputContextDescriptor* FindInputContext(FName Id) const;
    const FDAUICampaignActionDescriptor* FindAction(FName Id) const;
    const FDAUIOverlayDescriptor* FindOverlay(FName Id) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SchemaVersion = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Fingerprint;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDAUIVisualDirection VisualDirection;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIInputContextDescriptor> InputContexts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIScreenDescriptor> Screens;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIOverlayDescriptor> Overlays;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUICampaignActionDescriptor> CampaignCriticalActions;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDAUIAccessibilityOptionDescriptor> AccessibilityOptions;
};
