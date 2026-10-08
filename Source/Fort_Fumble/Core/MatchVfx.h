// death puff and the tower-damage vignette
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UParticleSystem;
class UWorld;

namespace MatchVfx
{
	// null if the sprite failed to build. cached after the first call
	UParticleSystem* GetDeathBurst();

	// component scale. death sits higher than mortar so the kill puff is the loud one
	void SpawnDeathBurst(UWorld* World, const FVector& Location, float Scale = 1.f);

	// HealthAlpha 1 = healthy, 0 = empty. HitPulse 0-1 is the extra rim
	UMaterialInterface* GetTowerDamageVignette();
}
