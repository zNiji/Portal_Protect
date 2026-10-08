// portal objective - HP and passive shooting at nearby slimes

#include "Tower/CentralTower.h"
#include "Core/PortalShaders.h"
#include "Core/UpgradeVisuals.h"
#include "Enemy/EnemyUnit.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ACentralTower::ACentralTower()
{
	PrimaryActorTick.bCanEverTick = true;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	SetRootComponent(BaseMesh);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BaseMesh->SetCollisionObjectType(ECC_WorldStatic);
	BaseMesh->SetCollisionResponseToAllChannels(ECR_Block);
	// enemy projectiles are WorldDynamic overlap - need overlap on portal for damage
	BaseMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	BaseMesh->SetGenerateOverlapEvents(true);

	TurretMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretMesh"));
	TurretMesh->SetupAttachment(BaseMesh);
	TurretMesh->SetVisibility(false);
	TurretMesh->SetHiddenInGame(true);
	TurretMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UpgradeRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("UpgradeRing"));
	UpgradeRing->SetupAttachment(BaseMesh);
	UpgradeRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UpgradeRing->SetCastShadow(false);
	UpgradeRing->SetVisibility(false);
	UpgradeRing->SetHiddenInGame(true);
	UpgradeRing->SetMobility(EComponentMobility::Movable);
	// parent scale is the portal mesh — keep the ring's size in world units
	UpgradeRing->SetUsingAbsoluteScale(true);
	UpgradeRing->SetUsingAbsoluteRotation(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PortalAsset(
		TEXT("/Game/RPGTinyFantasyForest/Mesh/BuildingUtilityDeco/SM_PortalA.SM_PortalA"));
	if (PortalAsset.Succeeded())
	{
		BaseMesh->SetStaticMesh(PortalAsset.Object);
		BaseVisualScale = 1.35f;
		BaseScaleVec = FVector(BaseVisualScale);
		BaseMesh->SetRelativeScale3D(BaseScaleVec);
	}
	else
	{
		// fallback cylinder if forest pack portal mesh is missing
		static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (CylinderAsset.Succeeded())
		{
			BaseMesh->SetStaticMesh(CylinderAsset.Object);
			BaseVisualScale = 1.6f;
			BaseScaleVec = FVector(BaseVisualScale, BaseVisualScale, 2.2f);
			BaseMesh->SetRelativeScale3D(BaseScaleVec);
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PortalMat(
		TEXT("/Game/RPGTinyFantasyForest/Material/MI_Portal01.MI_Portal01"));
	if (PortalMat.Succeeded())
	{
		PortalMaterial = PortalMat.Object;
		BaseMesh->SetMaterial(0, PortalMaterial);
	}

	Health = MaxHealth;
}

void ACentralTower::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	bUsingPortalShader = PortalShaders::ApplyPortalMaterials(BaseMesh, this);
	EnsureUpgradeRing();
	ApplyVisualColor();
}

void ACentralTower::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAlive())
	{
		return;
	}

	if (UpgradeLevel > 0 && IsValid(UpgradeRing))
	{
		UpgradeRing->AddWorldRotation(FRotator(0.f, 28.f * DeltaTime, 0.f));
	}

	// portal doesn't rotate - just shoots on cooldown
	AttackTimer -= DeltaTime;
	if (AttackTimer <= 0.f)
	{
		TryAttack();
		AttackTimer = AttackCooldown;
	}
}

// take damage, shrink visual a bit, broadcast destroy at zero
void ACentralTower::ApplyDamage(float Amount)
{
	if (!IsValid(this) || IsActorBeingDestroyed() || !IsAlive() || Amount <= 0.f)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - (Amount * IncomingDamageScale));
	ApplyVisualColor();

	if (Health <= 0.f)
	{
		OnTowerDestroyed.Broadcast();
	}
}

// instant damage to nearest slime in range (debug line shows the shot)
void ACentralTower::TryAttack()
{
	AEnemyUnit* Target = FindNearestEnemy();
	if (!IsValid(Target) || !Target->IsAlive())
	{
		return;
	}

	const FVector HitLoc = Target->GetActorLocation();
	Target->ApplyDamage(AttackDamage);
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, GetActorLocation() + FVector(0, 0, 160), HitLoc,
			FColor::Cyan, false, 0.15f, 0, 4.f);
	}
}

AEnemyUnit* ACentralTower::FindNearestEnemy() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyUnit::StaticClass(), Found);

	AEnemyUnit* Best = nullptr;
	float BestDistSq = AttackRange * AttackRange;
	const FVector Origin = GetActorLocation();

	for (AActor* Actor : Found)
	{
		AEnemyUnit* Enemy = Cast<AEnemyUnit>(Actor);
		if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || !Enemy->IsAlive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Origin, Enemy->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}
	return Best;
}

