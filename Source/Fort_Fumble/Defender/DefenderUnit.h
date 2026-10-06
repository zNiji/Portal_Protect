// placeable cannon - auto-aims and shoots slimes in range
// spawned on yellow pads when you spend coins
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/PortalProtectTypes.h"
#include "DefenderUnit.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class AEnemyUnit;
class ADefenderPlacementSpot;

UCLASS()
class FORT_FUMBLE_API ADefenderUnit : public AActor
{
	GENERATED_BODY()

public:
	ADefenderUnit();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Defender")
	void InitializeAsType(EDefenderType InType);

	// remember which pad we sit on so death can free it for reuse
	UFUNCTION(BlueprintCallable, Category = "Defender")
	void SetOwningSpot(ADefenderPlacementSpot* Spot);

	UFUNCTION(BlueprintCallable, Category = "Defender")
	void ApplyDamage(float Amount);

	UFUNCTION(BlueprintPure, Category = "Defender")
	EDefenderType GetDefenderType() const { return DefenderType; }

	UFUNCTION(BlueprintPure, Category = "Defender")
	bool IsAlive() const { return Health > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Defender")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Defender")
	float GetAttackRange() const { return AttackRange; }

	// how far pivot is above ground after scale - used when spawning on pad
	UFUNCTION(BlueprintPure, Category = "Defender")
	float GetPivotToGroundOffset() const { return PivotToGroundOffset; }

	static constexpr int32 MaxUpgradeLevel = 2;

	UFUNCTION(BlueprintPure, Category = "Defender|Upgrade")
	int32 GetUpgradeLevel() const { return UpgradeLevel; }

	UFUNCTION(BlueprintPure, Category = "Defender|Upgrade")
	bool CanAcceptUpgrade() const { return IsAlive() && UpgradeLevel < MaxUpgradeLevel; }

	// empty once this unit is already maxed
	UFUNCTION(BlueprintPure, Category = "Defender|Upgrade")
	FString GetNextUpgradeHint() const;

	// one step. caller spends the token only if this returns true
	UFUNCTION(BlueprintCallable, Category = "Defender|Upgrade")
	bool ApplyNextUpgrade();

	UPROPERTY(EditAnywhere, Category = "Defender")
	float MaxHealth = 90.f;

	// enemies stop-to-shoot at min(their range, this) * EngageStopFactor — keep that inside this value
	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackRange = 750.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackDamage = 18.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float AttackCooldown = 0.65f;

	// mortar only — splash on primary impact
	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float SplashRadius = 240.f;

	UPROPERTY(EditAnywhere, Category = "Defender|Combat")
	float SplashDamage = 8.f;

	UPROPERTY(EditAnywhere, Category = "Defender")
	EDefenderType DefenderType = EDefenderType::Cannon;

	// extra yaw if the imported mesh still points the wrong way
	UPROPERTY(EditAnywhere, Category = "Defender|Aim")
	float AimYawOffset = 0.f;

	// how fast the barrel visually tracks targets
	UPROPERTY(EditAnywhere, Category = "Defender|Aim", meta = (ClampMin = "0.1"))
	float AimInterpSpeed = 6.f;

	// pitch clamp while tracking (degrees)
	UPROPERTY(EditAnywhere, Category = "Defender|Aim")
	float AimMaxPitch = 18.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	// hidden until level 1 — gold ring, then a larger cyan-violet ring
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> UpgradeRing;

private:
	void UpdateAim(float DeltaTime);
	void TryAttack();
	AEnemyUnit* FindAttackTarget() const;
	AEnemyUnit* FindNearestEnemyInRange() const;
	AEnemyUnit* FindMarksmanTarget() const;
	void ApplySplashAt(const FVector& Center, AEnemyUnit* PrimaryTarget);
	void SetupMeshForType();
	// pack mats often blank after Interchange — force mat + BaseColor tex onto all slots
	void ApplyStylizedTurretMaterials(const TCHAR* MatPath, const TCHAR* TexPath);
	void ApplyMeshTint(const FLinearColor& Tint);
	void RefreshColor();
	void EnsureUpgradeRing();
	void UpdateUpgradeAccent();
	// pad stays; clear occupied + refund place budget
	void ReleasePlacementOnDeath();

	float Health = 90.f;
	float AttackTimer = 0.f;
	float BaseMeshScale = 1.f;
	float PivotToGroundOffset = 40.f;
	float FootprintDiameter = 140.f;
	float MeshLocalMinZ = 0.f;
	float UpgradeVisualScale = 1.f;
	int32 UpgradeLevel = 0;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AccentMID;
	bool bUsingCannonMesh = false;
	bool bTypeConfigured = false;
	bool bUpgradeRingBuilt = false;
	bool bReleasedPlacement = false; // avoid double-refund if EndPlay fires twice
	FVector FallbackScaleMul = FVector(1.f);

	TWeakObjectPtr<ADefenderPlacementSpot> OwningSpot;
};
