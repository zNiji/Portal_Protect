// runtime terrain actor - builds the whole map when the level starts
// see other functions below for paths, pads, and decoration
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PortalProtectTypes.h"
#include "ProceduralTerrainActor.generated.h"

class UProceduralMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;

UCLASS()
class FORT_FUMBLE_API AProceduralTerrainActor : public AActor
{
	GENERATED_BODY()

public:
	AProceduralTerrainActor();

	virtual void BeginPlay() override;

	// rebuild everything - uses Seed unless bUseRandomSeed rolls a new one
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	void GenerateTerrain();

	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FPortalPath>& GetPaths() const { return Paths; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	const TArray<FDefenderSlotData>& GetDefenderSlots() const { return DefenderSlots; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetTowerLocation() const { return TowerWorldLocation; }

	UFUNCTION(BlueprintPure, Category = "Terrain")
	int32 GetSeed() const { return Seed; }

	// random spot you can walk on but not on a path - good for coins or player spawn
	UFUNCTION(BlueprintCallable, Category = "Terrain")
	bool TryGetRandomOffPathLocation(FVector& OutLocation, float ZOffset = 40.f) const;

	// world pos for a grid cell from the height array
	UFUNCTION(BlueprintPure, Category = "Terrain")
	FVector GetCellWorldLocation(int32 X, int32 Y, float ZOffset = 0.f) const;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 Seed = 1;

	// grid size - forced odd so there's a real center cell, bigger = larger map
	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 Resolution = 73;

	// distance between cells in uu - map span is roughly (Resolution-1) * CellSize
	UPROPERTY(EditAnywhere, Category = "Terrain")
	float CellSize = 110.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	float MaxHeight = 280.f;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 NumPaths = 3;

	UPROPERTY(EditAnywhere, Category = "Terrain")
	int32 PathHalfWidth = 1;

	// how tall the rim walls are
	UPROPERTY(EditAnywhere, Category = "Terrain|Border", meta = (ClampMin = "50"))
	float BorderWallHeight = 280.f;

	// wall thickness - sits just outside the terrain edge
	UPROPERTY(EditAnywhere, Category = "Terrain|Border", meta = (ClampMin = "20"))
	float BorderWallThickness = 80.f;

	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing")
	int32 NumTrees = 32;

	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing")
	int32 NumRocks = 26;

	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing")
	float DressingMinDistFromPad = 180.f;

	// buffer around paths so props don't hang over the road
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DressingPathClearance = 2;

	// empty cells around map center for the portal plaza
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "3", ClampMax = "24"))
	int32 DressingTowerClearanceCells = 9;

	// world radius around tower - stops big trees spawning inside the portal
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "200"))
	float DressingTowerClearanceRadius = 1100.f;

	// minimum gap from a prop bounds center. real clearance is at least the mesh radius plus a margin
	UPROPERTY(EditAnywhere, Category = "Terrain|Dressing", meta = (ClampMin = "50"))
	float CoinDressingClearance = 180.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> TerrainMesh;

	// optional grass mat - leave null for simple green tint (pack foliage mats break on PMC)
	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Visual")
	TObjectPtr<UMaterialInterface> GrassMaterial;

	// optional path/dirt mat - same deal, null = tinted basic shape
	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Visual")
	TObjectPtr<UMaterialInterface> PathMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Dressing")
	TObjectPtr<UStaticMesh> TreeMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Terrain|Dressing")
	TArray<TObjectPtr<UStaticMesh>> RockMeshes;

private:
	float SampleHeight(int32 X, int32 Y) const;
	void CarvePaths();
	void BuildMesh();
	void BuildDefenderSlots();
	void ClearEnvironmentDressing();
	void SpawnEnvironmentDressing();
	void ClearMapBorder();
	void SpawnMapBorder();
	bool IsNearDefenderSlot(const FVector& WorldLoc) const;
	// too close to a tree/rock footprint (bounds radius + margin, not just the pivot)
	bool IsNearDressing(const FVector& WorldLoc) const;
	void RememberDressingFootprint(const UStaticMeshComponent* Comp);
	// path cell or within Radius cells of one (chebyshev)
	bool IsOnOrNearPath(int32 X, int32 Y, int32 Radius) const;
	// how far along a path this cell is - 0 spawn, 1 tower, false if nothing close
	bool TryGetNearestPathProgress(int32 X, int32 Y, int32& OutPathIndex, float& OutProgress, float& OutDist2D) const;
	FVector GridToWorld(int32 X, int32 Y, float Height) const;
	bool IsInside(int32 X, int32 Y) const;
	int32 Index(int32 X, int32 Y) const { return Y * Resolution + X; }

	TArray<float> Heights;
	TArray<uint8> PathMask; // 1 = path cell (enemies only)
	TArray<FPortalPath> Paths;
	TArray<FDefenderSlotData> DefenderSlots;
	FVector TowerWorldLocation = FVector::ZeroVector;
	int32 CenterX = 0;
	int32 CenterY = 0;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DressingComponents;

	// horizontal footprints for dressing and rim rocks — center is bounds origin, radius is XY extent
	struct FDressingFootprint
	{
		FVector Center = FVector::ZeroVector;
		float Radius = 0.f;
	};
	TArray<FDressingFootprint> DressingFootprints;

	// rim walls so the player can't walk off the map
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BorderWallComponents;
};
