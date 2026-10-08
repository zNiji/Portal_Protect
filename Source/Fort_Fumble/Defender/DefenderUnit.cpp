// defender units - cannon / marksman / mortar, aim + auto fire

#include "Defender/DefenderUnit.h"
#include "Defender/DefenderPlacementSpot.h"
#include "Core/MatchVfx.h"
#include "Core/UpgradeVisuals.h"
#include "Enemy/EnemyUnit.h"
#include "Game/PortalProtectGameMode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ADefenderUnit::ADefenderUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Mesh->SetGenerateOverlapEvents(true);

	UpgradeRing = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("UpgradeRing"));
	UpgradeRing->SetupAttachment(Root);
	UpgradeRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UpgradeRing->SetCastShadow(false);
	UpgradeRing->SetVisibility(false);
	UpgradeRing->SetHiddenInGame(true);
	UpgradeRing->SetMobility(EComponentMobility::Movable);

	// ctor-only soft refs so CDO has a cannon mesh before InitializeAsType runs
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CannonAsset(
		TEXT("/Game/cartoon_cannon_low_poly__extracted/source/CannonSketchfab.CannonSketchfab"));
	if (CannonAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CannonAsset.Object);
	}

	Health = MaxHealth;
}

void ADefenderUnit::BeginPlay()
{
	Super::BeginPlay();
	if (!bTypeConfigured)
	{
		InitializeAsType(DefenderType);
	}
	Health = MaxHealth;
	EnsureUpgradeRing();
	RefreshColor();
	UpdateUpgradeAccent();
}

void ADefenderUnit::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// level teardown also hits EndPlay — only free pad when this unit actually dies
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		ReleasePlacementOnDeath();
	}
	Super::EndPlay(EndPlayReason);
}

void ADefenderUnit::SetOwningSpot(ADefenderPlacementSpot* Spot)
{
	OwningSpot = Spot;
}

void ADefenderUnit::InitializeAsType(EDefenderType InType)
{
	DefenderType = InType;
	bTypeConfigured = true;
	bUsingCannonMesh = false;
	FallbackScaleMul = FVector(1.f);

	switch (DefenderType)
	{
	case EDefenderType::Cannon:
		MaxHealth = 90.f;
		AttackRange = 750.f;
		AttackDamage = 18.f;
		AttackCooldown = 1.0f;
		AimMaxPitch = 18.f;
		AimInterpSpeed = 6.f;
		SplashRadius = 0.f;
		SplashDamage = 0.f;
		break;
	case EDefenderType::Marksman:
		MaxHealth = 70.f;
		AttackRange = 1300.f;
		AttackDamage = 40.f;
		AttackCooldown = 2.2f;
		AimMaxPitch = 12.f;
		AimInterpSpeed = 4.f;
		SplashRadius = 0.f;
		SplashDamage = 0.f;
		break;
	case EDefenderType::Mortar:
		MaxHealth = 95.f;
		AttackRange = 600.f;
		AttackDamage = 12.f;
		AttackCooldown = 1.8f;
		AimMaxPitch = 55.f;
		AimInterpSpeed = 5.f;
		// was 240 / 8. direct hit stays 12; splash matches it so neighbors aren't a token tick
		// radius covers a few body widths plus the usual gap to the next slime on the path
		SplashRadius = 480.f;
		SplashDamage = 12.f;
		break;
	default:
		break;
	}

	Health = MaxHealth;
	SetupMeshForType();

	UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Defender InitializeAsType -> %s (range=%.0f dmg=%.0f)"),
		DefenderType == EDefenderType::Marksman ? TEXT("Marksman")
			: (DefenderType == EDefenderType::Mortar ? TEXT("Mortar") : TEXT("Cannon")),
		AttackRange, AttackDamage);
}

