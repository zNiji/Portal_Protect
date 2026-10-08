// two gameplay materials: portal stone health colour, and a light tree sway
#pragma once

#include "CoreMinimal.h"

class UObject;
class UStaticMeshComponent;

namespace PortalShaders
{
	// opaque stone slots only. leave the pack's translucent swirl on its own slot
	bool ApplyPortalMaterials(UStaticMeshComponent* Mesh, UObject* Outer);

	// 1 = full health, 0 = empty. ignored on slots that are not our dynamic instances
	void SetPortalHealthAlpha(UStaticMeshComponent* Mesh, float HealthAlpha);

	// forest trees. no-op when the sway graph cannot be built
	void ApplyTreeSway(UStaticMeshComponent* Mesh);
}