void ACentralTower::ApplyVisualColor()
{
	if (!IsValid(BaseMesh))
	{
		return;
	}

	const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	if (bUsingPortalShader)
	{
		// colour lives in the material. keep the mesh scale so collision does not shrink
		PortalShaders::SetPortalHealthAlpha(BaseMesh, Ratio);
		BaseMesh->SetRelativeScale3D(BaseScaleVec * UpgradeVisualScale);
	}
	else
	{
		// pack portal mat has no color param - pulse scale instead for damage feedback
		const float HealthMul = FMath::Lerp(0.85f, 1.f, Ratio);
		BaseMesh->SetRelativeScale3D(BaseScaleVec * HealthMul * UpgradeVisualScale);
		if (IsValid(PortalMaterial))
		{
			BaseMesh->SetMaterial(0, PortalMaterial);
		}
	}

	UpdateUpgradeAccent();
}

namespace TowerUpgradeStats
{
	// hint skips the small fire-rate bump. the other percents have to match these muls
	constexpr float HealthMul = 1.30f;     // +30%
	constexpr float DamageMul = 1.18f;     // +18%
	constexpr float CooldownMul = 0.92f;   // ~+9% fire rate
	constexpr float IncomingMul = 0.90f;   // 10% less damage taken
}

FString ACentralTower::GetNextUpgradeHint() const
{
	if (UpgradeLevel >= MaxUpgradeLevel)
	{
		return FString();
	}
	return TEXT("+30% health, +18% damage, 10% defense");
}

bool ACentralTower::ApplyNextUpgrade()
{
	if (!IsAlive() || UpgradeLevel >= MaxUpgradeLevel)
	{
		return false;
	}

	const float OldMax = MaxHealth;
	MaxHealth *= TowerUpgradeStats::HealthMul;
	const float Gained = MaxHealth - OldMax;
	Health = FMath::Min(MaxHealth, Health + Gained);

	AttackDamage *= TowerUpgradeStats::DamageMul;
	AttackCooldown = FMath::Max(0.2f, AttackCooldown * TowerUpgradeStats::CooldownMul);
	IncomingDamageScale *= TowerUpgradeStats::IncomingMul;

	++UpgradeLevel;
	UpgradeVisualScale = UpgradeVisual::ScaleForLevel(UpgradeLevel);
	ApplyVisualColor();

	UE_LOG(LogTemp, Log,
		TEXT("[PortalProtect] Tower upgraded to Lv %d (HP %.0f/%.0f dmg %.1f cd %.2f taken x%.2f)"),
		UpgradeLevel, Health, MaxHealth, AttackDamage, AttackCooldown, IncomingDamageScale);
	return true;
}

void ACentralTower::EnsureUpgradeRing()
{
	if (!IsValid(UpgradeRing) || bUpgradeRingBuilt)
	{
		return;
	}

	UpgradeVisual::BuildFlatTorus(UpgradeRing);
	bUpgradeRingBuilt = true;
}

void ACentralTower::UpdateUpgradeAccent()
{
	EnsureUpgradeRing();
	if (!IsValid(UpgradeRing) || !IsValid(BaseMesh))
	{
		return;
	}

	if (UpgradeLevel <= 0)
	{
		UpgradeRing->SetVisibility(false);
		UpgradeRing->SetHiddenInGame(true);
		return;
	}

	const FLinearColor Color = UpgradeVisual::AccentForLevel(UpgradeLevel);
	if (!AccentMID)
	{
		AccentMID = UpgradeVisual::MakeAccentMaterial(this, Color);
	}
	if (AccentMID)
	{
		AccentMID->SetVectorParameterValue(TEXT("Color"), Color);
		AccentMID->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (UpgradeLevel >= 2 ? 5.f : 2.5f));
		UpgradeRing->SetMaterial(0, AccentMID);
	}

	const UStaticMesh* PortalMesh = BaseMesh->GetStaticMesh();
	float Radius = 180.f;
	float BottomZ = 12.f;
	if (PortalMesh)
	{
		const FBoxSphereBounds Local = PortalMesh->GetBounds();
		const FVector MeshScale = BaseMesh->GetRelativeScale3D();
		// world radius — ring scale is absolute, so it is not multiplied by the portal again
		Radius = FMath::Max(Local.BoxExtent.X, Local.BoxExtent.Y) * MeshScale.X;
		Radius *= (UpgradeLevel >= 2) ? 1.18f : 1.02f;
		// relative Z is in parent space, which the portal scale already applies
		const float LocalBottom = Local.Origin.Z - Local.BoxExtent.Z;
		const float LiftLocal = 14.f / FMath::Max(MeshScale.Z, 0.01f);
		BottomZ = LocalBottom + LiftLocal;
	}

	const float Uniform = FMath::Max(Radius, 80.f) / UpgradeVisual::TorusMajorRadius;
	UpgradeRing->SetWorldScale3D(FVector(Uniform));
	UpgradeRing->SetRelativeLocation(FVector(0.f, 0.f, BottomZ));
	UpgradeRing->SetHiddenInGame(false);
	UpgradeRing->SetVisibility(true);
}