void ADefenderUnit::SetupMeshForType()
{
	if (!IsValid(Mesh))
	{
		return;
	}

	// stylized FBX is often ~1m (tiny in UU) — allow large scale; old 2.5 clamp left them pin-sized
	auto FitMeshToFootprint = [this](UStaticMesh* StaticMesh, float TargetFootprint, float RelYaw)
	{
		if (!IsValid(Mesh) || !IsValid(StaticMesh))
		{
			return;
		}
		Mesh->SetStaticMesh(StaticMesh);
		const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
		const float MeshFootprint = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.f;
		BaseMeshScale = FMath::Clamp(TargetFootprint / FMath::Max(MeshFootprint, 0.01f), 0.01f, 300.f);
		Mesh->SetRelativeScale3D(FVector(BaseMeshScale) * FallbackScaleMul);
		Mesh->SetRelativeRotation(FRotator(0.f, RelYaw, 0.f));
		// seats mesh on pad after scale — used by spawn placement
		PivotToGroundOffset = (Bounds.BoxExtent.Z - Bounds.Origin.Z) * BaseMeshScale * FallbackScaleMul.Z;
		MeshLocalMinZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
		FootprintDiameter = TargetFootprint;

		UE_LOG(LogTemp, Log,
			TEXT("[PortalProtect] Mesh bounds BoxExtent=(%.2f,%.2f,%.2f) OriginZ=%.2f footprint=%.2f scale=%.2f pivotOff=%.2f"),
			Bounds.BoxExtent.X, Bounds.BoxExtent.Y, Bounds.BoxExtent.Z,
			Bounds.Origin.Z, MeshFootprint, BaseMeshScale, PivotToGroundOffset);
	};

	static const TCHAR* StylizedA_Mat = TEXT(
		"/Game/Stylized_Turrets_-_Tower_Defense-c74b72af/fbx/stylized_turrets_fbx_extracted/FBX/Stylized_Turrets_A_mat.Stylized_Turrets_A_mat");
	static const TCHAR* StylizedA_Tex = TEXT(
		"/Game/Stylized_Turrets_-_Tower_Defense-c74b72af/fbx/stylized_turrets_fbx_extracted/Textures/Stylized_Turrets_A.Stylized_Turrets_A");
	static const TCHAR* StylizedB_Mat = TEXT(
		"/Game/Stylized_Turrets_-_Tower_Defense-c74b72af/fbx/stylized_turrets_fbx_extracted/FBX/Stylized_Turrets_B_mat.Stylized_Turrets_B_mat");
	static const TCHAR* StylizedB_Tex = TEXT(
		"/Game/Stylized_Turrets_-_Tower_Defense-c74b72af/fbx/stylized_turrets_fbx_extracted/Textures/Stylized_Turrets_B.Stylized_Turrets_B");

	// LoadObject — ConstructorHelpers only works inside constructors
	switch (DefenderType)
	{
	case EDefenderType::Cannon:
	{
		UStaticMesh* CannonMesh = LoadObject<UStaticMesh>(nullptr,
			TEXT("/Game/cartoon_cannon_low_poly__extracted/source/CannonSketchfab.CannonSketchfab"));
		if (CannonMesh)
		{
			FitMeshToFootprint(CannonMesh, 140.f, 90.f);
			bUsingCannonMesh = true;
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Cannon mesh loaded (CannonSketchfab)."));
		}
		else if (UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")))
		{
			UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] Cannon fallback — CannonSketchfab missing from Content."));
			FallbackScaleMul = FVector(0.9f, 0.9f, 1.3f);
			FitMeshToFootprint(Cone, 120.f, 0.f);
			ApplyMeshTint(FLinearColor(0.55f, 0.55f, 0.58f));
		}
		break;
	}
	case EDefenderType::Marksman:
	{
		// tall footprint reads as long-range; swap A_b in editor if barrel faces wrong way
		UStaticMesh* MarksmanMesh = LoadObject<UStaticMesh>(nullptr, TEXT(
			"/Game/Stylized_Turrets_-_Tower_Defense-c74b72af/fbx/stylized_turrets_fbx_extracted/FBX/Stylized_Turrets_A_a.Stylized_Turrets_A_a"));
		if (MarksmanMesh)
		{
			FallbackScaleMul = FVector(0.92f, 0.92f, 1.22f);
			// was 92 — bump toward pad / Cannon (~140) visual weight
			FitMeshToFootprint(MarksmanMesh, 220.f, -90.f);
			ApplyStylizedTurretMaterials(StylizedA_Mat, StylizedA_Tex);
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Marksman mesh loaded (Stylized_Turrets_A_a)."));
		}
		else if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			FallbackScaleMul = FVector(0.55f, 0.55f, 2.2f);
			FitMeshToFootprint(Cylinder, 90.f, 0.f);
			ApplyMeshTint(FLinearColor(0.25f, 0.45f, 0.95f));
			UE_LOG(LogTemp, Warning,
				TEXT("[PortalProtect] Marksman fallback cylinder — check Stylized_Turrets_A_a import path."));
		}
		break;
	}
	case EDefenderType::Mortar:
	{
		// wider B-set mesh + slight squash — splash / lobbing look
		UStaticMesh* MortarMesh = LoadObject<UStaticMesh>(nullptr, TEXT(
			"/Game/Stylized_Turrets_-_Tower_Defense-c74b72af/fbx/stylized_turrets_fbx_extracted/FBX/Stylized_Turrets_B_b.Stylized_Turrets_B_b"));
		if (MortarMesh)
		{
			FallbackScaleMul = FVector(1.12f, 1.12f, 0.92f);
			// was 148 — chunkier than cannon for splash identity
			FitMeshToFootprint(MortarMesh, 260.f, -90.f);
			ApplyStylizedTurretMaterials(StylizedB_Mat, StylizedB_Tex);
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Mortar mesh loaded (Stylized_Turrets_B_b)."));
		}
		else if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			FallbackScaleMul = FVector(1.4f, 1.4f, 0.85f);
			FitMeshToFootprint(Cube, 130.f, 0.f);
			ApplyMeshTint(FLinearColor(0.85f, 0.42f, 0.12f));
			UE_LOG(LogTemp, Warning,
				TEXT("[PortalProtect] Mortar fallback cube — check Stylized_Turrets_B_b import path."));
		}
		break;
	}
	default:
		break;
	}
}

