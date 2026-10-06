// shared upgrade look — scale steps plus a flat emissive ring (no new art)
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace UpgradeVisual
{
	// level 0 stays at 1. these are absolute (not stacked on each other)
	constexpr float ScaleLevel1 = 1.12f;
	constexpr float ScaleLevel2 = 1.24f;

	// torus is built at this major radius, then the component is scaled
	constexpr float TorusMajorRadius = 100.f;
	constexpr float TorusTubeRadius = 9.f;

	inline float ScaleForLevel(int32 Level)
	{
		if (Level >= 2)
		{
			return ScaleLevel2;
		}
		if (Level == 1)
		{
			return ScaleLevel1;
		}
		return 1.f;
	}

	// level 1 warm gold, level 2 cooler cyan-violet so the two steps read apart
	inline FLinearColor AccentForLevel(int32 Level)
	{
		if (Level >= 2)
		{
			return FLinearColor(0.38f, 0.48f, 1.f);
		}
		return FLinearColor(1.f, 0.74f, 0.12f);
	}

	inline UMaterialInstanceDynamic* MakeAccentMaterial(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Parent = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
		if (!Parent)
		{
			Parent = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		if (!Parent || !Outer)
		{
			return nullptr;
		}

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, Outer);
		if (!MID)
		{
			return nullptr;
		}

		// Color is the vector param on EmissiveMeshMaterial; BasicShapeMaterial uses it too
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetVectorParameterValue(TEXT("EmissiveColor"), Color * 3.f);
		return MID;
	}

	// unit torus in the XY plane. both windings so a single-sided emissive mat still shows
	inline void BuildFlatTorus(UProceduralMeshComponent* Ring)
	{
		if (!Ring)
		{
			return;
		}

		constexpr int32 Segments = 24;
		constexpr int32 Sides = 8;

		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		TArray<int32> Triangles;

		Vertices.Reserve((Segments + 1) * (Sides + 1));
		const FLinearColor White = FLinearColor::White;

		for (int32 Seg = 0; Seg <= Segments; ++Seg)
		{
			const float U = (static_cast<float>(Seg) / Segments) * 2.f * PI;
			const FVector RingDir(FMath::Cos(U), FMath::Sin(U), 0.f);
			const FVector Center = RingDir * TorusMajorRadius;
			const FVector Side = FVector::CrossProduct(FVector::UpVector, RingDir);

			for (int32 SideIndex = 0; SideIndex <= Sides; ++SideIndex)
			{
				const float V = (static_cast<float>(SideIndex) / Sides) * 2.f * PI;
				const FVector Normal = (RingDir * FMath::Cos(V) + FVector::UpVector * FMath::Sin(V)).GetSafeNormal();
				Vertices.Add(Center + Normal * TorusTubeRadius);
				Normals.Add(Normal);
				UVs.Add(FVector2D(static_cast<float>(Seg) / Segments, static_cast<float>(SideIndex) / Sides));
				Colors.Add(White);
				Tangents.Add(FProcMeshTangent(Side, false));
			}
		}

		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			for (int32 SideIndex = 0; SideIndex < Sides; ++SideIndex)
			{
				const int32 A = Seg * (Sides + 1) + SideIndex;
				const int32 B = A + Sides + 1;
				Triangles.Add(A);
				Triangles.Add(A + 1);
				Triangles.Add(B);
				Triangles.Add(A + 1);
				Triangles.Add(B + 1);
				Triangles.Add(B);

				// reverse winding — identical color, so a one-sided mat still reads from above
				Triangles.Add(A);
				Triangles.Add(B);
				Triangles.Add(A + 1);
				Triangles.Add(A + 1);
				Triangles.Add(B);
				Triangles.Add(B + 1);
			}
		}

		Ring->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
	}
}
