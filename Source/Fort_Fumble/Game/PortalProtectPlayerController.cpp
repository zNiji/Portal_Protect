// FPS placement trace, pause menu, level restart

#include "Game/PortalProtectPlayerController.h"
#include "Game/PortalProtectGameMode.h"
#include "Core/PortalProtectTypes.h"
#include "Defender/DefenderPlacementSpot.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

APortalProtectPlayerController::APortalProtectPlayerController()
{
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	PauseMenuClassSoft = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/UI/Widgets/WBP_PauseMenu.WBP_PauseMenu_C")));
	GameOverMenuClassSoft = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/UI/Widgets/WBP_GameOverMenu.WBP_GameOverMenu_C")));
	VictoryMenuClassSoft = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/UI/Widgets/WBP_VictoryScreen.WBP_VictoryScreen_C")));

	// default: Content/UI/Widgets/WBP_PauseMenu
	static ConstructorHelpers::FClassFinder<UUserWidget> PauseMenuBP(
		TEXT("/Game/UI/Widgets/WBP_PauseMenu"));
	if (PauseMenuBP.Succeeded())
	{
		PauseMenuWidgetClass = PauseMenuBP.Class;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FClassFinder failed for pause menu at /Game/UI/Widgets/WBP_PauseMenu."));
	}

	// default: Content/UI/Widgets/WBP_GameOverMenu
	static ConstructorHelpers::FClassFinder<UUserWidget> GameOverMenuBP(
		TEXT("/Game/UI/Widgets/WBP_GameOverMenu"));
	if (GameOverMenuBP.Succeeded())
	{
		GameOverMenuWidgetClass = GameOverMenuBP.Class;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FClassFinder failed for game-over menu at /Game/UI/Widgets/WBP_GameOverMenu."));
	}

	// default: Content/UI/Widgets/WBP_VictoryScreen
	static ConstructorHelpers::FClassFinder<UUserWidget> VictoryMenuBP(
		TEXT("/Game/UI/Widgets/WBP_VictoryScreen"));
	if (VictoryMenuBP.Succeeded())
	{
		VictoryMenuWidgetClass = VictoryMenuBP.Class;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FClassFinder failed for victory menu at /Game/UI/Widgets/WBP_VictoryScreen."));
	}
}

void APortalProtectPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("PortalProtectPlayerController active"));
	FInputModeGameOnly Mode;
	SetInputMode(Mode);
	bShowMouseCursor = false;
}

void APortalProtectPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APortalProtectPlayerController::OnLeftClick);
		InputComponent->BindKey(EKeys::F, IE_Pressed, this, &APortalProtectPlayerController::OnUpgradeInteract);

		FInputKeyBinding& RestartBinding = InputComponent->BindKey(
			EKeys::R, IE_Pressed, this, &APortalProtectPlayerController::OnRestart);
		RestartBinding.bExecuteWhenPaused = true;

		// esc/p still work while paused so you can unpause
		FInputKeyBinding& EscapeBinding = InputComponent->BindKey(
			EKeys::Escape, IE_Pressed, this, &APortalProtectPlayerController::TogglePauseMenu);
		EscapeBinding.bExecuteWhenPaused = true;

		FInputKeyBinding& PauseBinding = InputComponent->BindKey(
			EKeys::P, IE_Pressed, this, &APortalProtectPlayerController::TogglePauseMenu);
		PauseBinding.bExecuteWhenPaused = true;

		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &APortalProtectPlayerController::SelectDefenderCannon);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &APortalProtectPlayerController::SelectDefenderMarksman);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &APortalProtectPlayerController::SelectDefenderMortar);
		InputComponent->BindKey(EKeys::NumPadOne, IE_Pressed, this, &APortalProtectPlayerController::SelectDefenderCannon);
		InputComponent->BindKey(EKeys::NumPadTwo, IE_Pressed, this, &APortalProtectPlayerController::SelectDefenderMarksman);
		InputComponent->BindKey(EKeys::NumPadThree, IE_Pressed, this, &APortalProtectPlayerController::SelectDefenderMortar);
		InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &APortalProtectPlayerController::CycleDefenderPrev);
		InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APortalProtectPlayerController::CycleDefenderNext);
		InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &APortalProtectPlayerController::CycleDefenderNext);
		InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APortalProtectPlayerController::CycleDefenderPrev);
		UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Defender type keys bound (1/2/3, numpad, Q/E, wheel). F upgrades."));
	}
}

bool APortalProtectPlayerController::IsGameplayInputBlocked() const
{
	return bPauseMenuOpen || IsEndMatchMenuOpen() || UGameplayStatics::IsGamePaused(this);
}