void ADefenderUnit::ApplyStylizedTurretMaterials(const TCHAR* MatPath, const TCHAR* TexPath)
{
	if (!IsValid(Mesh) || !MatPath || !TexPath)
	{
		return;
	}

	auto ApplyToAllSlots = [this](UMaterialInterface* Mat)
	{
		if (!IsValid(Mesh) || !IsValid(Mat))
		{
			return;
		}
		const int32 NumSlots = FMath::Clamp(Mesh->GetNumMaterials(), 1, 32);
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			Mesh->SetMaterial(Slot, Mat);
		}
	};

	UMaterialInterface* PackMat = LoadObject<UMaterialInterface>(nullptr, MatPath);
	UTexture2D* PackTex = LoadObject<UTexture2D>(nullptr, TexPath);

	// prefer pack material; if blank Interchange MIC, MID + wire BaseColor like CoinPickup
	UMaterialInterface* ParentMat = PackMat;
	if (!IsValid(ParentMat))
	{
		ParentMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/RPGTinyFantasyForest/Material/MI_DefaultPBR.MI_DefaultPBR"));
	}
	if (!IsValid(ParentMat))
	{
		ParentMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/RPGTinyFantasyForest/Material/BaseMAT/M_DefaultPBR.M_DefaultPBR"));
	}
	if (!IsValid(ParentMat))
	{
		ParentMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	if (!IsValid(ParentMat))
	{
		UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] No parent mat for stylized turret (%s)."), MatPath);
		return;
	}

	if (IsValid(PackTex))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMat, this);
		if (IsValid(MID))
		{
			static const FName TexParams[] = {
				TEXT("DiffuseColorMap"),
				TEXT("T_BaseColor"),
				TEXT("BaseColor"),
				TEXT("Base Color"),
				TEXT("Diffuse"),
				TEXT("Texture"),
				TEXT("Diffuse Color"),
			};
			for (const FName& Param : TexParams)
			{
				MID->SetTextureParameterValue(Param, PackTex);
			}
			static const FName WeightParams[] = {
				TEXT("DiffuseColorMapWeight"),
				TEXT("AmbientColorMapWeight"),
			};
			for (const FName& Param : WeightParams)
			{
				MID->SetScalarParameterValue(Param, 1.f);
			}
			MID->SetVectorParameterValue(TEXT("DiffuseColor"), FLinearColor::White);
			MID->SetVectorParameterValue(TEXT("AmbientColor"), FLinearColor(0.05f, 0.05f, 0.05f));
			ApplyToAllSlots(MID);
			UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Stylized turret MID applied (tex=%s)."), TexPath);
			return;
		}
	}

	if (IsValid(PackMat))
	{
		ApplyToAllSlots(PackMat);
		UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Stylized turret pack mat applied (%s)."), MatPath);
	}
}

