// pathing, combat, type setup, idle/walk anims, HP bar

#include "Enemy/EnemyUnit.h"
#include "Enemy/EnemyProjectile.h"
#include "Enemy/EnemyHealthBarWidget.h"
#include "Game/PortalProtectGameMode.h"
#include "Tower/CentralTower.h"
#include "Defender/DefenderUnit.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AEnemyUnit::AEnemyUnit()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_Pawn);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetSphereRadius(40.f);
	Collision->SetCanEverAffectNavigation(false);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// pack meshes face +X-ish - rel yaw +90 then MeshYawOffset so they don't moonwalk
	Mesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(Collision);
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(false);
	HealthBar->SetDrawSize(FVector2D(90.f, 12.f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.f));
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBar->SetWidgetClass(UEnemyHealthBarWidget::StaticClass());

	// default constructor load = slime so BP/default spawn still looks ok before InitializeAsType
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SoftSlimeMesh(
		TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Slime_SK.Slime_SK"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SoftSlimeIdle(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_IdleNormal_ANIM.Slime_IdleNormal_ANIM"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> SoftSlimeWalk(
		TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Walk_ANIM.Slime_Walk_ANIM"));

	if (SoftSlimeMesh.Succeeded())
	{
		ApplyMeshSetup(SoftSlimeMesh.Object,
			SoftSlimeIdle.Succeeded() ? SoftSlimeIdle.Object : nullptr,
			SoftSlimeWalk.Succeeded() ? SoftSlimeWalk.Object : nullptr);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("[PortalProtect] Failed to load Slime_SK — enemy will have no skeletal mesh until the MonsterForSurvivalGame pack is present."));
		Collision->SetSphereRadius(35.f);
		bUsingMonsterMesh = false;
	}

	Health = MaxHealth;
}

void AEnemyUnit::BeginPlay()
{
	Super::BeginPlay();
	if (!bTypeConfigured)
	{
		InitializeAsType(EnemyType);
	}
	Health = MaxHealth;
	RefreshDamageVisual();
	UpdateLocomotionAnim(false);
	UpdateHealthBar();
}

void AEnemyUnit::InitializeAsType(EEnemyType InType)
{
	EnemyType = InType;
	bTypeConfigured = true;
	CurrentAnim = nullptr;

	USkeletalMesh* NewMesh = nullptr;
	UAnimSequence* NewIdle = nullptr;
	UAnimSequence* NewWalk = nullptr;

	switch (EnemyType)
	{
	case EEnemyType::Runner:
	{
		// cactus - skinny and fast, short melee only
		NewMesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Cactus_SK.Cactus_SK"));
		NewIdle = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/Cactus_IdleNormal_ANIM.Cactus_IdleNormal_ANIM"));
		NewWalk = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Cactus/Cactus_RunFWD_ANIM.Cactus_RunFWD_ANIM"));
		MaxHealth = 38.f;
		MoveSpeed = 390.f;
		AttackDamage = 7.f;
		AttackRange = 140.f;
		AttackCooldown = 0.65f;
		DefenderAggroRange = 220.f;
		TowerAttackRange = 180.f;
		EngageStopFactor = 0.95f;
		bUsesProjectile = false;
		ProjectileSpeed = 0.f;
		TargetHeight = 72.f;
		MeshYawOffset = 180.f;
		MeshTint = FLinearColor(0.55f, 1.f, 0.35f);
		KillScore = 25;
		break;
	}
	case EEnemyType::Tank:
	{
		// chest monster - fat HP sponge, chunky projectiles
		NewMesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/ChestMonster_SK.ChestMonster_SK"));
		NewIdle = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/ChestMonster/ChestMonster_IdleNormal_ANIM.ChestMonster_IdleNormal_ANIM"));
		NewWalk = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/ChestMonster/ChestMonster_WalkFWD_ANIM.ChestMonster_WalkFWD_ANIM"));
		MaxHealth = 220.f;
		MoveSpeed = 115.f;
		AttackDamage = 20.f;
		AttackRange = 820.f;
		AttackCooldown = 1.55f;
		DefenderAggroRange = 820.f;
		TowerAttackRange = 920.f;
		EngageStopFactor = 0.88f;
		bUsesProjectile = true;
		ProjectileSpeed = 680.f;
		TargetHeight = 135.f;
		MeshYawOffset = 180.f;
		MeshTint = FLinearColor(1.f, 0.45f, 0.35f);
		KillScore = 70;
		break;
	}
	case EEnemyType::Slime:
	default:
	{
		NewMesh = LoadObject<USkeletalMesh>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Mesh/PBR/Slime_SK.Slime_SK"));
		NewIdle = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_IdleNormal_ANIM.Slime_IdleNormal_ANIM"));
		NewWalk = LoadObject<UAnimSequence>(nullptr,
			TEXT("/Game/MonsterForSurvivalGame/Animation/PBR/Slime/Slime_Walk_ANIM.Slime_Walk_ANIM"));
		MaxHealth = 70.f;
		MoveSpeed = 220.f;
		AttackDamage = 11.f;
		AttackRange = 750.f;
		AttackCooldown = 1.05f;
		DefenderAggroRange = 750.f;
		TowerAttackRange = 850.f;
		EngageStopFactor = 0.9f;
		bUsesProjectile = true;
		ProjectileSpeed = 900.f;
		TargetHeight = 90.f;
		MeshYawOffset = 180.f;
		MeshTint = FLinearColor(0.45f, 0.85f, 1.f);
		KillScore = 40;
		break;
	}
	}

	if (NewMesh)
	{
		ApplyMeshSetup(NewMesh, NewIdle, NewWalk);
		ApplyTint();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] InitializeAsType failed to load mesh for type %d"),
			static_cast<int32>(EnemyType));
	}

	Health = MaxHealth;
	UpdateHealthBar();
	UpdateLocomotionAnim(false);

	UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Enemy type %d ready (HP=%.0f Speed=%.0f Projectile=%d)"),
		static_cast<int32>(EnemyType), MaxHealth, MoveSpeed, bUsesProjectile ? 1 : 0);
}

