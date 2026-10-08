// wave logic - rest, spawn mix, wait until path is clear

#include "Enemy/EnemySpawner.h"
#include "Enemy/EnemyUnit.h"
#include "Game/PortalProtectGameMode.h"
#include "Terrain/ProceduralTerrainActor.h"
#include "Tower/CentralTower.h"
#include "Engine/World.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	EnemyClass = AEnemyUnit::StaticClass();
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();
	// short intro rest so the player can look around before wave 1
	WavePhase = EWavePhase::Resting;
	CurrentWave = 0;
	PhaseTimer = 3.0f;
}

void AEnemySpawner::Configure(AProceduralTerrainActor* InTerrain)
{
	Terrain = InTerrain;
	CachedPaths.Reset();
	if (Terrain)
	{
		CachedPaths = Terrain->GetPaths();
	}
}

void AEnemySpawner::SetSpawningEnabled(bool bEnabled)
{
	bSpawningEnabled = bEnabled;
}

int32 AEnemySpawner::GetEnemiesRemaining() const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<AEnemyUnit>& Ref : AliveThisWave)
	{
		if (Ref.IsValid() && Ref->IsAlive())
		{
			++Alive;
		}
	}
	return Alive + EnemiesLeftToSpawn;
}

void AEnemySpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bSpawningEnabled || CachedPaths.Num() == 0)
	{
		return;
	}

	CleanupDeadRefs();

	switch (WavePhase)
	{
	case EWavePhase::Resting:
	{
		PhaseTimer -= DeltaTime;
		if (PhaseTimer <= 0.f)
		{
			// don't start past the last wave
			if (CurrentWave >= MaxWaves)
			{
				bSpawningEnabled = false;
				return;
			}
			BeginWave(CurrentWave + 1);
		}
		break;
	}
	case EWavePhase::Spawning:
	{
		PhaseTimer -= DeltaTime;
		if (PhaseTimer <= 0.f)
		{
			SpawnNextFromQueue();
			if (EnemiesLeftToSpawn > 0)
			{
				PhaseTimer = SpawnInterval;
			}
			else
			{
				WavePhase = EWavePhase::WaitingClear;
				ClearWaitTimer = 0.f;
			}
		}
		break;
	}
	case EWavePhase::WaitingClear:
	{
		ClearWaitTimer += DeltaTime;
		const int32 Alive = GetEnemiesRemaining();
		const bool bCleared = Alive <= 0;
		// timeout can start the next wave if something is stuck. it must not award the win
		const bool bForceNext = !bCleared && ClearWaitTimer >= MaxClearWait && CurrentWave < MaxWaves;
		if (bCleared || bForceNext)
		{
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Wave %d %s"),
				CurrentWave, bCleared ? TEXT("cleared") : TEXT("forced forward with enemies still alive"));

			// bake skill feedback into next wave BEFORE we decide rest length
			UpdateAdaptiveDifficulty();

			// last wave done = victory, no more rests / waves
			if (CurrentWave >= MaxWaves)
			{
				bSpawningEnabled = false;
				if (!bVictoryNotified)
				{
					bVictoryNotified = true;
					if (APortalProtectGameMode* GM = GetWorld()->GetAuthGameMode<APortalProtectGameMode>())
					{
						GM->NotifyAllWavesCleared();
					}
				}
				return;
			}

			WavePhase = EWavePhase::Resting;
			// later waves get a tiny bit less rest so pressure creeps up
			const float Scale = FMath::Clamp(1.f - (CurrentWave - 1) * 0.04f, 0.7f, 1.f);
			PhaseTimer = RestDuration * Scale * AdaptiveRestMultiplier;
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Resting %.1fs before next wave (adapt x%.2f)"),
				PhaseTimer, AdaptiveRestMultiplier);
		}
		break;
	}
	default:
		break;
	}
}

void AEnemySpawner::BeginWave(int32 WaveNumber)
{
	CurrentWave = FMath::Clamp(WaveNumber, 1, MaxWaves);
	BuildWaveComposition(CurrentWave);
	EnemiesLeftToSpawn = SpawnQueue.Num();
	AliveThisWave.Reset();
	WavePhase = EWavePhase::Spawning;
	PhaseTimer = 0.25f;
	ClearWaitTimer = 0.f;
	WaveStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	int32 MutantCount = 0;
	for (const EEnemyType Type : SpawnQueue)
	{
		if (Type == EEnemyType::Mutant)
		{
			++MutantCount;
		}
	}

	// tell HUD to flash "Wave N" (and a mutant tag when this wave rolled any)
	if (APortalProtectGameMode* GM = GetWorld()->GetAuthGameMode<APortalProtectGameMode>())
	{
		GM->ShowWaveBanner(CurrentWave, MutantCount > 0);
	}

	UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Starting wave %d with %d enemies (%d mutants, adapt %+d)"),
		CurrentWave, EnemiesLeftToSpawn, MutantCount, AdaptiveCountDelta);
}

