// first-person pawn - just movement and camera
// placement and pause live on the player controller
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PortalProtectPawn.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class ACentralTower;

UCLASS()
class FORT_FUMBLE_API APortalProtectPawn : public ACharacter
{
	GENERATED_BODY()

public:
	APortalProtectPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float LookSensitivity = 1.f;

protected:
	void MoveForward(float Value);
	void MoveRight(float Value);
	void LookYaw(float Value);
	void LookPitch(float Value);
	void UpdateTowerVignette(float DeltaTime);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TowerVignetteMID;

	TWeakObjectPtr<ACentralTower> CachedTower;
	float LastTowerHealth = -1.f;
	float VignettePulse = 0.f;
};