void AEnemyUnit::ApplyLateWaveHealthScale(int32 WaveNumber)
{
	// leave the early waves alone — upgrade tokens are rare, so pressure starts once they can stack
	if (WaveNumber < 5 || MaxHealth <= 0.f)
	{
		return;
	}

	const float HpMul = FMath::Clamp(1.f + 0.06f * static_cast<float>(WaveNumber - 4), 1.f, 1.36f);
	MaxHealth *= HpMul;
	Health = MaxHealth;
	UpdateHealthBar();
}

void AEnemyUnit::ApplyMeshSetup(USkeletalMesh* InMesh, UAnimSequence* InIdle, UAnimSequence* InWalk)
{
	if (!InMesh || !Mesh)
	{
		return;
	}

	Mesh->SetSkeletalMesh(InMesh);
	IdleAnim = InIdle;
	WalkAnim = InWalk;

	const FBoxSphereBounds Bounds = InMesh->GetBounds();
	const float MeshHeight = FMath::Max(Bounds.BoxExtent.Z * 2.f, 1.f);
	BaseMeshScale = FMath::Clamp(TargetHeight / MeshHeight, 0.12f, 3.f);
	MeshScale = BaseMeshScale;
	Mesh->SetRelativeScale3D(FVector(BaseMeshScale));

	// lift mesh so feet sit on path surface
	const float BottomZ = Bounds.Origin.Z - Bounds.BoxExtent.Z;
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -BottomZ * BaseMeshScale));

	const float Radius = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * BaseMeshScale * 0.85f;
	const float RadiusClampMax = (EnemyType == EEnemyType::Tank) ? 70.f : 55.f;
	Collision->SetSphereRadius(FMath::Clamp(Radius, 24.f, RadiusClampMax));
	bUsingMonsterMesh = true;

	// sit HP bar above the scaled head
	if (HealthBar)
	{
		const float BarZ = MeshHeight * BaseMeshScale * 0.55f + 35.f;
		HealthBar->SetRelativeLocation(FVector(0.f, 0.f, BarZ));
	}
}

