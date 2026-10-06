// shared structs for terrain, spawners, game mode - keeps everyone on the same data
#pragma once

#include "CoreMinimal.h"
#include "PortalProtectTypes.generated.h"

// slime = baseline, runner = fast cactus, tank = heavy chest monster
// mutant = seeded per spawn (size, tint, mesh, stats) — see EnemyUnit::ApplyProceduralProfile
UENUM(BlueprintType)
enum class EEnemyType : uint8
{
	Slime UMETA(DisplayName = "Slime"),
	Runner UMETA(DisplayName = "Runner"),
	Tank UMETA(DisplayName = "Tank"),
	Mutant UMETA(DisplayName = "Mutant")
};

// cannon = balanced, marksman = long range burst, mortar = splash groups
UENUM(BlueprintType)
enum class EDefenderType : uint8
{
	Cannon UMETA(DisplayName = "Cannon"),
	Marksman UMETA(DisplayName = "Marksman"),
	Mortar UMETA(DisplayName = "Mortar")
};

// one enemy route - waypoints from map edge to the tower
USTRUCT(BlueprintType)
struct FPortalPath
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FVector> Waypoints;

	UPROPERTY(BlueprintReadOnly)
	FVector SpawnLocation = FVector::ZeroVector;
};

// where a defender pad goes - terrain picks spots off the paths
USTRUCT(BlueprintType)
struct FDefenderSlotData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	bool bOccupied = false;
};

// proximity upgrade popup — filled by the game mode, drawn by the HUD
struct FUpgradePrompt
{
	bool bValid = false;
	FString Title;
	FString LevelLine;
	FString HintLine;
	FString ActionLine;
	int32 Level = 0;
	bool bCanUpgrade = false;
	FVector WorldAnchor = FVector::ZeroVector;
};
