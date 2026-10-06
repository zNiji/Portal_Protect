// game mode - boots the match, owns coins, placement budget, win/lose

// spawns terrain, tower, spawners, pads, drops player on the map

#pragma once



#include "CoreMinimal.h"

#include "GameFramework/GameModeBase.h"
#include "Core/PortalProtectTypes.h"

#include "PortalProtectGameMode.generated.h"



class AProceduralTerrainActor;

class ACentralTower;

class AEnemySpawner;

class ADefenderPlacementSpot;

class ACoinSpawner;

class AUpgradeTokenSpawner;

struct FUpgradePrompt;



UCLASS()

class FORT_FUMBLE_API APortalProtectGameMode : public AGameModeBase

{

	GENERATED_BODY()



public:

	APortalProtectGameMode();



	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;



	UFUNCTION(BlueprintCallable, Category = "PortalProtect")

	bool TryPlaceDefenderAtSpot(ADefenderPlacementSpot* Spot);



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	bool IsGameOver() const { return bGameOver; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	bool IsVictory() const { return bVictory; }



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	int32 GetDefendersRemaining() const { return DefendersRemaining; }



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	int32 GetCoinBalance() const { return CoinBalance; }



	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "PortalProtect")

	int32 GetScore() const { return Score; }



	UFUNCTION(BlueprintCallable, Category = "PortalProtect")

	void AddScore(int32 Amount);



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	int32 GetPointsPerKill() const { return PointsPerKill; }



	UFUNCTION(BlueprintCallable, Category = "PortalProtect")

	void AddCoins(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "PortalProtect|Upgrade")
	int32 GetUpgradeTokenCount() const { return UpgradeTokenCount; }

	UFUNCTION(BlueprintCallable, Category = "PortalProtect|Upgrade")
	void AddUpgradeTokens(int32 Amount);

	// F while standing near a defender or the tower. spends one token on success
	UFUNCTION(BlueprintCallable, Category = "PortalProtect|Upgrade")
	bool TryUpgradeNearestTarget();

	// nearest living defender or tower inside the interact radius. false if none
	bool GetNearestUpgradePrompt(FUpgradePrompt& OutPrompt) const;



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	int32 GetDefenderCost() const { return GetDefenderCostForType(SelectedDefenderType); }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	EDefenderType GetSelectedDefenderType() const { return SelectedDefenderType; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	int32 GetDefenderCostForType(EDefenderType Type) const;

	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void SetSelectedDefenderType(EDefenderType Type);

	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void CycleSelectedDefenderType(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	static FString GetDefenderDisplayName(EDefenderType Type);



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	FString GetStatusMessage() const { return StatusMessage; }

	// big centered "Wave N" flash - HUD reads these each frame
	UPROPERTY(VisibleAnywhere, Category = "PortalProtect|Waves")
	FString WaveBannerText;

	UPROPERTY(VisibleAnywhere, Category = "PortalProtect|Waves")
	float WaveBannerTimeRemaining = 0.f;

	// how long the wave banner stays up (~2.5–3.5s feels right)
	UPROPERTY(EditAnywhere, Category = "PortalProtect|Waves")
	float WaveBannerDuration = 3.0f;

	UFUNCTION(BlueprintCallable, Category = "PortalProtect|Waves")
	void ShowWaveBanner(int32 WaveNum, bool bIncludesMutants = false);

	UFUNCTION(BlueprintPure, Category = "PortalProtect|Waves")
	FString GetWaveBannerText() const { return WaveBannerText; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect|Waves")
	float GetWaveBannerTimeRemaining() const { return WaveBannerTimeRemaining; }



	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	ACentralTower* GetTower() const { return Tower; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect")
	AEnemySpawner* GetEnemySpawner() const { return Spawner; }

	UFUNCTION(BlueprintPure, Category = "PortalProtect|Waves")
	int32 GetCurrentWave() const;

	UFUNCTION(BlueprintPure, Category = "PortalProtect|Waves")
	int32 GetEnemiesRemainingInWave() const;
	UFUNCTION(BlueprintPure, Category = "PortalProtect")

	int32 GetTerrainSeed() const;



	UFUNCTION(BlueprintCallable, Category = "PortalProtect")

	void NotifyTowerDestroyed();

	// spawner calls this after wave 10 is fully cleared
	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void NotifyAllWavesCleared();

	// defender died — refund one place so pads can be filled again
	UFUNCTION(BlueprintCallable, Category = "PortalProtect")
	void NotifyDefenderDestroyed();



	UFUNCTION(BlueprintCallable, Category = "PortalProtect")

	void PlacePlayerOnTerrain();



	// how many cannons you can still place - terrain aims for 4 pads per path

	UPROPERTY(EditAnywhere, Category = "PortalProtect")

	int32 StartingDefenders = 16;



	UPROPERTY(EditAnywhere, Category = "PortalProtect|Economy")

	int32 StartingCoins = 25;



	UPROPERTY(EditAnywhere, Category = "PortalProtect|Economy")

	int32 CannonCost = 15;

	UPROPERTY(EditAnywhere, Category = "PortalProtect|Economy")
	int32 MarksmanCost = 25;

	UPROPERTY(EditAnywhere, Category = "PortalProtect|Economy")
	int32 MortarCost = 30;



	// running match score - survival ticks + kills, frozen when the tower falls

	UPROPERTY(VisibleAnywhere, Category = "PortalProtect|Score")

	int32 Score = 0;



	UPROPERTY(EditAnywhere, Category = "PortalProtect|Score")

	int32 SurvivalPointsPerSecond = 2; // slow drip so a long match doesn't explode the score



	UPROPERTY(EditAnywhere, Category = "PortalProtect|Score")

	int32 PointsPerKill = 40;



protected:

	void SpawnWorld();

	void SetStatusMessage(const FString& Message, float Duration = 2.5f);

	void ClearStatusMessage();

	AActor* FindNearestUpgradeTarget() const;

	void FillUpgradePrompt(AActor* Target, FUpgradePrompt& OutPrompt) const;

	// drip survival points once a second until game over

	void TickSurvivalScore();



	UPROPERTY()

	TObjectPtr<AProceduralTerrainActor> Terrain;



	UPROPERTY()

	TObjectPtr<ACentralTower> Tower;



	UPROPERTY()

	TObjectPtr<AEnemySpawner> Spawner;



	UPROPERTY()

	TObjectPtr<ACoinSpawner> CoinSpawner;

	UPROPERTY()

	TObjectPtr<AUpgradeTokenSpawner> TokenSpawner;



	UPROPERTY()

	TArray<TObjectPtr<ADefenderPlacementSpot>> PlacementSpots;



	FTimerHandle PlayerPlaceTimer;

	FTimerHandle StatusMessageTimer;

	FTimerHandle SurvivalScoreTimer;

	int32 PlayerPlaceAttempts = 0;

	int32 DefendersRemaining = 16;

	int32 CoinBalance = 25;

	int32 UpgradeTokenCount = 0;

	bool bGameOver = false;

	bool bVictory = false;

	FString StatusMessage;

	EDefenderType SelectedDefenderType = EDefenderType::Cannon;

};

