// canvas HUD - score / wave / resources / controls split across all four corners

#include "Game/PortalProtectHUD.h"
#include "Game/PortalProtectGameMode.h"
#include "Core/PortalProtectTypes.h"
#include "Tower/CentralTower.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

APortalProtectHUD::APortalProtectHUD()
{
	// engine SDF font - crisp at ~24-32px, no stretching a tiny bitmap
	static ConstructorHelpers::FObjectFinder<UFont> FontFinder(TEXT("/Engine/EngineFonts/RobotoDistanceField"));
	if (FontFinder.Succeeded())
	{
		ScoreFont = FontFinder.Object;
	}
}

// pull stats from game mode / tower each frame
void APortalProtectHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	AGameModeBase* RawGM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	APortalProtectGameMode* GM = Cast<APortalProtectGameMode>(RawGM);
	if (!GM)
	{
		if (!bLoggedScoreDraw)
		{
			bLoggedScoreDraw = true;
			UE_LOG(LogTemp, Warning,
				TEXT("PortalProtectHUD: cannot draw Score, expected PortalProtectGameMode but got %s."),
				RawGM ? *RawGM->GetClass()->GetName() : TEXT("null"));
		}
		return;
	}

	if (!bLoggedScoreDraw)
	{
		bLoggedScoreDraw = true;
		UE_LOG(LogTemp, Log, TEXT("PortalProtectHUD: drawing Score each frame (GetScore=%d)."), GM->GetScore());
	}

	// prefer SDF score font; body falls back through engine fonts so panels never skip draw
	UFont* BodyFont = nullptr;
	UFont* CrispScoreFont = ScoreFont;
	if (GEngine)
	{
		BodyFont = GEngine->GetMediumFont();
		if (!BodyFont)
		{
			BodyFont = GEngine->GetSmallFont();
		}
		if (!CrispScoreFont)
		{
			CrispScoreFont = GEngine->GetSubtitleFont() ? GEngine->GetSubtitleFont() : BodyFont;
		}
	}
	if (!BodyFont)
	{
		BodyFont = CrispScoreFont;
	}

	const float ScreenW = static_cast<float>(Canvas->SizeX);
	const float ScreenH = static_cast<float>(Canvas->SizeY);
	const float Margin = 28.f;
	const float PadX = 22.f;
	const float PadY = 16.f;
	const float LineGap = 9.f;
	const FLinearColor PanelBg(0.f, 0.f, 0.f, 0.62f);
	const FLinearColor ShadowColor(0.f, 0.f, 0.f, 1.f);

	auto Measure = [this](const FString& Text, UFont* Font, float Scale, float& OutW, float& OutH)
	{
		GetTextSize(Text, OutW, OutH, Font, Scale);
		// some fonts briefly report 0 — keep panels on-screen
		if (OutH <= 0.f)
		{
			OutH = 18.f * Scale;
		}
		if (OutW <= 0.f)
		{
			OutW = FMath::Max(40.f, Text.Len() * 9.f * Scale);
		}
	};

	auto DrawPanelText = [&](const TArray<TPair<FString, FLinearColor>>& Lines, UFont* Font, float Scale,
		float AnchorX, float AnchorY, bool bRightAlign, bool bBottomAlign, bool bCenterX) -> float
	{
		if (Lines.Num() == 0)
		{
			return 0.f;
		}
		// last resort: still draw with whatever font we have so a null doesn't blank a corner
		if (!Font)
		{
			Font = ScoreFont;
		}
		if (!Font)
		{
			return 0.f;
		}

		float MaxW = 0.f;
		float TotalH = 0.f;
		TArray<float> LineHeights;
		LineHeights.Reserve(Lines.Num());

		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			float W = 0.f;
			float H = 0.f;
			Measure(Lines[i].Key, Font, Scale, W, H);
			MaxW = FMath::Max(MaxW, W);
			LineHeights.Add(H);
			TotalH += H;
			if (i + 1 < Lines.Num())
			{
				TotalH += LineGap;
			}
		}

		const float PanelW = MaxW + PadX * 2.f;
		const float PanelH = TotalH + PadY * 2.f;
		const float PanelX = bCenterX ? (AnchorX - PanelW * 0.5f) : (bRightAlign ? (AnchorX - PanelW) : AnchorX);
		const float PanelY = bBottomAlign ? (AnchorY - PanelH) : AnchorY;

		DrawRect(PanelBg, PanelX, PanelY, PanelW, PanelH);

		float CursorY = PanelY + PadY;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			float W = 0.f;
			float H = 0.f;
			Measure(Lines[i].Key, Font, Scale, W, H);
			const float TextX = bRightAlign
				? (PanelX + PanelW - PadX - W)
				: (PanelX + PadX);
			DrawText(Lines[i].Key, ShadowColor, TextX + 3.f, CursorY + 3.f, Font, Scale);
			DrawText(Lines[i].Key, Lines[i].Value, TextX, CursorY, Font, Scale);
			CursorY += LineHeights[i] + LineGap;
		}

		return PanelH;
	};

	const ACentralTower* Tower = GM->GetTower();
	const float TowerHP = Tower ? Tower->GetHealth() : 0.f;
	const float TowerMax = Tower ? Tower->GetMaxHealth() : 1.f;

	const int32 WaveNum = GM->GetCurrentWave();
	const int32 EnemiesLeft = GM->GetEnemiesRemainingInWave();

	const FString ScoreLine = FString::Printf(TEXT("Score: %d"), GM->GetScore());
	const FString TitleLine = FString::Printf(TEXT("PORTAL PROTECT  |  Seed: %d"), GM->GetTerrainSeed());
	const FString WaveLine = FString::Printf(TEXT("Wave: %d   |   Enemies left: %d"),
		WaveNum > 0 ? WaveNum : 1, EnemiesLeft);
	const FString HpLine = FString::Printf(TEXT("Tower HP: %.0f / %.0f"), TowerHP, TowerMax);
	const FString SelectedName = APortalProtectGameMode::GetDefenderDisplayName(GM->GetSelectedDefenderType());
	const int32 SelectedCost = GM->GetDefenderCost();
	const FString CoinsLine = FString::Printf(TEXT("Coins: %d   |   Places left: %d"),
		GM->GetCoinBalance(), GM->GetDefendersRemaining());
	const FString TokenLine = FString::Printf(TEXT("Upgrade tokens: %d"), GM->GetUpgradeTokenCount());
	const FString SelectedLine = FString::Printf(TEXT("Selected: %s (%d coins)"), *SelectedName, SelectedCost);
	const FString CostsLine = FString::Printf(TEXT("Costs — Cannon: %d   Marksman: %d   Mortar: %d"),
		GM->GetDefenderCostForType(EDefenderType::Cannon),
		GM->GetDefenderCostForType(EDefenderType::Marksman),
		GM->GetDefenderCostForType(EDefenderType::Mortar));
	const FString ControlsLine = TEXT("1/2/3 or Q/E or wheel — pick defender");
	const FString ControlsLine2 = TEXT("LMB place  |  F upgrade  |  WASD  |  Esc pause  |  R restart");

	// Top-left: score
	{
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Add(TPair<FString, FLinearColor>(ScoreLine, FLinearColor(1.f, 0.95f, 0.12f, 1.f)));
		DrawPanelText(Lines, CrispScoreFont, 1.5f, Margin, Margin, false, false, false);
	}

	// Top-right: title + wave status
	{
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Add(TPair<FString, FLinearColor>(TitleLine, FLinearColor::White));
		Lines.Add(TPair<FString, FLinearColor>(WaveLine, FLinearColor(1.f, 0.55f, 0.35f, 1.f)));
		DrawPanelText(Lines, BodyFont, 1.75f, ScreenW - Margin, Margin, true, false, false);
	}

	// Bottom-left: tower HP + coins / selection
	float BottomLeftPanelH = 0.f;
	{
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Add(TPair<FString, FLinearColor>(HpLine, FLinearColor(0.6f, 0.85f, 1.f, 1.f)));
		Lines.Add(TPair<FString, FLinearColor>(CoinsLine, FLinearColor(1.f, 0.9f, 0.3f, 1.f)));
		Lines.Add(TPair<FString, FLinearColor>(TokenLine, FLinearColor(0.78f, 0.48f, 1.f, 1.f)));
		Lines.Add(TPair<FString, FLinearColor>(SelectedLine, FLinearColor(1.f, 0.9f, 0.3f, 1.f)));
		BottomLeftPanelH = DrawPanelText(Lines, BodyFont, 1.75f, Margin, ScreenH - Margin, false, true, false);
	}

	// Bottom-right: shop costs + controls
	{
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Add(TPair<FString, FLinearColor>(CostsLine, FLinearColor(0.85f, 0.9f, 1.f, 1.f)));
		Lines.Add(TPair<FString, FLinearColor>(ControlsLine, FLinearColor(0.92f, 0.92f, 0.78f, 1.f)));
		Lines.Add(TPair<FString, FLinearColor>(ControlsLine2, FLinearColor(0.92f, 0.92f, 0.78f, 1.f)));
		DrawPanelText(Lines, BodyFont, 1.6f, ScreenW - Margin, ScreenH - Margin, true, true, false);
	}

	// upgrade prompt sits on the unit when it's on screen, otherwise lower-center
	FUpgradePrompt Prompt;
	if (GM->GetNearestUpgradePrompt(Prompt))
	{
		FLinearColor LevelColor = FLinearColor::White;
		if (Prompt.Level >= 2)
		{
			LevelColor = FLinearColor(0.55f, 0.62f, 1.f, 1.f);
		}
		else if (Prompt.Level == 1)
		{
			LevelColor = FLinearColor(1.f, 0.82f, 0.28f, 1.f);
		}
		const FLinearColor ActionColor = Prompt.bCanUpgrade
			? FLinearColor(0.45f, 1.f, 0.55f, 1.f)
			: FLinearColor(1.f, 0.62f, 0.35f, 1.f);

		TArray<TPair<FString, FLinearColor>> PromptLines;
		PromptLines.Add(TPair<FString, FLinearColor>(Prompt.Title, FLinearColor::White));
		PromptLines.Add(TPair<FString, FLinearColor>(Prompt.LevelLine, LevelColor));
		if (!Prompt.HintLine.IsEmpty())
		{
			PromptLines.Add(TPair<FString, FLinearColor>(Prompt.HintLine, FLinearColor(0.82f, 0.9f, 1.f, 1.f)));
		}
		if (!Prompt.ActionLine.IsEmpty())
		{
			PromptLines.Add(TPair<FString, FLinearColor>(Prompt.ActionLine, ActionColor));
		}

		bool bDrewOnTarget = false;
		if (APlayerController* PC = GetOwningPlayerController())
		{
			FVector CamLoc = FVector::ZeroVector;
			FRotator CamRot = FRotator::ZeroRotator;
			PC->GetPlayerViewPoint(CamLoc, CamRot);
			const bool bInFront = FVector::DotProduct(CamRot.Vector(), Prompt.WorldAnchor - CamLoc) > 0.f;
			const FVector Screen = Canvas->Project(Prompt.WorldAnchor, false);
			const bool bOnScreen = bInFront
				&& Screen.X >= 48.f && Screen.X <= ScreenW - 48.f
				&& Screen.Y >= 48.f && Screen.Y <= ScreenH - 48.f;
			if (bOnScreen)
			{
				const float AnchorX = FMath::Clamp(Screen.X, 180.f, ScreenW - 180.f);
				const float AnchorY = FMath::Clamp(Screen.Y - 10.f, 120.f, ScreenH * 0.78f);
				DrawPanelText(PromptLines, BodyFont, 1.45f, AnchorX, AnchorY, false, true, true);
				bDrewOnTarget = true;
			}
		}
		if (!bDrewOnTarget)
		{
			DrawPanelText(PromptLines, BodyFont, 1.55f, ScreenW * 0.5f, ScreenH * 0.62f, false, false, true);
		}
	}

	const float CX = ScreenW * 0.5f;
	const float CY = ScreenH * 0.5f;
	DrawLine(CX - 10.f, CY, CX + 10.f, CY, FLinearColor(1.f, 1.f, 1.f, 0.7f));
	DrawLine(CX, CY - 10.f, CX, CY + 10.f, FLinearColor(1.f, 1.f, 1.f, 0.7f));

	// transient status sits just above the bottom-left panel
	const FString Status = GM->GetStatusMessage();
	if (!Status.IsEmpty())
	{
		TArray<TPair<FString, FLinearColor>> Lines;
		Lines.Add(TPair<FString, FLinearColor>(Status, FLinearColor(1.f, 0.75f, 0.35f, 1.f)));
		DrawPanelText(Lines, BodyFont, 1.75f, Margin, ScreenH - Margin - BottomLeftPanelH - 12.f, false, true, false);
	}

	// centered wave / win banner (GameMode owns the timer)
	const FString Banner = GM->GetWaveBannerText();
	const float BannerTime = GM->GetWaveBannerTimeRemaining();
	if (!Banner.IsEmpty() && BannerTime > 0.f)
	{
		UFont* BannerFont = CrispScoreFont ? CrispScoreFont : BodyFont;
		const float BannerScale = 3.6f;

		// fade out in the last ~0.75s (skip fade for permanent win banner)
		float Alpha = 1.f;
		if (!GM->IsVictory() && BannerTime < 0.75f)
		{
			Alpha = FMath::Clamp(BannerTime / 0.75f, 0.f, 1.f);
		}

		float TextW = 0.f;
		float TextH = 0.f;
		GetTextSize(Banner, TextW, TextH, BannerFont, BannerScale);

		const float DrawX = (ScreenW - TextW) * 0.5f;
		const float DrawY = ScreenH * 0.28f;

		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f * Alpha),
			DrawX - PadX, DrawY - PadY, TextW + PadX * 2.f, TextH + PadY * 2.f);

		const FLinearColor Shadow(0.f, 0.f, 0.f, Alpha);
		const FLinearColor Fill = GM->IsVictory()
			? FLinearColor(0.35f, 1.f, 0.45f, Alpha)
			: FLinearColor(1.f, 0.92f, 0.35f, Alpha);

		DrawText(Banner, Shadow, DrawX + 4.f, DrawY + 4.f, BannerFont, BannerScale);
		DrawText(Banner, Fill, DrawX, DrawY, BannerFont, BannerScale);
	}
}
