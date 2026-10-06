// violet crystal + halo so a token doesn't read as another gold coin

#include "Economy/UpgradeTokenPickup.h"
#include "Core/UpgradeVisuals.h"
#include "Game/PortalProtectGameMode.h"
#include "Game/PortalProtectPawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AUpgradeTokenPickup::AUpgradeTokenPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(56.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);

	HaloMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HaloMesh"));
	HaloMesh->SetupAttachment(CollisionSphere);
	HaloMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HaloMesh->SetMobility(EComponentMobility::Movable);
	HaloMesh->SetCastShadow(false);

	CrystalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrystalMesh"));
	CrystalMesh->SetupAttachment(CollisionSphere);
	CrystalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CrystalMesh->SetMobility(EComponentMobility::Movable);
	CrystalMesh->SetCastShadow(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderAsset.Succeeded())
	{
		HaloMesh->SetStaticMesh(CylinderAsset.Object);
		// flat disc under the crystal — engine cylinder is 100uu tall
		HaloMesh->SetRelativeScale3D(FVector(0.72f, 0.72f, 0.035f));
		HaloMesh->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeAsset(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeAsset.Succeeded())
	{
		CrystalMesh->SetStaticMesh(ConeAsset.Object);
		CrystalMesh->SetRelativeScale3D(FVector(0.38f, 0.38f, 0.62f));
		CrystalMesh->SetRelativeLocation(FVector(0.f, 0.f, 36.f));
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereAsset.Succeeded())
		{
			CrystalMesh->SetStaticMesh(SphereAsset.Object);
			CrystalMesh->SetRelativeScale3D(FVector(0.35f));
			CrystalMesh->SetRelativeLocation(FVector(0.f, 0.f, 28.f));
		}
	}
}

void AUpgradeTokenPickup::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	BobTime = FMath::FRandRange(0.f, PI);

	// violet, not gold — coins already own the yellow read
	const FLinearColor TokenColor(0.62f, 0.22f, 1.f);
	if (UMaterialInstanceDynamic* MID = UpgradeVisual::MakeAccentMaterial(this, TokenColor))
	{
		if (CrystalMesh)
		{
			CrystalMesh->SetMaterial(0, MID);
		}
		if (HaloMesh)
		{
			HaloMesh->SetMaterial(0, MID);
		}
	}

	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AUpgradeTokenPickup::OnOverlapBegin);
}

void AUpgradeTokenPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bCollected)
	{
		return;
	}

	BobTime += DeltaTime * BobSpeed;
	const float Z = FMath::Sin(BobTime) * BobAmplitude;
	SetActorLocation(BaseLocation + FVector(0.f, 0.f, Z));
	AddActorWorldRotation(FRotator(0.f, SpinSpeed * DeltaTime, 0.f));
}

void AUpgradeTokenPickup::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bCollected || !OtherActor || !OtherActor->IsA(APortalProtectPawn::StaticClass()))
	{
		return;
	}

	APortalProtectGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APortalProtectGameMode>() : nullptr;
	if (!GM || GM->IsGameOver())
	{
		return;
	}

	bCollected = true;
	GM->AddUpgradeTokens(TokenAmount);
	Destroy();
}