void APortalProtectPlayerController::ApplyDefenderTypeSelection(EDefenderType Type)
{
	if (IsGameplayInputBlocked())
	{
		return;
	}
	if (APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr)
	{
		if (!GM->IsGameOver())
		{
			GM->SetSelectedDefenderType(Type);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] Select defender failed - AuthGameMode is not PortalProtectGameMode."));
	}
}

void APortalProtectPlayerController::SelectDefenderCannon()
{
	ApplyDefenderTypeSelection(EDefenderType::Cannon);
}

void APortalProtectPlayerController::SelectDefenderMarksman()
{
	ApplyDefenderTypeSelection(EDefenderType::Marksman);
}

void APortalProtectPlayerController::SelectDefenderMortar()
{
	ApplyDefenderTypeSelection(EDefenderType::Mortar);
}

void APortalProtectPlayerController::CycleDefenderPrev()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}
	if (APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr)
	{
		if (!GM->IsGameOver())
		{
			GM->CycleSelectedDefenderType(-1);
		}
	}
}

void APortalProtectPlayerController::CycleDefenderNext()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}
	if (APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr)
	{
		if (!GM->IsGameOver())
		{
			GM->CycleSelectedDefenderType(1);
		}
	}
}

void APortalProtectPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (IsGameplayInputBlocked())
	{
		return;
	}

	ADefenderPlacementSpot* Spot = TracePlacementSpot();
	if (HoveredSpot && HoveredSpot != Spot)
	{
		HoveredSpot->SetHighlighted(false);
	}
	HoveredSpot = Spot;
	if (HoveredSpot)
	{
		HoveredSpot->SetHighlighted(true);
	}
}

// camera line trace - only hits pads (visibility channel on cylinder)
ADefenderPlacementSpot* APortalProtectPlayerController::TracePlacementSpot() const
{
	FVector ViewLoc;
	FRotator ViewRot;
	GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector TraceEnd = ViewLoc + ViewRot.Vector() * PlacementTraceDistance;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlacementTrace), true, GetPawn());
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, ViewLoc, TraceEnd, ECC_Visibility, Params))
	{
		return nullptr;
	}
	return Cast<ADefenderPlacementSpot>(Hit.GetActor());
}

void APortalProtectPlayerController::OnUpgradeInteract()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}

	APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr;
	if (!GM || GM->IsGameOver())
	{
		return;
	}

	GM->TryUpgradeNearestTarget();
}

void APortalProtectPlayerController::OnLeftClick()
{
	if (IsGameplayInputBlocked())
	{
		return;
	}

	APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr;
	if (!GM || GM->IsGameOver())
	{
		return;
	}

	if (ADefenderPlacementSpot* Spot = TracePlacementSpot())
	{
		GM->TryPlaceDefenderAtSpot(Spot);
	}
}

void APortalProtectPlayerController::OnRestart()
{
	if (bPauseMenuOpen)
	{
		return;
	}
	RestartGame();
}

UClass* APortalProtectPlayerController::ResolvePauseMenuClass()
{
	if (PauseMenuWidgetClass)
	{
		return PauseMenuWidgetClass.Get();
	}
	if (UClass* Loaded = PauseMenuClassSoft.LoadSynchronous())
	{
		UE_LOG(LogTemp, Warning, TEXT("PauseMenu loaded via default soft class path: %s"), *PauseMenuClassSoft.ToString());
		return Loaded;
	}
	return TryLoadWidgetClass(TEXT("/Game/UI/Widgets/WBP_PauseMenu.WBP_PauseMenu_C"), TEXT("PauseMenu"));
}

UClass* APortalProtectPlayerController::ResolveGameOverMenuClass()
{
	if (GameOverMenuWidgetClass)
	{
		return GameOverMenuWidgetClass.Get();
	}
	if (UClass* Loaded = GameOverMenuClassSoft.LoadSynchronous())
	{
		UE_LOG(LogTemp, Warning, TEXT("GameOverMenu loaded via default soft class path: %s"), *GameOverMenuClassSoft.ToString());
		return Loaded;
	}
	return TryLoadWidgetClass(TEXT("/Game/UI/Widgets/WBP_GameOverMenu.WBP_GameOverMenu_C"), TEXT("GameOverMenu"));
}

