// first-person movement and look - walk the map, collect coins, aim at pads

#include "Game/PortalProtectPawn.h"
#include "Core/MatchVfx.h"
#include "Tower/CentralTower.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

APortalProtectPawn::APortalProtectPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->JumpZVelocity = 420.f;
	Move->AirControl = 0.25f;
	Move->MaxWalkSpeed = 600.f;
	Move->MinAnalogWalkSpeed = 20.f;
	Move->BrakingDecelerationWalking = 2000.f;
	Move->SetWalkableFloorAngle(50.f);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // eye height
	FirstPersonCamera->bUsePawnControlRotation = true;
}

void APortalProtectPawn::BeginPlay()
{
	Super::BeginPlay();

	// compile the puff once so the first kill is not also the first shader build
	MatchVfx::GetDeathBurst();

	if (UMaterialInterface* Parent = MatchVfx::GetTowerDamageVignette())
	{
		TowerVignetteMID = UMaterialInstanceDynamic::Create(Parent, this);
		if (TowerVignetteMID && FirstPersonCamera)
		{
			TowerVignetteMID->SetScalarParameterValue(TEXT("HealthAlpha"), 1.f);
			TowerVignetteMID->SetScalarParameterValue(TEXT("HitPulse"), 0.f);
			FirstPersonCamera->PostProcessSettings.AddBlendable(TowerVignetteMID, 0.f);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] Tower damage vignette missing. Camera stays clean."));
	}
}

void APortalProtectPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateTowerVignette(DeltaTime);
}

// blend weight follows missing tower health. a hit adds a short extra rim
void APortalProtectPawn::UpdateTowerVignette(float DeltaTime)
{
	if (!TowerVignetteMID || !FirstPersonCamera)
	{
		return;
	}

	ACentralTower* Tower = CachedTower.Get();
	if (!IsValid(Tower))
	{
		CachedTower = nullptr;
		if (UWorld* World = GetWorld())
		{
			TArray<AActor*> Found;
			UGameplayStatics::GetAllActorsOfClass(World, ACentralTower::StaticClass(), Found);
			if (Found.Num() > 0)
			{
				Tower = Cast<ACentralTower>(Found[0]);
				CachedTower = Tower;
			}
		}
	}

	float HealthAlpha = 1.f;
	if (IsValid(Tower) && Tower->GetMaxHealth() > 0.f)
	{
		const float Health = Tower->GetHealth();
		HealthAlpha = FMath::Clamp(Health / Tower->GetMaxHealth(), 0.f, 1.f);
		if (LastTowerHealth >= 0.f && Health < LastTowerHealth - 0.5f)
		{
			VignettePulse = 1.f;
		}
		LastTowerHealth = Health;
	}
	else
	{
		LastTowerHealth = -1.f;
		VignettePulse = 0.f;
	}

	VignettePulse = FMath::Max(0.f, VignettePulse - DeltaTime / 0.35f);

	// quiet until the portal has actually lost health, then climb. full weight would hide the view
	const float Damage = 1.f - HealthAlpha;
	const float Shaped = FMath::SmoothStep(0.08f, 0.85f, Damage);
	const float Weight = FMath::Clamp(Shaped * 0.92f + VignettePulse * 0.42f, 0.f, 1.f);

	TowerVignetteMID->SetScalarParameterValue(TEXT("HealthAlpha"), HealthAlpha);
	TowerVignetteMID->SetScalarParameterValue(TEXT("HitPulse"), VignettePulse);
	FirstPersonCamera->PostProcessSettings.AddBlendable(TowerVignetteMID, Weight);
}

// WASD, mouse look, jump
void APortalProtectPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	// WASD are digital keys - use axis mappings from DefaultInput.ini
	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &APortalProtectPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &APortalProtectPawn::MoveRight);
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &APortalProtectPawn::LookYaw);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &APortalProtectPawn::LookPitch);

	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ACharacter::StopJumping);
}

void APortalProtectPawn::MoveForward(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void APortalProtectPawn::MoveRight(float Value)
{
	if (Controller && !FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void APortalProtectPawn::LookYaw(float Value)
{
	AddControllerYawInput(Value * LookSensitivity);
}

void APortalProtectPawn::LookPitch(float Value)
{
	AddControllerPitchInput(-Value * LookSensitivity);
}
