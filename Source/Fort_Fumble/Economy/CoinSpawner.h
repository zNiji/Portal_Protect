// keeps coins scattered on walkable off-path ground and respawns over time
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoinSpawner.generated.h"

class AProceduralTerrainActor;
class ACoinPickup;

UCLASS()
class FORT_FUMBLE_API ACoinSpawner : public AActor
{
	GENERATED_BODY()

public:
	ACoinSpawner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Coins")
	void Configure(AProceduralTerrainActor* InTerrain);

	// a few more on the board at match start
	UPROPERTY(EditAnywhere, Category = "Coins")
	int32 InitialCoinCount = 18;

	// hard cap — bump a bit so the faster respawn has room to breathe
	UPROPERTY(EditAnywhere, Category = "Coins")
	int32 MaxActiveCoins = 26;

	// was 3.5s — snappier top-ups so the map doesn't feel empty
	UPROPERTY(EditAnywhere, Category = "Coins")
	float RespawnInterval = 2.1f;

	/** Chance each spawn is a big coin (value 10, larger). Rest are small (value 5). */
	UPROPERTY(EditAnywhere, Category = "Coins", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BigCoinChance = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Coins")
	TSubclassOf<ACoinPickup> CoinClass;

private:
	void SpawnOneCoin();
	int32 CountActiveCoins() const;

	UPROPERTY()
	TObjectPtr<AProceduralTerrainActor> Terrain;

	float RespawnTimer = 0.f;
};