void AEnemySpawner::BuildWaveComposition(int32 WaveNumber)
{
	SpawnQueue.Reset();
	SpawnProfileKeys.Reset();

	// base count grows with wave, seed keeps mixes repeatable per wave index
	FRandomStream Rng(WaveNumber * 9176 + 42);
	// early waves stay lean so wave 2 isn't a meat grinder
	int32 Count;
	if (WaveNumber <= 3)
	{
		Count = FMath::Clamp(2 + WaveNumber + Rng.RandRange(0, 1), 3, 7);
	}
	else
	{
		Count = FMath::Clamp(3 + WaveNumber * 2 + Rng.RandRange(0, 1), 3, 28);
	}

	// skill adapt from previous clear - keep early tutorial waves mostly stable
	if (WaveNumber > 1)
	{
		Count = FMath::Clamp(Count + AdaptiveCountDelta, 2, 28);
	}

	// later waves add a few bodies so a handful of upgraded turrets still get pushed
	if (WaveNumber >= 6)
	{
		Count += (WaveNumber >= 9) ? 2 : 1;
		Count = FMath::Clamp(Count, 2, 30);
	}

	for (int32 i = 0; i < Count; ++i)
	{
		SpawnQueue.Add(PickTypeForWave(WaveNumber, Rng));
	}

	// shuffle a bit so tanks aren't always last
	for (int32 i = SpawnQueue.Num() - 1; i > 0; --i)
	{
		const int32 j = Rng.RandRange(0, i);
		SpawnQueue.Swap(i, j);
	}

	InjectMutants(WaveNumber);
}

void AEnemySpawner::InjectMutants(int32 WaveNumber)
{
	SpawnProfileKeys.SetNum(SpawnQueue.Num());
	for (int32 i = 0; i < SpawnQueue.Num(); ++i)
	{
		// stable id for this slot so a seed replays the same mutant body
		SpawnProfileKeys[i] = WaveNumber * 1000 + i;
	}

	// waves 1-2 stay on slime / runner so the roster is learned first
	if (WaveNumber < 3 || SpawnQueue.Num() == 0)
	{
		return;
	}

	// separate stream from the roster mix, so slime/runner/tank odds stay put
	uint32 Mix = static_cast<uint32>(Terrain ? Terrain->GetSeed() : 0) * 0x9E3779B1u;
	const uint32 WaveSalt = static_cast<uint32>(WaveNumber * 40503 + 91);
	Mix ^= WaveSalt + 0x85EBCA77u + (Mix << 6) + (Mix >> 2);
	FRandomStream MutantRng(static_cast<int32>(Mix));

	// wave 3 is rare (~8%). later waves climb and stop at a quarter of the slots
	const int32 Chance = FMath::Clamp(8 + (WaveNumber - 3) * 4, 0, 25);
	const int32 Cap = FMath::Max(1, SpawnQueue.Num() / 4);

	TArray<int32> Hits;
	for (int32 i = 0; i < SpawnQueue.Num(); ++i)
	{
		if (MutantRng.RandRange(0, 99) < Chance)
		{
			Hits.Add(i);
		}
	}

	while (Hits.Num() > Cap)
	{
		Hits.RemoveAt(MutantRng.RandRange(0, Hits.Num() - 1));
	}

	for (const int32 Index : Hits)
	{
		SpawnQueue[Index] = EEnemyType::Mutant;
	}
}

