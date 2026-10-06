// rare walk-over upgrade token — adds to the token inventory, not the coin balance
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UpgradeTokenPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class FORT_FUMBLE_API AUpgradeTokenPickup : public AActor
{
	GENERATED_BODY()

public:
	AUpgradeTokenPickup();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CrystalMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HaloMesh;

	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	int32 TokenAmount = 1;

	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	float SpinSpeed = 140.f;

	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	float BobAmplitude = 16.f;

	UPROPERTY(EditAnywhere, Category = "UpgradeToken")
	float BobSpeed = 2.2f;

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	FVector BaseLocation = FVector::ZeroVector;
	float BobTime = 0.f;
	bool bCollected = false;
};
