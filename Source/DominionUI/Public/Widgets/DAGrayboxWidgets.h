#pragma once

#include "Accessibility/DAAccessibilitySettings.h"
#include "CommonActivatableWidget.h"
#include "CommonButtonBase.h"
#include "Manifest/DAUIManifest.h"
#include "ViewModels/DAUIViewModels.h"

#include "DAGrayboxWidgets.generated.h"

class UDAUIScreenRouter;
class UDAActionButton;
class UDAAccessibilityOptionButton;
class UVerticalBox;
class UWidget;

/** Source-backed CommonUI base used by generated graybox Widget Blueprints. */
UCLASS(Abstract, BlueprintType)
class DOMINIONUI_API UDAActivatableScreen : public UCommonActivatableWidget, public IDAAccessibilityConsumer
{
    GENERATED_BODY()
public:
    UDAActivatableScreen(const FObjectInitializer& ObjectInitializer);
    void BindRouter(UDAUIScreenRouter* InRouter, FName InScreenId);
    void ConfigureScreen(const FDAUIScreenDescriptor& Descriptor, UDAUIScreenRouter* InRouter);

    UFUNCTION(BlueprintCallable, Category="Dominion|UI")
    bool RequestCampaignAction(FName ActionId, const FString& PayloadJson);

    UFUNCTION(BlueprintPure, Category="Dominion|UI")
    FName GetStableScreenId() const { return StableScreenId; }

    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Accessibility")
    void ApplyAccessibilitySettings(const FDAAccessibilitySettings& Settings);
    virtual void ApplyAccessibilityPolicy(const FDAAccessibilityRuntimePolicy& Policy) override;

    UFUNCTION(BlueprintPure, Category="Dominion|UI")
    TArray<FName> GetRegisteredActionIds() const;
    UFUNCTION(BlueprintPure, Category="Dominion|UI")
    int32 GetDataSlotCount() const;
    void SetProjection(const FDAUIScreenProjection& InProjection);
    UFUNCTION(BlueprintPure, Category="Dominion|UI") const TArray<FString>& GetRenderedDataValues() const
        { return Projection.RenderedValues; }
    FString BuildPayloadForAction(FName ActionId, int32 PayloadIndex = 0,
        bool bController = false, float AxisValue = 0.f, bool bHasAxis = false) const;
    bool CycleAccessibilityOption(FName OptionId);
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Accessibility")
    bool SetAccessibilityBinding(FName ActionId, FKey Key, bool bController, FString& OutError);
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Accessibility")
    bool SetHoldToggleMode(FName ActionId, EDAHoldToggleMode Mode, FString& OutError);
    UFUNCTION(BlueprintPure, Category="Dominion|UI|Accessibility")
    int32 GetAccessibilityOptionControlCount() const { return AccessibilityOptionButtons.Num(); }

    UDAActionButton* FindActionButton(FName ActionId) const;
    UFUNCTION(BlueprintPure, Category="Dominion|UI") UWidget* GetResolvedFocusWidget() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual bool NativeOnHandleBackAction() override;
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI") FName StableScreenId;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dominion|UI|Graybox") FLinearColor PanelColor = FLinearColor(0.075f, 0.09f, 0.09f, 0.96f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dominion|UI|Graybox") FLinearColor CivicAccent = FLinearColor(0.25f, 0.72f, 0.65f, 1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dominion|UI|Graybox") bool bKeepPlayfieldCenterClear = true;
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|Accessibility") FDAAccessibilitySettings AppliedAccessibility;
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|Accessibility") FDAUIEffectsPolicy EffectsPolicy;
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|Accessibility") FDAAccessibilityRuntimePolicy RuntimePolicy;
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|Data") FDAUIScreenProjection Projection;