void AEnemySpawner::UpdateAdaptiveDifficulty()
{
	// defaults = neutral; overwritten below from this clear
	AdaptiveCountDelta = 0;
	AdaptiveTankChanceBonus = 0;
	AdaptiveRestMultiplier = 1.f;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float ClearTime = FMath::Max(0.f, Now - WaveStartTimeSeconds);
	// rough expected clear: spawn drip + fight time that grows with wave index
	const float ExpectedClear = 16.f + CurrentWave * 5.f;

	float TowerHPPct = 1.f;
	if (APortalProtectGameMode* GM = GetWorld()->GetAuthGameMode<APortalProtectGameMode>())
	{
		if (ACentralTower* Tower = GM->GetTower())
		{
			TowerHPPct = (Tower->GetMaxHealth() > 0.f)
				? (Tower->GetHealth() / Tower->GetMaxHealth())
				: 1.f;
		}
	}

	const bool bFastClear = ClearTime < ExpectedClear * 0.65f;
	const bool bSlowClear = ClearTime > ExpectedClear * 1.35f;
	const bool bHealthyTower = TowerHPPct >= 0.7f;
	const bool bLowTower = TowerHPPct <= 0.4f;

	if (bFastClear && bHealthyTower)
	{
		// crushing it → slightly denser / tankier next wave
		AdaptiveCountDelta = (ClearTime < ExpectedClear * 0.45f) ? 2 : 1;
		AdaptiveTankChanceBonus = 8;
		AdaptiveRestMultiplier = 0.9f;
	}
	else if (bSlowClear || bLowTower)
	{
		// struggling → one fewer enemy and a longer breather
		AdaptiveCountDelta = -1;
		AdaptiveTankChanceBonus = -6;
		AdaptiveRestMultiplier = 1.3f;
	}

	UE_LOG(LogTemp, Log,
		TEXT("[PortalProtect] Adapt after wave %d: clear=%.1fs (expect ~%.1fs) tower=%.0f%% -> count%+d tank%+d restx%.2f"),
		CurrentWave, ClearTime, ExpectedClear, TowerHPPct * 100.f,
		AdaptiveCountDelta, AdaptiveTankChanceBonus, AdaptiveRestMultiplier);
}

EEnemyType AEnemySpawner::PickTypeForWave(int32 WaveNumber, FRandomStream& Rng) const
{
	// wave 1 = only slimes
	if (WaveNumber <= 1)
	{
		return EEnemyType::Slime;
	}

	const int32 Roll = Rng.RandRange(0, 99);

	if (WaveNumber == 2)
	{
		// introduce runners
		return (Roll < 55) ? EEnemyType::Slime : EEnemyType::Runner;
	}

	if (WaveNumber == 3)
	{
		// first tanks show up, still mostly slime/runner (+/- adapt bias)
		const int32 TankFloor = FMath::Clamp(80 - AdaptiveTankChanceBonus, 60, 90);
		if (Roll < 45) return EEnemyType::Slime;
		if (Roll < TankFloor) return EEnemyType::Runner;
		return EEnemyType::Tank;
	}

	// later waves: more mixed, slowly more tanks (+ skill bias)
	const int32 TankChance = FMath::Clamp(12 + (WaveNumber - 3) * 4 + AdaptiveTankChanceBonus, 6, 40);
	const int32 RunnerChance = FMath::Clamp(30 + (WaveNumber - 2) * 3, 30, 45);
	if (Roll < TankChance) return EEnemyType::Tank;
	if (Roll < TankChance + RunnerChance) return EEnemyType::Runner;
	return EEnemyType::Slime;
}

void AEnemySpawner::SpawnNextFromQueue()
{
	if (!EnemyClass || CachedPaths.Num() == 0 || SpawnQueue.Num() == 0)
	{
		EnemiesLeftToSpawn = 0;
		return;
	}

	const EEnemyType Type = SpawnQueue[0];
	const int32 ProfileKey = SpawnProfileKeys.IsValidIndex(0) ? SpawnProfileKeys[0] : 0;
	SpawnQueue.RemoveAt(0);
	if (SpawnProfileKeys.Num() > 0)
	{
		SpawnProfileKeys.RemoveAt(0);
	}
	EnemiesLeftToSpawn = SpawnQueue.Num();

	// rotate through path start points so pressure spreads across lanes
	const FPortalPath& Path = CachedPaths[NextPathIndex % CachedPaths.Num()];
	NextPathIndex++;

	if (Path.Waypoints.Num() == 0)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnemyUnit* Enemy = GetWorld()->SpawnActor<AEnemyUnit>(
		EnemyClass, Path.Waypoints[0], FRotator::ZeroRotator, Params);
	if (Enemy)
	{
		if (Type == EEnemyType::Mutant)
		{
			const int32 TerrainSeed = Terrain ? Terrain->GetSeed() : 0;
			Enemy->ApplyProceduralProfile(TerrainSeed, ProfileKey);
		}
		else
		{
			Enemy->InitializeAsType(Type);
		}
		Enemy->ApplyLateWaveHealthScale(CurrentWave);
		Enemy->InitializeOnPath(Path.Waypoints);
		AliveThisWave.Add(Enemy);
	}
}

void AEnemySpawner::CleanupDeadRefs()
{
	for (int32 i = AliveThisWave.Num() - 1; i >= 0; --i)
	{
		if (!AliveThisWave[i].IsValid() || !AliveThisWave[i]->IsAlive())
		{
			AliveThisWave.RemoveAt(i);
		}
	}
}
