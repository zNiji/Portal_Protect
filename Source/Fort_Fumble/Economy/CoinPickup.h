// walk-over coin - adds to balance and destroys itself
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoinPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;

UENUM(BlueprintType)
enum class ECoinPickupType : uint8
{
	Small UMETA(DisplayName = "Small"),
	Big UMETA(DisplayName = "Big"),
};

UCLASS()
class FORT_FUMBLE_API ACoinPickup : public AActor
{
	GENERATED_BODY()

public:
	ACoinPickup();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Apply size + value for small vs big. Safe to call right after spawn. */
	UFUNCTION(BlueprintCallable, Category = "Coin")
	void ApplyCoinType(ECoinPickupType InType);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CoinMesh;

	UPROPERTY(EditAnywhere, Category = "Coin")
	ECoinPickupType CoinType = ECoinPickupType::Small;

	UPROPERTY(EditAnywhere, Category = "Coin")
	int32 CoinValue = 5;

	UPROPERTY(EditAnywhere, Category = "Coin")
	float SpinSpeed = 120.f;

	UPROPERTY(EditAnywhere, Category = "Coin")
	float BobAmplitude = 12.f;

	UPROPERTY(EditAnywhere, Category = "Coin")
	float BobSpeed = 2.5f;

protected:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	void ApplyVisualScale(float TargetDiameter);

	FVector BaseLocation = FVector::ZeroVector;
	float BobTime = 0.f;
	bool bCollected = false;
	bool bUsingStylizedCoin = false;
};