private:
    void RebuildManifestSlots();
    UPROPERTY(Transient) TObjectPtr<UDAUIScreenRouter> Router;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> ActionList;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> DataList;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UDAActionButton>> ActionButtons;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UWidget>> FocusWidgets;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<UDAAccessibilityOptionButton>> AccessibilityOptionButtons;
    FDAUIScreenDescriptor ScreenDescriptor;
};

/** One focusable control per frozen accessibility option; remap/map options use the typed setters above. */
UCLASS(BlueprintType)
class DOMINIONUI_API UDAAccessibilityOptionButton final : public UCommonButtonBase
{
    GENERATED_BODY()
public:
    void Configure(UDAActivatableScreen* InOwner, FName InOptionId)
    { OwnerScreen = InOwner; OptionId = InOptionId; SetIsFocusable(true); }
    UFUNCTION(BlueprintPure, Category="Dominion|UI|Accessibility") FName GetOptionId() const { return OptionId; }
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|Accessibility") bool Trigger();
protected:
    virtual void NativeOnClicked() override;
private:
    UPROPERTY(Transient) TObjectPtr<UDAActivatableScreen> OwnerScreen;
    UPROPERTY(Transient) FName OptionId;
};

/** Concrete controller-focusable CommonUI button for one manifest command route. */
UCLASS(BlueprintType)
class DOMINIONUI_API UDAActionButton final : public UCommonButtonBase
{
    GENERATED_BODY()
public:
    void Configure(UDAActivatableScreen* InOwner, FName InActionId, FName InFocusId);
    UFUNCTION(BlueprintCallable, Category="Dominion|UI") bool Trigger();
    UFUNCTION(BlueprintPure, Category="Dominion|UI") FName GetActionId() const { return ActionId; }
    UFUNCTION(BlueprintPure, Category="Dominion|UI") FName GetFocusId() const { return FocusId; }
    UFUNCTION(BlueprintPure, Category="Dominion|UI") FText GetLabelText() const { return LabelText; }
    UFUNCTION(BlueprintPure, Category="Dominion|UI") FString GetPayloadJson() const;
protected:
    virtual void NativeOnClicked() override;
private:
    UPROPERTY(Transient) TObjectPtr<UDAActivatableScreen> OwnerScreen;
    UPROPERTY(Transient) FName ActionId;
    UPROPERTY(Transient) FName FocusId;
    UPROPERTY(Transient) FText LabelText;
};

UCLASS(BlueprintType)
class DOMINIONUI_API UDAGrayboxScreenWidget : public UDAActivatableScreen
{
    GENERATED_BODY()
};

UCLASS(BlueprintType)
class DOMINIONUI_API UDACityHUDWidget : public UDAActivatableScreen
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|HUD")
    void SetViewModel(const FDACityHUDViewModel& InViewModel) { ViewModel = InViewModel; }
protected:
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|HUD") FDACityHUDViewModel ViewModel;
};

UCLASS(BlueprintType)
class DOMINIONUI_API UDAFounderHUDWidget : public UDAActivatableScreen
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|HUD")
    void SetViewModel(const FDAFounderHUDViewModel& InViewModel) { ViewModel = InViewModel; }
protected:
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|HUD") FDAFounderHUDViewModel ViewModel;
};

UCLASS(BlueprintType)
class DOMINIONUI_API UDACommandHUDWidget : public UDAActivatableScreen
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Dominion|UI|HUD")
    void SetViewModel(const FDACommandHUDViewModel& InViewModel) { ViewModel = InViewModel; }
protected:
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|HUD") FDACommandHUDViewModel ViewModel;
};

UCLASS(BlueprintType)
class DOMINIONUI_API UDAOverlayWidget : public UDAActivatableScreen
{
    GENERATED_BODY()
public:
    void ConfigureOverlay(FName OverlayId, const FDAUIOverlayReadModel& InReadModel, UDAUIScreenRouter* InRouter);
protected:
    UPROPERTY(BlueprintReadOnly, Category="Dominion|UI|Overlay") FDAUIOverlayReadModel OverlayReadModel;
};
