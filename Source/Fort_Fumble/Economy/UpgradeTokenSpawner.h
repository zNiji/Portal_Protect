// rare token scatter — same off-path picks as coins, on a much slower loop
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UpgradeTokenSpawner.generated.h"

class AProceduralTerrainActor;
class AUpgradeTokenPickup;

UCLASS()
class FORT_FUMBLE_API AUpgradeTokenSpawner : public AActor
{
	GENERATED_BODY()

public:
	AUpgradeTokenSpawner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "UpgradeToken")
	void Configure(AProceduralTerrainActor* InTerrain);

	// one on the board at match start — coins drop 18
	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	int32 InitialTokenCount = 1;

	// cap stays tiny so the map isn't littered with upgrades
	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	int32 MaxActiveTokens = 2;

	// coins respawn every ~2.1s and 20% of those are big.
	// 26s is slower than that big-coin cadence (~10s) on purpose.
	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	float RespawnInterval = 26.f;

	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	TSubclassOf<AUpgradeTokenPickup> TokenClass;

private:
	void SpawnOneToken();
	int32 CountActiveTokens() const;

	UPROPERTY()
	TObjectPtr<AProceduralTerrainActor> Terrain;

	float RespawnTimer = 0.f;
};