void AEnemyUnit::ApplyTint()
{
	if (!Mesh)
	{
		return;
	}

	const int32 MatCount = Mesh->GetNumMaterials();
	for (int32 i = 0; i < MatCount; ++i)
	{
		if (UMaterialInstanceDynamic* Mid = Mesh->CreateAndSetMaterialInstanceDynamic(i))
		{
			// pack mats vary - try a few common param names, no big deal if ignored
			Mid->SetVectorParameterValue(TEXT("Color"), MeshTint);
			Mid->SetVectorParameterValue(TEXT("BaseColor"), MeshTint);
			Mid->SetVectorParameterValue(TEXT("Tint"), MeshTint);
		}
	}
}

// spawner passes waypoint list, snap to first path cell
void AEnemyUnit::InitializeOnPath(const TArray<FVector>& InWaypoints)
{
	Waypoints = InWaypoints;
	WaypointIndex = 0;
	bInitialized = Waypoints.Num() > 0;
	if (bInitialized)
	{
		// keep collision center on path surface
		const float GroundZ = Waypoints[0].Z;
		const float Radius = Collision->GetScaledSphereRadius();
		SetActorLocation(FVector(Waypoints[0].X, Waypoints[0].Y, GroundZ + Radius));
	}
}

void AEnemyUnit::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsAlive() || !bInitialized)
	{
		return;
	}

	TryAttackEnemyTargets(DeltaTime);
	MoveAlongPath(DeltaTime);
}

void AEnemyUnit::ApplyDamage(float Amount)
{
	if (!IsValid(this) || IsActorBeingDestroyed() || !IsAlive() || Amount <= 0.f)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	RefreshDamageVisual();
	UpdateHealthBar();
	if (Health <= 0.f)
	{
		if (IsValid(HealthBar))
		{
			HealthBar->SetVisibility(false);
			if (UEnemyHealthBarWidget* Bar = Cast<UEnemyHealthBarWidget>(HealthBar->GetUserWidgetObject()))
			{
				Bar->SetBarVisible(false);
			}
		}

		// pay out kill points before we vanish (type-specific score)
		if (UWorld* World = GetWorld())
		{
			if (APortalProtectGameMode* GM = World->GetAuthGameMode<APortalProtectGameMode>())
			{
				GM->AddScore(KillScore);
			}
		}
		Destroy();
	}
}

void AEnemyUnit::UpdateHealthBar()
{
	if (!HealthBar)
	{
		return;
	}

	UEnemyHealthBarWidget* Bar = Cast<UEnemyHealthBarWidget>(HealthBar->GetUserWidgetObject());
	if (!Bar)
	{
		// widget may not exist until after BeginPlay init
		HealthBar->InitWidget();
		Bar = Cast<UEnemyHealthBarWidget>(HealthBar->GetUserWidgetObject());
	}
	if (Bar)
	{
		const float Pct = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
		Bar->SetHealthPercent(Pct);
		Bar->SetBarVisible(IsAlive());
	}
}

// step toward current waypoint, pause when bCombatEngaged
void AEnemyUnit::MoveAlongPath(float DeltaTime)
{
	// only stop when inside engage range, not just aggro range
	if (bCombatEngaged)
	{
		return;
	}

	if (WaypointIndex >= Waypoints.Num())
	{
		// reached end of path - face tower and shoot from TryAttackEnemyTargets
		if (ACentralTower* Tower = FindTower())
		{
			UpdateFacing(Tower->GetActorLocation() - GetActorLocation(), DeltaTime);
		}
		UpdateLocomotionAnim(false);
		return;
	}

	const FVector Target = Waypoints[WaypointIndex];
	const FVector Current = GetActorLocation();
	const float Radius = Collision->GetScaledSphereRadius();
	const FVector TargetCenter(Target.X, Target.Y, Target.Z + Radius);
	const FVector Delta = TargetCenter - Current;
	const float Dist = Delta.Size();
	const float Step = MoveSpeed * DeltaTime;

	if (Dist > 5.f)
	{
		UpdateFacing(Delta, DeltaTime);
	}

	if (Dist <= Step || Dist < 5.f)
	{
		SetActorLocation(TargetCenter);
		++WaypointIndex;
		UpdateLocomotionAnim(WaypointIndex < Waypoints.Num());
	}
	else
	{
		SetActorLocation(Current + Delta.GetSafeNormal() * Step);
		UpdateLocomotionAnim(true);
	}
}

