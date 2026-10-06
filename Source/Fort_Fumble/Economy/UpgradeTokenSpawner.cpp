// initial token + slow respawn, never mixed into the coin cap

#include "Economy/UpgradeTokenSpawner.h"
#include "Economy/UpgradeTokenPickup.h"
#include "Terrain/ProceduralTerrainActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AUpgradeTokenSpawner::AUpgradeTokenSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	TokenClass = AUpgradeTokenPickup::StaticClass();
}

void AUpgradeTokenSpawner::BeginPlay()
{
	Super::BeginPlay();
	RespawnTimer = RespawnInterval;
}

void AUpgradeTokenSpawner::Configure(AProceduralTerrainActor* InTerrain)
{
	Terrain = InTerrain;
	if (!Terrain || !TokenClass)
	{
		return;
	}

	const int32 ToSpawn = FMath::Min(InitialTokenCount, MaxActiveTokens);
	for (int32 I = 0; I < ToSpawn; ++I)
	{
		SpawnOneToken();
	}
}

void AUpgradeTokenSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Terrain || !TokenClass)
	{
		return;
	}

	RespawnTimer -= DeltaTime;
	if (RespawnTimer <= 0.f)
	{
		RespawnTimer = RespawnInterval;
		if (CountActiveTokens() < MaxActiveTokens)
		{
			SpawnOneToken();
		}
	}
}

void AUpgradeTokenSpawner::SpawnOneToken()
{
	if (!Terrain || !GetWorld() || !TokenClass)
	{
		return;
	}

	if (CountActiveTokens() >= MaxActiveTokens)
	{
		return;
	}

	FVector Loc = FVector::ZeroVector;
	bool bFound = false;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		if (Terrain->TryGetRandomOffPathLocation(Loc, 48.f))
		{
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AUpgradeTokenPickup* Token = GetWorld()->SpawnActor<AUpgradeTokenPickup>(
		TokenClass, Loc, FRotator::ZeroRotator, Params);
	if (Token)
	{
		UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Upgrade token spawned."));
	}
}

int32 AUpgradeTokenSpawner::CountActiveTokens() const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AUpgradeTokenPickup::StaticClass(), Found);
	return Found.Num();
}