void ADefenderUnit::ApplyMeshTint(const FLinearColor& Tint)
{
	if (!IsValid(Mesh))
	{
		return;
	}
	UMaterialInterface* BaseMat = Mesh->GetMaterial(0);
	if (!IsValid(BaseMat))
	{
		BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (IsValid(BaseMat))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		if (IsValid(MID))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Tint);
			Mesh->SetMaterial(0, MID);
		}
	}
}

void ADefenderUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAlive())
	{
		return;
	}

	UpdateAim(DeltaTime);

	if (UpgradeLevel > 0 && IsValid(UpgradeRing))
	{
		UpgradeRing->AddLocalRotation(FRotator(0.f, 40.f * DeltaTime, 0.f));
	}

	AttackTimer -= DeltaTime;
	if (AttackTimer <= 0.f)
	{
		TryAttack();
		AttackTimer = AttackCooldown;
	}
}

void ADefenderUnit::ApplyDamage(float Amount)
{
	if (!IsValid(this) || IsActorBeingDestroyed() || !IsAlive() || Amount <= 0.f)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	RefreshColor();
	if (Health <= 0.f)
	{
		// free pad + refund place budget before actor goes away (pad actor stays)
		ReleasePlacementOnDeath();
		Destroy();
	}
}

void ADefenderUnit::ReleasePlacementOnDeath()
{
	if (bReleasedPlacement)
	{
		return;
	}
	bReleasedPlacement = true;

	if (ADefenderPlacementSpot* Spot = OwningSpot.Get())
	{
		Spot->SetOccupied(false); // pad stays in world, ready for LMB place again
		OwningSpot.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		if (APortalProtectGameMode* GM = World->GetAuthGameMode<APortalProtectGameMode>())
		{
			GM->NotifyDefenderDestroyed();
		}
	}
}

void ADefenderUnit::UpdateAim(float DeltaTime)
{
	AEnemyUnit* Target = FindAttackTarget();
	if (!IsValid(Target) || !Target->IsAlive())
	{
		return;
	}

	FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	ToTarget.Z += DefenderType == EDefenderType::Mortar ? 20.f : 40.f;
	if (ToTarget.SizeSquared() < 1.f)
	{
		return;
	}

	FRotator Desired = ToTarget.Rotation();
	Desired.Yaw += AimYawOffset;
	Desired.Pitch = FMath::Clamp(Desired.Pitch, -AimMaxPitch, AimMaxPitch);
	Desired.Roll = 0.f;

	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), Desired, DeltaTime, AimInterpSpeed);
	SetActorRotation(NewRot);
}

void ADefenderUnit::TryAttack()
{
	AEnemyUnit* Target = FindAttackTarget();
	if (!IsValid(Target) || !Target->IsAlive())
	{
		return;
	}

	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 80.f);
	const FVector HitLoc = Target->GetActorLocation();
	// keep weak ref — primary may be destroyed by the direct hit before splash runs
	const TWeakObjectPtr<AEnemyUnit> PrimaryWeak(Target);

	Target->ApplyDamage(AttackDamage);

	// mortar only. cannon and marksman stay single-target. primary already took AttackDamage
	if (DefenderType == EDefenderType::Mortar && SplashRadius > 0.f && SplashDamage > 0.f)
	{
		ApplySplashAt(HitLoc, PrimaryWeak.Get());
		if (UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, HitLoc, SplashRadius, 16, FColor::Orange, false, 0.25f, 0, 2.f);
			// same system as a kill, pulled down so the death puff stays louder
			MatchVfx::SpawnDeathBurst(World, HitLoc, 0.55f);
		}
	}

	FColor LineColor = FColor::Green;
	if (DefenderType == EDefenderType::Marksman)
	{
		LineColor = FColor::Cyan;
	}
	else if (DefenderType == EDefenderType::Mortar)
	{
		LineColor = FColor::Orange;
	}

	const float LineThickness = DefenderType == EDefenderType::Marksman ? 5.f : 3.f;
	const float LineDuration = DefenderType == EDefenderType::Marksman ? 0.08f : 0.12f;
	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, Muzzle, HitLoc, LineColor, false, LineDuration, 0, LineThickness);
	}
}