// pick defender or tower in range, spawn projectile / melee on cooldown
bool AEnemyUnit::TryAttackEnemyTargets(float DeltaTime)
{
	bCombatEngaged = false;
	AttackTimer -= DeltaTime;

	const bool bAtPathEnd = WaypointIndex >= Waypoints.Num();
	const FVector Origin = GetActorLocation();

	// notice defenders out to aggro, but only fire/stop inside mutual reach so they can shoot back
	const float SelectRange = FMath::Max(AttackRange, DefenderAggroRange);
	ADefenderUnit* Defender = FindNearbyDefender(SelectRange);
	ACentralTower* Tower = FindTower();
	const float EffectiveTowerRange = bAtPathEnd ? FMath::Max(AttackRange, TowerAttackRange) : AttackRange;

	float DefenderDistSq = TNumericLimits<float>::Max();
	float MutualDefenderRange = AttackRange;
	if (IsValid(Defender) && !Defender->IsActorBeingDestroyed() && Defender->IsAlive())
	{
		DefenderDistSq = FVector::DistSquared2D(Origin, Defender->GetActorLocation());
		MutualDefenderRange = FMath::Min(AttackRange, Defender->GetAttackRange());
	}
	else
	{
		Defender = nullptr;
	}
	const bool bDefenderInFireRange = Defender && DefenderDistSq <= MutualDefenderRange * MutualDefenderRange;

	float TowerDistSq = TNumericLimits<float>::Max();
	float MutualTowerRange = EffectiveTowerRange;
	const bool bTowerAlive = IsValid(Tower) && !Tower->IsActorBeingDestroyed() && Tower->IsAlive();
	if (!bTowerAlive)
	{
		Tower = nullptr;
	}
	if (bTowerAlive)
	{
		TowerDistSq = FVector::DistSquared2D(Origin, Tower->GetActorLocation());
		MutualTowerRange = FMath::Min(EffectiveTowerRange, Tower->GetAttackRange());
	}
	const bool bTowerInFireRange = bTowerAlive && TowerDistSq <= MutualTowerRange * MutualTowerRange;

	if (!bDefenderInFireRange && !bTowerInFireRange)
	{
		return false;
	}

	// stop inside the target's reach (not out at our longer projectile range)
	const float StopRange = MutualDefenderRange * EngageStopFactor;
	const float TowerStopRange = MutualTowerRange * EngageStopFactor;
	const bool bCloseEnoughToStop =
		(bDefenderInFireRange && DefenderDistSq <= StopRange * StopRange)
		|| (bTowerInFireRange && TowerDistSq <= TowerStopRange * TowerStopRange);

	if (bCloseEnoughToStop)
	{
		bCombatEngaged = true;
		UpdateLocomotionAnim(false);
	}

	AActor* TargetActor = nullptr;
	FVector FaceDir = FVector::ZeroVector;
	if (bDefenderInFireRange)
	{
		TargetActor = Defender;
		FaceDir = Defender->GetActorLocation() - Origin;
	}
	else if (bTowerInFireRange)
	{
		TargetActor = Tower;
		FaceDir = Tower->GetActorLocation() - Origin;
	}
	UpdateFacing(FaceDir, DeltaTime);

	if (AttackTimer > 0.f || !TargetActor)
	{
		return true;
	}

	if (bUsesProjectile)
	{
		FireProjectileAt(TargetActor);
	}
	else
	{
		// runner melee - only land the hit when close (use mutual reach vs defenders)
		float MeleeReach = AttackRange * 1.05f;
		if (ADefenderUnit* DefTarget = Cast<ADefenderUnit>(TargetActor))
		{
			MeleeReach = FMath::Min(AttackRange, DefTarget->GetAttackRange()) * 1.05f;
		}
		if (FVector::DistSquared2D(Origin, TargetActor->GetActorLocation()) <= MeleeReach * MeleeReach)
		{
			MeleeHit(TargetActor);
		}
	}
	AttackTimer = AttackCooldown;
	return true;
}

