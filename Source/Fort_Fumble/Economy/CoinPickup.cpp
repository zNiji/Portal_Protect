// coin mesh setup, texture fix, bob/spin, collection

#include "Economy/CoinPickup.h"
#include "Game/PortalProtectGameMode.h"
#include "Game/PortalProtectPawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace CoinPickupVisual
{
	// sketchfab coins can have multiple material slots
	static void ApplyMaterialToAllSlots(UStaticMeshComponent* Mesh, UMaterialInterface* Mat)
	{
		if (!Mesh || !Mat)
		{
			return;
		}
		const int32 NumSlots = FMath::Max(Mesh->GetNumMaterials(), 1);
		for (int32 Slot = 0; Slot < NumSlots; ++Slot)
		{
			Mesh->SetMaterial(Slot, Mat);
		}
	}

	// interchange import left diffuse map disconnected - wire Coin2_BaseColor into phong params
	// falls back to forest MI_DefaultPBR if pack parent missing
	static bool TryApplyTexturedCoinMaterial(UStaticMeshComponent* Mesh, UObject* Outer)
	{
		UTexture2D* CoinTex = LoadObject<UTexture2D>(
			nullptr, TEXT("/Game/stylized-coin/textures/Coin2_BaseColor.Coin2_BaseColor"));
		if (!CoinTex)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[PortalProtect] Failed to load Coin2_BaseColor texture."));
			return false;
		}

		// try pack MIC parent first - texture param is DiffuseColorMap on phong
		UMaterialInterface* ParentMat = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/stylized-coin/source/Material.Material"));
		if (!ParentMat)
		{
			ParentMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/RPGTinyFantasyForest/Material/MI_DefaultPBR.MI_DefaultPBR"));
		}
		if (!ParentMat)
		{
			ParentMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Game/RPGTinyFantasyForest/Material/BaseMAT/M_DefaultPBR.M_DefaultPBR"));
		}
		if (!ParentMat)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[PortalProtect] No parent material for textured coin MID."));
			return false;
		}

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ParentMat, Outer);
		if (!MID)
		{
			return false;
		}

		// set every likely texture param name, unused ones are no-ops
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
			MID->SetTextureParameterValue(Param, CoinTex);
		}

		// full weight on texture maps
		static const FName WeightParams[] = {
			TEXT("DiffuseColorMapWeight"),
			TEXT("AmbientColorMapWeight"),
		};
		for (const FName& Param : WeightParams)
		{
			MID->SetScalarParameterValue(Param, 1.f);
		}

		// white diffuse so texture isn't darkened
		MID->SetVectorParameterValue(TEXT("DiffuseColor"), FLinearColor::White);
		MID->SetVectorParameterValue(TEXT("AmbientColor"), FLinearColor(0.05f, 0.05f, 0.05f));

		ApplyMaterialToAllSlots(Mesh, MID);
		return true;
	}
}

ACoinPickup::ACoinPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	SetRootComponent(CollisionSphere);
	CollisionSphere->InitSphereRadius(48.f);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);

	CoinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoinMesh"));
	CoinMesh->SetupAttachment(CollisionSphere);
	CoinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoinMesh->SetMobility(EComponentMobility::Movable);
	CoinMesh->SetCastShadow(true);
	CoinMesh->bCastDynamicShadow = true;

	// stylized coin mesh from content/stylized-coin
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CoinAsset(
		TEXT("/Game/stylized-coin/source/Coin.Coin"));
	if (CoinAsset.Succeeded())
	{
		CoinMesh->SetStaticMesh(CoinAsset.Object);
		bUsingStylizedCoin = true;
		ApplyVisualScale(55.f);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[PortalProtect] Failed to load stylized Coin mesh — falling back to BasicShapes Sphere."));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereAsset.Succeeded())
		{
			CoinMesh->SetStaticMesh(SphereAsset.Object);
			CoinMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.2f));
		}
		bUsingStylizedCoin = false;
	}
}

void ACoinPickup::ApplyVisualScale(float TargetDiameter)
{
	if (!CoinMesh)
	{
		return;
	}

	UStaticMesh* Mesh = CoinMesh->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const float MeshDiameter = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.f;
	const float Scale = FMath::Clamp(TargetDiameter / FMath::Max(MeshDiameter, 1.f), 0.05f, 8.f);
	CoinMesh->SetRelativeScale3D(FVector(Scale));

	const float HalfHeight = Bounds.BoxExtent.Z * Scale;
	CoinMesh->SetRelativeLocation(FVector(0.f, 0.f, HalfHeight * 0.15f));

	const float OverlapMin = (TargetDiameter <= 60.f) ? 40.f : 48.f;
	const float OverlapMax = (TargetDiameter <= 60.f) ? 70.f : 90.f;
	const float OverlapRadius = FMath::Clamp(
		FMath::Max(Bounds.SphereRadius * Scale * 1.15f, TargetDiameter * 0.55f),
		OverlapMin, OverlapMax);
	CollisionSphere->InitSphereRadius(OverlapRadius);
}

void ACoinPickup::ApplyCoinType(ECoinPickupType InType)
{
	CoinType = InType;
	if (InType == ECoinPickupType::Big)
	{
		CoinValue = 10;
		if (bUsingStylizedCoin)
		{
			// slightly larger than the 55uu small coin
			ApplyVisualScale(72.f);
		}
		else if (CoinMesh)
		{
			CoinMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.28f));
			CollisionSphere->InitSphereRadius(56.f);
		}
	}
	else
	{
		CoinValue = 5;
		if (bUsingStylizedCoin)
		{
			ApplyVisualScale(55.f);
		}
		else if (CoinMesh)
		{
			CoinMesh->SetRelativeScale3D(FVector(0.45f, 0.45f, 0.2f));
			CollisionSphere->InitSphereRadius(48.f);
		}
	}
}

void ACoinPickup::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	BobTime = FMath::FRandRange(0.f, PI);

	if (bUsingStylizedCoin)
	{
		if (!CoinPickupVisual::TryApplyTexturedCoinMaterial(CoinMesh, this))
		{
			// gold tint fallback if texture wiring fails
			if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(
					nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
				MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.85f, 0.15f));
				CoinPickupVisual::ApplyMaterialToAllSlots(CoinMesh, MID);
			}
		}
	}
	else
	{
		if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.85f, 0.15f));
			CoinMesh->SetMaterial(0, MID);
		}
	}

	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &ACoinPickup::OnOverlapBegin);
}

void ACoinPickup::Tick(float DeltaTime)
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

// only FPS pawn collects, credits game mode, destroys pickup
void ACoinPickup::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
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
	GM->AddCoins(CoinValue);
	Destroy();
}