void ADefenderUnit::ApplySplashAt(const FVector& Center, AEnemyUnit* PrimaryTarget)
{
	UWorld* World = GetWorld();
	if (!World || SplashRadius <= 0.f || SplashDamage <= 0.f)
	{
		return;
	}

	// distance check, not an overlap. enemy capsules are ECC_Pawn so a static/dynamic trace misses them
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyUnit::StaticClass(), Found);

	TArray<TWeakObjectPtr<AEnemyUnit>> SplashTargets;
	SplashTargets.Reserve(Found.Num());

	for (AActor* Actor : Found)
	{
		AEnemyUnit* Enemy = Cast<AEnemyUnit>(Actor);
		if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || !Enemy->IsAlive())
		{
			continue;
		}
		if (PrimaryTarget && Enemy == PrimaryTarget)
		{
			continue;
		}
		// center distance plus the enemy sphere, so a body overlapping the blast still counts
		const float Reach = SplashRadius + FMath::Max(0.f, Enemy->GetSimpleCollisionRadius());
		if (FVector::DistSquared2D(Center, Enemy->GetActorLocation()) <= Reach * Reach)
		{
			SplashTargets.Add(Enemy);
		}
	}

	for (const TWeakObjectPtr<AEnemyUnit>& WeakEnemy : SplashTargets)
	{
		AEnemyUnit* Enemy = WeakEnemy.Get();
		if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || !Enemy->IsAlive())
		{
			continue;
		}
		Enemy->ApplyDamage(SplashDamage);
	}
}

AEnemyUnit* ADefenderUnit::FindAttackTarget() const
{
	switch (DefenderType)
	{
	case EDefenderType::Marksman:
		return FindMarksmanTarget();
	case EDefenderType::Cannon:
	case EDefenderType::Mortar:
	default:
		return FindNearestEnemyInRange();
	}
}

AEnemyUnit* ADefenderUnit::FindNearestEnemyInRange() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyUnit::StaticClass(), Found);

	AEnemyUnit* Best = nullptr;
	// slight grace so enemies stopped at mutual EngageStopFactor stay acquirable
	const float AcquireRange = AttackRange * 1.05f;
	float BestDistSq = AcquireRange * AcquireRange;
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

// furthest in range; tie-break toward higher current HP
AEnemyUnit* ADefenderUnit::FindMarksmanTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyUnit::StaticClass(), Found);

	AEnemyUnit* Best = nullptr;
	float BestDistSq = 0.f;
	float BestHealth = -1.f;
	const FVector Origin = GetActorLocation();
	const float MaxRangeSq = (AttackRange * 1.05f) * (AttackRange * 1.05f);

	for (AActor* Actor : Found)
	{
		AEnemyUnit* Enemy = Cast<AEnemyUnit>(Actor);
		if (!IsValid(Enemy) || Enemy->IsActorBeingDestroyed() || !Enemy->IsAlive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Origin, Enemy->GetActorLocation());
		if (DistSq > MaxRangeSq)
		{
			continue;
		}
		const float HP = Enemy->GetHealth();
		if (!Best || DistSq > BestDistSq + 1.f || (FMath::IsNearlyEqual(DistSq, BestDistSq, 100.f) && HP > BestHealth))
		{
			Best = Enemy;
			BestDistSq = DistSq;
			BestHealth = HP;
		}
	}
	return Best;
}

void ADefenderUnit::RefreshColor()
{
	if (!IsValid(Mesh))
	{
		return;
	}

	const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	const float HealthMul = FMath::Lerp(0.92f, 1.f, Ratio);
	const float VisualMul = HealthMul * UpgradeVisualScale;
	const FVector BaseVec = bUsingCannonMesh ? FVector(BaseMeshScale) : (FallbackScaleMul * BaseMeshScale);
	Mesh->SetRelativeScale3D(BaseVec * VisualMul);

	// grow upward from the pad instead of sinking the extra scale into the ground
	const float BaseZ = BaseVec.Z;
	const float NewZ = BaseZ * VisualMul;
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, MeshLocalMinZ * (BaseZ - NewZ)));
}