UClass* APortalProtectPlayerController::ResolveVictoryMenuClass()
{
	if (VictoryMenuWidgetClass)
	{
		return VictoryMenuWidgetClass.Get();
	}
	if (UClass* Loaded = VictoryMenuClassSoft.LoadSynchronous())
	{
		UE_LOG(LogTemp, Warning, TEXT("VictoryMenu loaded via default soft class path: %s"), *VictoryMenuClassSoft.ToString());
		return Loaded;
	}
	return TryLoadWidgetClass(TEXT("/Game/UI/Widgets/WBP_VictoryScreen.WBP_VictoryScreen_C"), TEXT("VictoryMenu"));
}

UClass* APortalProtectPlayerController::TryLoadWidgetClass(const TCHAR* ClassObjectPath, const TCHAR* DebugName) const
{
	if (UClass* LoadedViaClass = LoadClass<UUserWidget>(nullptr, ClassObjectPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s loaded via LoadClass: %s"), DebugName, ClassObjectPath);
		return LoadedViaClass;
	}

	if (UClass* LoadedViaStatic = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, ClassObjectPath)))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s loaded via StaticLoadObject: %s"), DebugName, ClassObjectPath);
		return LoadedViaStatic;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("%s widget class failed to load. Tried class path: %s. Assign the class on PortalProtectPlayerController defaults."),
		DebugName,
		ClassObjectPath);
	return nullptr;
}

void APortalProtectPlayerController::TogglePauseMenu()
{
	// don't open pause over game-over or victory screens
	if (IsEndMatchMenuOpen())
	{
		return;
	}

	if (bPauseMenuOpen)
	{
		ResumeGame();
	}
	else
	{
		ShowPauseMenu();
	}
}

void APortalProtectPlayerController::ResumeGame()
{
	HidePauseMenu();
}

void APortalProtectPlayerController::UnpauseGame()
{
	ResumeGame();
}

void APortalProtectPlayerController::QuitToMainMenu()
{
	HidePauseMenu();
	HideGameOverMenu();
	HideVictoryMenu();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/Scenes/MainMenu")));
}

void APortalProtectPlayerController::RestartGame()
{
	HidePauseMenu();
	HideGameOverMenu();
	HideVictoryMenu();
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
}

// wire retry / main-menu buttons (also accepts older Restart/Quit names)
void APortalProtectPlayerController::BindEndMenuButtons(UUserWidget* MenuWidget)
{
	if (!MenuWidget)
	{
		return;
	}

	auto BindRestart = [this](UButton* Btn)
	{
		if (Btn)
		{
			Btn->OnClicked.AddDynamic(this, &APortalProtectPlayerController::RestartGame);
		}
	};
	auto BindQuit = [this](UButton* Btn)
	{
		if (Btn)
		{
			Btn->OnClicked.AddDynamic(this, &APortalProtectPlayerController::QuitToMainMenu);
		}
	};

	// current WBP names
	BindRestart(Cast<UButton>(MenuWidget->GetWidgetFromName(TEXT("ButtonRetry"))));
	BindQuit(Cast<UButton>(MenuWidget->GetWidgetFromName(TEXT("ButtonMainMenu"))));
	// legacy aliases if someone renames back
	BindRestart(Cast<UButton>(MenuWidget->GetWidgetFromName(TEXT("ButtonRestart"))));
	BindQuit(Cast<UButton>(MenuWidget->GetWidgetFromName(TEXT("ButtonQuit"))));
}

// pause, show cursor, load pause widget, wire resume/quit if buttons exist
void APortalProtectPlayerController::ShowPauseMenu()
{
	if (bPauseMenuOpen)
	{
		return;
	}

	UClass* WidgetClass = ResolvePauseMenuClass();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Pause menu widget class missing (expected /Game/UI/Widgets/WBP_PauseMenu)."));
		return;
	}

	if (!PauseMenuWidget)
	{
		PauseMenuWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (!PauseMenuWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create WBP_PauseMenu."));
			return;
		}

		// hook common button names - BP can also call ResumeGame / QuitToMainMenu directly
		if (UButton* ResumeBtn = Cast<UButton>(PauseMenuWidget->GetWidgetFromName(TEXT("ButtonResume"))))
		{
			ResumeBtn->OnClicked.AddDynamic(this, &APortalProtectPlayerController::ResumeGame);
		}
		if (UButton* QuitBtn = Cast<UButton>(PauseMenuWidget->GetWidgetFromName(TEXT("ButtonQuit"))))
		{
			QuitBtn->OnClicked.AddDynamic(this, &APortalProtectPlayerController::QuitToMainMenu);
		}
	}

	PauseMenuWidget->AddToViewport(100);
	PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	SetPause(true);
	bPauseMenuOpen = true;
}

void APortalProtectPlayerController::HidePauseMenu()
{
	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	SetPause(false);
	bPauseMenuOpen = false;
}