void AEnemyUnit::FireProjectileAt(AActor* Target)
{
	if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !GetWorld())
	{
		return;
	}

	const FVector Muzzle = GetActorLocation() + FVector(0.f, 0.f, 35.f) + GetActorForwardVector() * 30.f;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = nullptr;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemyProjectile* Shot = GetWorld()->SpawnActor<AEnemyProjectile>(
		AEnemyProjectile::StaticClass(), Muzzle, GetActorRotation(), Params);
	if (IsValid(Shot))
	{
		Shot->InitProjectile(Target, AttackDamage, ProjectileSpeed, this);
	}

	const FColor LineColor = (EnemyType == EEnemyType::Tank) ? FColor::Red : FColor::Orange;
	DrawDebugLine(GetWorld(), Muzzle, Target->GetActorLocation() + FVector(0.f, 0.f, 40.f),
		LineColor, false, 0.1f, 0, EnemyType == EEnemyType::Tank ? 3.5f : 2.f);
}

void AEnemyUnit::MeleeHit(AActor* Target)
{
	if (!IsValid(Target) || Target->IsActorBeingDestroyed())
	{
		return;
	}

	if (ADefenderUnit* Defender = Cast<ADefenderUnit>(Target))
	{
		if (IsValid(Defender) && Defender->IsAlive())
		{
			Defender->ApplyDamage(AttackDamage);
		}
	}
	else if (ACentralTower* Tower = Cast<ACentralTower>(Target))
	{
		if (IsValid(Tower) && Tower->IsAlive())
		{
			Tower->ApplyDamage(AttackDamage);
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (IsValid(Target))
		{
			DrawDebugLine(World, GetActorLocation() + FVector(0.f, 0.f, 30.f),
				Target->GetActorLocation() + FVector(0.f, 0.f, 40.f),
				FColor::Yellow, false, 0.12f, 0, 2.5f);
		}
	}
}

// yaw toward move/combat dir, MeshYawOffset fixes import facing
void AEnemyUnit::UpdateFacing(const FVector& WorldDirection, float DeltaTime)
{
	FVector Flat = WorldDirection;
	Flat.Z = 0.f;
	if (Flat.SizeSquared() < 1.f)
	{
		return;
	}

	FRotator Desired = Flat.Rotation();
	Desired.Yaw += MeshYawOffset;
	const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), Desired, DeltaTime, TurnInterpSpeed);
	SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));
}

void AEnemyUnit::UpdateLocomotionAnim(bool bMoving)
{
	if (!IsValid(Mesh))
	{
		return;
	}

	UAnimSequence* Desired = bMoving ? WalkAnim.Get() : IdleAnim.Get();
	if (!Desired || Desired == CurrentAnim)
	{
		return;
	}

	Mesh->PlayAnimation(Desired, true);
	CurrentAnim = Desired;
}

ADefenderUnit* AEnemyUnit::FindNearbyDefender(float Range) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(World, ADefenderUnit::StaticClass(), Found);

	ADefenderUnit* Best = nullptr;
	float BestDistSq = Range * Range;
	const FVector Origin = GetActorLocation();

	for (AActor* Actor : Found)
	{
		ADefenderUnit* Defender = Cast<ADefenderUnit>(Actor);
		if (!IsValid(Defender) || Defender->IsActorBeingDestroyed() || !Defender->IsAlive())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(Origin, Defender->GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Defender;
		}
	}
	return Best;
}

ACentralTower* AEnemyUnit::FindTower() const
{
	return Cast<ACentralTower>(UGameplayStatics::GetActorOfClass(GetWorld(), ACentralTower::StaticClass()));
}

void AEnemyUnit::RefreshDamageVisual()
{
	if (!IsValid(Mesh))
	{
		return;
	}

	// damage feedback = slight squash, keep pack materials
	const float Ratio = MaxHealth > 0.f ? Health / MaxHealth : 0.f;
	const float ScaleMul = FMath::Lerp(0.88f, 1.f, Ratio);
	if (bUsingMonsterMesh)
	{
		Mesh->SetRelativeScale3D(FVector(BaseMeshScale * ScaleMul));
	}
}
