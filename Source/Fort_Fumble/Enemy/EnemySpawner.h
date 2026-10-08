// wave spawner - builds compositions, rests between waves, waits for clear

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PortalProtectTypes.h"
#include "EnemySpawner.generated.h"

class AEnemyUnit;
class AProceduralTerrainActor;

UENUM(BlueprintType)
enum class EWavePhase : uint8
{
	Resting,
	Spawning,
	WaitingClear
};

UCLASS()
class FORT_FUMBLE_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void Configure(AProceduralTerrainActor* InTerrain);

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SetSpawningEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Spawner")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Spawner")
	int32 GetEnemiesRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Spawner")
	int32 GetEnemiesLeftToSpawn() const { return EnemiesLeftToSpawn; }

	UFUNCTION(BlueprintPure, Category = "Spawner")
	EWavePhase GetWavePhase() const { return WavePhase; }

	UFUNCTION(BlueprintPure, Category = "Spawner|Waves")
	int32 GetMaxWaves() const { return MaxWaves; }

	// hard cap - clearing this wave ends the match (no endless mode)
	UPROPERTY(EditAnywhere, Category = "Spawner|Waves")
	int32 MaxWaves = 10;

	// seconds between fully clearing a wave and starting the next
	UPROPERTY(EditAnywhere, Category = "Spawner|Waves")
	float RestDuration = 8.5f;

	// gap between individual spawns inside a wave
	UPROPERTY(EditAnywhere, Category = "Spawner|Waves")
	float SpawnInterval = 2.35f;

	// safety: if a stuck enemy blocks forever, force the next wave after this.
	// longer than a slow tank's walk (about 60s) plus a short fight, so a live wave is not skipped.
	// the last wave never force-clears — victory requires every enemy dead
	UPROPERTY(EditAnywhere, Category = "Spawner|Waves")
	float MaxClearWait = 100.f;

	UPROPERTY(EditAnywhere, Category = "Spawner")
	TSubclassOf<AEnemyUnit> EnemyClass;

private:
	void BeginWave(int32 WaveNumber);
	void BuildWaveComposition(int32 WaveNumber);
	// swap a capped slice of the queue into mutants. waves 1-2 stay on the normal roster
	void InjectMutants(int32 WaveNumber);
	void SpawnNextFromQueue();
	void CleanupDeadRefs();
	EEnemyType PickTypeForWave(int32 WaveNumber, FRandomStream& Rng) const;
	// tweak next wave from clear time + tower HP (skill adapt)
	void UpdateAdaptiveDifficulty();

	UPROPERTY()
	TObjectPtr<AProceduralTerrainActor> Terrain;

	TArray<FPortalPath> CachedPaths;
	TArray<EEnemyType> SpawnQueue;
	// parallel to SpawnQueue — wave*1000+slot, fed to mutant profiles
	TArray<int32> SpawnProfileKeys;
	TArray<TWeakObjectPtr<AEnemyUnit>> AliveThisWave;

	EWavePhase WavePhase = EWavePhase::Resting;
	int32 CurrentWave = 0;
	int32 EnemiesLeftToSpawn = 0;
	int32 NextPathIndex = 0;
	float PhaseTimer = 2.0f;
	float ClearWaitTimer = 0.f;
	bool bSpawningEnabled = true;
	bool bVictoryNotified = false;

	// world time when BeginWave fired - used for clear-time skill check
	float WaveStartTimeSeconds = 0.f;
	// applied to the NEXT BuildWaveComposition / rest after a clear
	int32 AdaptiveCountDelta = 0;
	int32 AdaptiveTankChanceBonus = 0;
	float AdaptiveRestMultiplier = 1.f;
};
