// player controller - aim at pads to place, escape for pause menu
// line trace from camera each tick, left click asks game mode to spawn a cannon
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/PortalProtectTypes.h"
#include "PortalProtectPlayerController.generated.h"

class ADefenderPlacementSpot;
class UUserWidget;

UCLASS()
class FORT_FUMBLE_API APortalProtectPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APortalProtectPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Placement")
	float PlacementTraceDistance = 4000.f;

	// pause widget - defaults to WBP_PauseMenu in content
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Pause")
	TSubclassOf<UUserWidget> PauseMenuWidgetClass;

	// game over widget - defaults to WBP_GameOverMenu
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|GameOver")
	TSubclassOf<UUserWidget> GameOverMenuWidgetClass;

	// victory widget - defaults to WBP_VictoryScreen
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Victory")
	TSubclassOf<UUserWidget> VictoryMenuWidgetClass;

	// UMG TextBlock name on end-of-match menus for final score (Is Variable / rename in designer)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Score")
	FName ScoreTextWidgetName = TEXT("TextScore");

	// escape or P - safe to call from blueprint
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void TogglePauseMenu();

	// close pause and go back to playing - hook resume button to this
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void ResumeGame();

	// same as ResumeGame - WBP_PauseMenu uses UnpauseGame name sometimes
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void UnpauseGame();

	// back to main menu map
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void QuitToMainMenu();

	// reload this level - hook restart on game over / victory screens
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void RestartGame();

	// game mode calls this when the tower dies
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void ShowGameOverMenu();

	// game mode calls this when all waves are cleared
	UFUNCTION(BlueprintCallable, Category = "Victory")
	void ShowVictoryMenu();

	UFUNCTION(BlueprintPure, Category = "Pause")
	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "GameOver")
	bool IsGameOverMenuOpen() const { return bGameOverMenuOpen; }

	UFUNCTION(BlueprintPure, Category = "Victory")
	bool IsVictoryMenuOpen() const { return bVictoryMenuOpen; }

protected:
	void OnLeftClick();
	void OnUpgradeInteract();
	void OnRestart();
	bool IsGameplayInputBlocked() const;
	bool IsEndMatchMenuOpen() const { return bGameOverMenuOpen || bVictoryMenuOpen; }
	void ApplyDefenderTypeSelection(EDefenderType Type);
	void SelectDefenderCannon();
	void SelectDefenderMarksman();
	void SelectDefenderMortar();
	void CycleDefenderPrev();
	void CycleDefenderNext();
	ADefenderPlacementSpot* TracePlacementSpot() const;

	void ShowPauseMenu();
	void HidePauseMenu();
	UClass* ResolvePauseMenuClass();
	UClass* ResolveGameOverMenuClass();
	UClass* ResolveVictoryMenuClass();
	void HideGameOverMenu();
	void HideVictoryMenu();
	// push GM->GetScore() into an end-menu TextBlock (ScoreTextWidgetName + common aliases)
	void UpdateEndMenuScoreText(UUserWidget* MenuWidget);
	void UpdateGameOverScoreText();
	void UpdateVictoryScoreText();
	UClass* TryLoadWidgetClass(const TCHAR* ClassObjectPath, const TCHAR* DebugName) const;
	// bind retry/main-menu (and legacy restart/quit) buttons if present
	void BindEndMenuButtons(UUserWidget* MenuWidget);

	UPROPERTY()
	TObjectPtr<ADefenderPlacementSpot> HoveredSpot;

	UPROPERTY()
	TObjectPtr<UUserWidget> PauseMenuWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> GameOverMenuWidget;

	UPROPERTY()
	TObjectPtr<UUserWidget> VictoryMenuWidget;

	UPROPERTY()
	TSoftClassPtr<UUserWidget> PauseMenuClassSoft;

	UPROPERTY()
	TSoftClassPtr<UUserWidget> GameOverMenuClassSoft;

	UPROPERTY()
	TSoftClassPtr<UUserWidget> VictoryMenuClassSoft;

	bool bPauseMenuOpen = false;
	bool bGameOverMenuOpen = false;
	bool bVictoryMenuOpen = false;
};