// same deal for game over screen
void APortalProtectPlayerController::ShowGameOverMenu()
{
	if (bGameOverMenuOpen || bVictoryMenuOpen)
	{
		return; // don't stack lose UI on a win screen
	}

	if (bPauseMenuOpen)
	{
		HidePauseMenu();
	}

	UClass* WidgetClass = ResolveGameOverMenuClass();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Game over menu widget class missing (expected /Game/UI/Widgets/WBP_GameOverMenu)."));
		return;
	}

	if (!GameOverMenuWidget)
	{
		GameOverMenuWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (!GameOverMenuWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create WBP_GameOverMenu."));
			return;
		}

		BindEndMenuButtons(GameOverMenuWidget);
	}

	// final score from game mode - every time we show (widget may already exist)
	UpdateGameOverScoreText();

	GameOverMenuWidget->AddToViewport(110);
	GameOverMenuWidget->SetVisibility(ESlateVisibility::Visible);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(GameOverMenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	SetPause(true);
	bGameOverMenuOpen = true;
}

// victory screen - same pause/cursor flow as game over, different widget
void APortalProtectPlayerController::ShowVictoryMenu()
{
	if (bVictoryMenuOpen || bGameOverMenuOpen)
	{
		return;
	}

	if (bPauseMenuOpen)
	{
		HidePauseMenu();
	}

	UClass* WidgetClass = ResolveVictoryMenuClass();
	if (!WidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("Victory menu widget class missing (expected /Game/UI/Widgets/WBP_VictoryScreen)."));
		return;
	}

	if (!VictoryMenuWidget)
	{
		VictoryMenuWidget = CreateWidget<UUserWidget>(this, WidgetClass);
		if (!VictoryMenuWidget)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create WBP_VictoryScreen."));
			return;
		}

		BindEndMenuButtons(VictoryMenuWidget);
	}

	UpdateVictoryScoreText();

	VictoryMenuWidget->AddToViewport(110);
	VictoryMenuWidget->SetVisibility(ESlateVisibility::Visible);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(VictoryMenuWidget->TakeWidget());
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	SetPause(true);
	bVictoryMenuOpen = true;
}

void APortalProtectPlayerController::UpdateGameOverScoreText()
{
	UpdateEndMenuScoreText(GameOverMenuWidget);
}

void APortalProtectPlayerController::UpdateVictoryScoreText()
{
	UpdateEndMenuScoreText(VictoryMenuWidget);
}

void APortalProtectPlayerController::UpdateEndMenuScoreText(UUserWidget* MenuWidget)
{
	if (!MenuWidget)
	{
		return;
	}

	APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr;
	if (!GM)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateEndMenuScoreText: no PortalProtectGameMode."));
		return;
	}

	const int32 FinalScore = GM->GetScore();
	const FText ScoreLabel = FText::FromString(FString::Printf(TEXT("Score: %d"), FinalScore));

	// configured name first, then common designer aliases
	TArray<FName> Candidates;
	Candidates.Add(ScoreTextWidgetName);
	static const TCHAR* Fallbacks[] = {
		TEXT("TextScore"),
		TEXT("ScoreText"),
		TEXT("PointsText"),
		TEXT("Text_Points"),
		TEXT("TextPoints"),
		TEXT("Text_Score"),
		TEXT("Score"),
		TEXT("Points")
	};
	for (const TCHAR* Name : Fallbacks)
	{
		Candidates.AddUnique(FName(Name));
	}

	for (const FName& Candidate : Candidates)
	{
		if (Candidate.IsNone())
		{
			continue;
		}
		if (UTextBlock* ScoreText = Cast<UTextBlock>(MenuWidget->GetWidgetFromName(Candidate)))
		{
			ScoreText->SetText(ScoreLabel);
			UE_LOG(LogTemp, Log, TEXT("End-menu score set on TextBlock '%s' -> %d"), *Candidate.ToString(), FinalScore);
			return;
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("End-menu score TextBlock not found (wanted '%s'). Rename the score text to TextScore, or set ScoreTextWidgetName on PortalProtectPlayerController."),
		*ScoreTextWidgetName.ToString());
}

void APortalProtectPlayerController::HideGameOverMenu()
{
	if (GameOverMenuWidget)
	{
		GameOverMenuWidget->RemoveFromParent();
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	SetPause(false);
	bGameOverMenuOpen = false;
}

void APortalProtectPlayerController::HideVictoryMenu()
{
	if (VictoryMenuWidget)
	{
		VictoryMenuWidget->RemoveFromParent();
	}

	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;

	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	SetPause(false);
	bVictoryMenuOpen = false;
}