namespace DefenderUpgradeStats
{
	// percents here must match GetNextUpgradeHint
	constexpr float HealthMul = 1.30f;          // +30% max health, heal by the gain
	constexpr float CannonDamageMul = 1.22f;    // +22% damage
	constexpr float CannonCooldownMul = 0.95f;  // small fire-rate bump, damage is the cannon identity
	constexpr float MarksmanDamageMul = 1.10f;  // modest, fire rate is the marksman identity
	constexpr float MarksmanCooldownMul = 0.80f; // +25% fire rate (1/0.80)
	constexpr float MortarDamageMul = 1.15f;
	constexpr float MortarSplashDamageMul = 1.25f; // +25% splash
	constexpr float MortarSplashRadiusMul = 1.12f;
	constexpr float MortarCooldownMul = 0.95f;
}

FString ADefenderUnit::GetNextUpgradeHint() const
{
	if (UpgradeLevel >= MaxUpgradeLevel)
	{
		return FString();
	}

	switch (DefenderType)
	{
	case EDefenderType::Marksman:
		return TEXT("+30% health, +25% fire rate");
	case EDefenderType::Mortar:
		return TEXT("+30% health, +25% splash");
	case EDefenderType::Cannon:
	default:
		return TEXT("+30% health, +22% damage");
	}
}

bool ADefenderUnit::ApplyNextUpgrade()
{
	if (!IsAlive() || UpgradeLevel >= MaxUpgradeLevel)
	{
		return false;
	}

	const float OldMax = MaxHealth;
	MaxHealth *= DefenderUpgradeStats::HealthMul;
	const float Gained = MaxHealth - OldMax;
	Health = FMath::Min(MaxHealth, Health + Gained);

	switch (DefenderType)
	{
	case EDefenderType::Cannon:
		AttackDamage *= DefenderUpgradeStats::CannonDamageMul;
		AttackCooldown = FMath::Max(0.15f, AttackCooldown * DefenderUpgradeStats::CannonCooldownMul);
		break;
	case EDefenderType::Marksman:
		AttackDamage *= DefenderUpgradeStats::MarksmanDamageMul;
		AttackCooldown = FMath::Max(0.2f, AttackCooldown * DefenderUpgradeStats::MarksmanCooldownMul);
		break;
	case EDefenderType::Mortar:
		AttackDamage *= DefenderUpgradeStats::MortarDamageMul;
		SplashDamage *= DefenderUpgradeStats::MortarSplashDamageMul;
		SplashRadius *= DefenderUpgradeStats::MortarSplashRadiusMul;
		AttackCooldown = FMath::Max(0.2f, AttackCooldown * DefenderUpgradeStats::MortarCooldownMul);
		break;
	default:
		break;
	}

	++UpgradeLevel;
	UpgradeVisualScale = UpgradeVisual::ScaleForLevel(UpgradeLevel);
	RefreshColor();
	UpdateUpgradeAccent();

	UE_LOG(LogTemp, Log,
		TEXT("[PortalProtect] %s upgraded to Lv %d (HP %.0f/%.0f dmg %.1f cd %.2f splash %.1f r %.0f)"),
		DefenderType == EDefenderType::Marksman ? TEXT("Marksman")
			: (DefenderType == EDefenderType::Mortar ? TEXT("Mortar") : TEXT("Cannon")),
		UpgradeLevel, Health, MaxHealth, AttackDamage, AttackCooldown, SplashDamage, SplashRadius);
	return true;
}

void ADefenderUnit::EnsureUpgradeRing()
{
	if (!IsValid(UpgradeRing) || bUpgradeRingBuilt)
	{
		return;
	}

	UpgradeVisual::BuildFlatTorus(UpgradeRing);
	bUpgradeRingBuilt = true;
}

void ADefenderUnit::UpdateUpgradeAccent()
{
	EnsureUpgradeRing();
	if (!IsValid(UpgradeRing))
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
		// level 2 pushes the emissive harder so it doesn't just look like a recolor of level 1
		AccentMID->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (UpgradeLevel >= 2 ? 5.f : 2.5f));
		UpgradeRing->SetMaterial(0, AccentMID);
	}

	const float RadiusMul = (UpgradeLevel >= 2) ? 0.70f : 0.56f;
	const float Radius = FMath::Max(FootprintDiameter * RadiusMul, 72.f);
	const float Uniform = Radius / UpgradeVisual::TorusMajorRadius;
	UpgradeRing->SetRelativeScale3D(FVector(Uniform));
	UpgradeRing->SetRelativeLocation(FVector(0.f, 0.f, -PivotToGroundOffset + 12.f));
	UpgradeRing->SetHiddenInGame(false);
	UpgradeRing->SetVisibility(true);
}
