// portal health tint and tree sway, built in code like the mutant warmth wash

#include "Core/PortalShaders.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

#if WITH_EDITOR
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionBounds.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#endif

#include "Engine/StaticMesh.h"

namespace PortalShaders
{
#if WITH_EDITOR
	struct FGraph
	{
		UMaterial* Mat = nullptr;
		TArray<UMaterialExpression*> Exprs;

		template <typename T>
		T* Node()
		{
			T* Expr = NewObject<T>(Mat);
			Expr->Material = Mat;
			Exprs.Add(Expr);
			return Expr;
		}
	};

	static UMaterialExpressionConstant* Const1(FGraph& G, float Value)
	{
		UMaterialExpressionConstant* Expr = G.Node<UMaterialExpressionConstant>();
		Expr->R = Value;
		return Expr;
	}

	static UMaterialExpressionConstant3Vector* Const3(FGraph& G, const FLinearColor& Value)
	{
		UMaterialExpressionConstant3Vector* Expr = G.Node<UMaterialExpressionConstant3Vector>();
		Expr->Constant = Value;
		return Expr;
	}

	static UMaterialExpressionComponentMask* MaskChannel(FGraph& G, UMaterialExpression* Source, bool bR, bool bG, bool bB, int32 OutputIndex = 0)
	{
		UMaterialExpressionComponentMask* Mask = G.Node<UMaterialExpressionComponentMask>();
		Mask->Input.Connect(OutputIndex, Source);
		Mask->R = bR ? 1 : 0;
		Mask->G = bG ? 1 : 0;
		Mask->B = bB ? 1 : 0;
		Mask->A = 0;
		return Mask;
	}

	static UMaterialExpressionTextureCoordinate* UV0(FGraph& G)
	{
		UMaterialExpressionTextureCoordinate* UV = G.Node<UMaterialExpressionTextureCoordinate>();
		UV->CoordinateIndex = 0;
		return UV;
	}

	// mesh UV0. sampler follows the texture so an sRGB albedo is not sampled as linear (or the reverse)
	static UMaterialExpressionTextureSample* SampleTexture(FGraph& G, UTexture* Tex, UMaterialExpression* UV)
	{
		UMaterialExpressionTextureSample* Sample = G.Node<UMaterialExpressionTextureSample>();
		Sample->Texture = Tex;
		Sample->SamplerType = (Tex && Tex->SRGB) ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
		if (UV)
		{
			Sample->Coordinates.Connect(0, UV);
		}
		return Sample;
	}

	static UMaterialExpression* Saturate01(FGraph& G, UMaterialExpression* Value)
	{
		UMaterialExpressionClamp* ClampExpr = G.Node<UMaterialExpressionClamp>();
		ClampExpr->Input.Connect(0, Value);
		ClampExpr->MinDefault = 0.f;
		ClampExpr->MaxDefault = 1.f;
		return ClampExpr;
	}

	// white at full, warm orange at half, warning red when empty. multiply so the stone atlas still shows
	static UMaterialExpression* MakeHealthTintedColor(FGraph& G, UMaterialExpression* TextureColor)
	{
		UMaterialExpressionScalarParameter* Health = G.Node<UMaterialExpressionScalarParameter>();
		Health->ParameterName = TEXT("HealthAlpha");
		Health->DefaultValue = 1.f;
		Health->SliderMin = 0.f;
		Health->SliderMax = 1.f;
		Health->ExpressionGUID = FGuid::NewGuid();

		// 0 at half health and below, 1 at full
		UMaterialExpressionSubtract* AboveHalf = G.Node<UMaterialExpressionSubtract>();
		AboveHalf->A.Connect(0, Health);
		AboveHalf->B.Connect(0, Const1(G, 0.5f));
		UMaterialExpressionMultiply* UpperT = G.Node<UMaterialExpressionMultiply>();
		UpperT->A.Connect(0, AboveHalf);
		UpperT->B.Connect(0, Const1(G, 2.f));

		// 0 at empty, 1 at half health and above
		UMaterialExpressionMultiply* LowerT = G.Node<UMaterialExpressionMultiply>();
		LowerT->A.Connect(0, Health);
		LowerT->B.Connect(0, Const1(G, 2.f));

		UMaterialExpressionConstant3Vector* Warm = Const3(G, FLinearColor(1.50f, 0.40f, 0.16f, 1.f));
		UMaterialExpressionConstant3Vector* Warning = Const3(G, FLinearColor(1.90f, 0.14f, 0.05f, 1.f));

		UMaterialExpressionLinearInterpolate* UpperColor = G.Node<UMaterialExpressionLinearInterpolate>();
		UpperColor->A.Connect(0, Warm);
		UpperColor->B.Connect(0, Const3(G, FLinearColor::White));
		UpperColor->Alpha.Connect(0, Saturate01(G, UpperT));

		UMaterialExpressionLinearInterpolate* LowerColor = G.Node<UMaterialExpressionLinearInterpolate>();
		LowerColor->A.Connect(0, Warning);
		LowerColor->B.Connect(0, Warm);
		LowerColor->Alpha.Connect(0, Saturate01(G, LowerT));

		// health above half uses the white-to-warm half; at and below half uses warm-to-red
		UMaterialExpressionSubtract* SelectRaw = G.Node<UMaterialExpressionSubtract>();
		SelectRaw->A.Connect(0, Health);
		SelectRaw->B.Connect(0, Const1(G, 0.5f));
		UMaterialExpressionMultiply* SelectWide = G.Node<UMaterialExpressionMultiply>();
		SelectWide->A.Connect(0, SelectRaw);
		SelectWide->B.Connect(0, Const1(G, 1000.f));

		UMaterialExpressionLinearInterpolate* Tint = G.Node<UMaterialExpressionLinearInterpolate>();
		Tint->A.Connect(0, LowerColor);
		Tint->B.Connect(0, UpperColor);
		Tint->Alpha.Connect(0, Saturate01(G, SelectWide));

		UMaterialExpressionMultiply* Tinted = G.Node<UMaterialExpressionMultiply>();
		Tinted->A.Connect(0, TextureColor);
		Tinted->B.Connect(0, Tint);
		return Tinted;
	}

	static bool Finish(FGraph& G, EMaterialShadingModel Model, UMaterialExpression* BaseColor, UMaterialExpression* Emissive, UMaterialExpression* Roughness, UMaterialExpression* WorldOffset)
	{
		UMaterialExpressionSubstrateShadingModels* Shading = G.Node<UMaterialExpressionSubstrateShadingModels>();
		Shading->ShadingModelOverride = Model;
		if (BaseColor)
		{
			Shading->BaseColor.Connect(0, BaseColor);
		}
		if (Emissive)
		{
			Shading->EmissiveColor.Connect(0, Emissive);
		}
		if (Roughness)
		{
			Shading->Roughness.Connect(0, Roughness);
		}

		UMaterialEditorOnlyData* EditorOnly = G.Mat->GetEditorOnlyData();
		if (!EditorOnly)
		{
			return false;
		}

		for (UMaterialExpression* Expr : G.Exprs)
		{
			EditorOnly->ExpressionCollection.AddExpression(Expr);
		}
		EditorOnly->FrontMaterial.Connect(0, Shading);
		if (BaseColor)
		{
			EditorOnly->BaseColor.Connect(0, BaseColor);
		}
		if (Emissive)
		{
			EditorOnly->EmissiveColor.Connect(0, Emissive);
		}
		if (Roughness)
		{
			EditorOnly->Roughness.Connect(0, Roughness);
		}
		if (WorldOffset)
		{
			EditorOnly->WorldPositionOffset.Connect(0, WorldOffset);
			G.Mat->bAlwaysEvaluateWorldPositionOffset = true;
		}

		G.Mat->UpdateCachedExpressionData();
		G.Mat->PostEditChange();
		G.Mat->ForceRecompileForRendering();
		G.Mat->AddToRoot();
		return true;
	}

	static UMaterial* BuildPortalMaterial(const TCHAR* Name)
	{
		// stone slot on SM_PortalA is this atlas via MI_DefaultPBR. T_Portal01 is the swirl, not the arch
		UTexture* Albedo = LoadObject<UTexture>(nullptr, TEXT("/Game/RPGTinyFantasyForest/Texture/T_BaseColor.T_BaseColor"));
		if (!Albedo)
		{
			return nullptr;
		}

		FGraph G;
		G.Mat = NewObject<UMaterial>(GetTransientPackage(), Name, RF_Transient);
		G.Mat->MaterialDomain = MD_Surface;
		G.Mat->BlendMode = BLEND_Opaque;
		G.Mat->SetShadingModel(MSM_DefaultLit);
		G.Mat->TwoSided = 1;
		G.Mat->SetUsageByFlag(MATUSAGE_StaticMesh, true);

		UMaterialExpression* UV = UV0(G);
		UMaterialExpression* Color = MakeHealthTintedColor(G, SampleTexture(G, Albedo, UV));

		UMaterialExpression* Rough = nullptr;
		if (UTexture* Ram = LoadObject<UTexture>(nullptr, TEXT("/Game/RPGTinyFantasyForest/Texture/T_RAM.T_RAM")))
		{
			Rough = MaskChannel(G, SampleTexture(G, Ram, UV), true, false, false);
		}
		else
		{
			Rough = Const1(G, 0.8f);
		}

		if (!Finish(G, MSM_DefaultLit, Color, nullptr, Rough, nullptr))
		{
			return nullptr;
		}
		return G.Mat;
	}

	static UMaterialInterface* GetPortalStone()
	{
		static TObjectPtr<UMaterial> Cached = nullptr;
		if (!Cached)
		{
			Cached = BuildPortalMaterial(TEXT("M_PortalStone"));
		}
		return Cached;
	}

	static UMaterialInterface* GetTreeSway()
	{
		static TObjectPtr<UMaterial> Cached = nullptr;
		if (Cached)
		{
			return Cached;
		}

		UTexture* Albedo = LoadObject<UTexture>(nullptr, TEXT("/Game/RPGTinyFantasyForest/Texture/T_BaseColor.T_BaseColor"));
		if (!Albedo)
		{
			return nullptr;
		}

		FGraph G;
		G.Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_TreeSway"), RF_Transient);
		G.Mat->MaterialDomain = MD_Surface;
		G.Mat->BlendMode = BLEND_Opaque;
		G.Mat->SetShadingModel(MSM_DefaultLit);
		G.Mat->SetUsageByFlag(MATUSAGE_StaticMesh, true);
		G.Mat->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes, true);
		G.Mat->MaxWorldPositionOffsetDisplacement = 40.f;

		UMaterialExpression* UV = UV0(G);
		UMaterialExpression* Color = SampleTexture(G, Albedo, UV);
		UMaterialExpression* Rough = nullptr;
		if (UTexture* Ram = LoadObject<UTexture>(nullptr, TEXT("/Game/RPGTinyFantasyForest/Texture/T_RAM.T_RAM")))
		{
			Rough = MaskChannel(G, SampleTexture(G, Ram, UV), true, false, false);
		}
		else
		{
			Rough = Const1(G, 0.78f);
		}

		UMaterialExpressionTime* Time = G.Node<UMaterialExpressionTime>();
		UMaterialExpressionWorldPosition* WorldPos = G.Node<UMaterialExpressionWorldPosition>();
		WorldPos->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;

		// height 0 at the bottom of the mesh bounds, 1 at the top. pivot Z is often ~0, which clamped the old sway to nothing
		UMaterialExpressionLocalPosition* LocalPos = G.Node<UMaterialExpressionLocalPosition>();
		LocalPos->IncludedOffsets = EPositionIncludedOffsets::ExcludeOffsets;
		LocalPos->LocalOrigin = ELocalPositionOrigin::Instance;

		UMaterialExpressionBounds* Bounds = G.Node<UMaterialExpressionBounds>();
		Bounds->Type = MEILB_InstanceLocal;

		UMaterialExpression* LocalZ = MaskChannel(G, LocalPos, false, false, true);
		UMaterialExpression* MinZ = MaskChannel(G, Bounds, false, false, true, UMaterialExpressionBounds::BoundsMinOutputIndex);
		UMaterialExpression* MaxZ = MaskChannel(G, Bounds, false, false, true, UMaterialExpressionBounds::BoundsMaxOutputIndex);

		UMaterialExpressionSubtract* FromMin = G.Node<UMaterialExpressionSubtract>();
		FromMin->A.Connect(0, LocalZ);
		FromMin->B.Connect(0, MinZ);

		UMaterialExpressionSubtract* Span = G.Node<UMaterialExpressionSubtract>();
		Span->A.Connect(0, MaxZ);
		Span->B.Connect(0, MinZ);

		UMaterialExpressionAdd* SpanSafe = G.Node<UMaterialExpressionAdd>();
		SpanSafe->A.Connect(0, Span);
		SpanSafe->B.Connect(0, Const1(G, 1.f));

		UMaterialExpressionDivide* HeightRaw = G.Node<UMaterialExpressionDivide>();
		HeightRaw->A.Connect(0, FromMin);
		HeightRaw->B.Connect(0, SpanSafe);

		UMaterialExpressionClamp* Height01 = G.Node<UMaterialExpressionClamp>();
		Height01->Input.Connect(0, HeightRaw);
		Height01->MinDefault = 0.f;
		Height01->MaxDefault = 1.f;

		// world units at the canopy. the base of the bounds stays planted
		UMaterialExpressionMultiply* HeightAmp = G.Node<UMaterialExpressionMultiply>();
		HeightAmp->A.Connect(0, Height01);
		HeightAmp->B.Connect(0, Const1(G, 24.f));

		auto MakeAxis = [&G, Time, WorldPos, HeightAmp](bool bAxisX, float Period, float WorldFreq, float AxisScale)
		{
			UMaterialExpressionMultiply* Spatial = G.Node<UMaterialExpressionMultiply>();
			Spatial->A.Connect(0, MaskChannel(G, WorldPos, bAxisX, !bAxisX, false));
			Spatial->B.Connect(0, Const1(G, WorldFreq));

			UMaterialExpressionAdd* Phase = G.Node<UMaterialExpressionAdd>();
			Phase->A.Connect(0, Time);
			Phase->B.Connect(0, Spatial);

			UMaterialExpressionSine* Wave = G.Node<UMaterialExpressionSine>();
			Wave->Period = Period;
			Wave->Input.Connect(0, Phase);

			UMaterialExpressionMultiply* ByHeight = G.Node<UMaterialExpressionMultiply>();
			ByHeight->A.Connect(0, Wave);
			ByHeight->B.Connect(0, HeightAmp);

			UMaterialExpressionMultiply* Scaled = G.Node<UMaterialExpressionMultiply>();
			Scaled->A.Connect(0, ByHeight);
			Scaled->B.Connect(0, Const1(G, AxisScale));
			return Scaled;
		};

		UMaterialExpression* SwayX = MakeAxis(true, 3.6f, 0.01f, 1.f);
		UMaterialExpression* SwayY = MakeAxis(false, 4.8f, 0.01f, 0.55f);
		UMaterialExpressionConstant* Zero = Const1(G, 0.f);

		UMaterialExpressionAppendVector* XY = G.Node<UMaterialExpressionAppendVector>();
		XY->A.Connect(0, SwayX);
		XY->B.Connect(0, SwayY);
		UMaterialExpressionAppendVector* Offset = G.Node<UMaterialExpressionAppendVector>();
		Offset->A.Connect(0, XY);
		Offset->B.Connect(0, Zero);

		if (!Finish(G, MSM_DefaultLit, Color, nullptr, Rough, Offset))
		{
			return nullptr;
		}
		Cached = G.Mat;
		return Cached;
	}
#else
	static UMaterialInterface* GetPortalStone() { return nullptr; }
	static UMaterialInterface* GetTreeSway() { return nullptr; }
#endif

	// MI_Portal01 is the translucent swirl. the stone slot is opaque MI_DefaultPBR
	static bool IsPackPortalEffect(const UMaterialInterface* Mat)
	{
		if (!Mat)
		{
			return false;
		}
		const EBlendMode Blend = Mat->GetBlendMode();
		if (Blend == BLEND_Translucent || Blend == BLEND_Additive || Blend == BLEND_AlphaComposite)
		{
			return true;
		}
		return Mat->GetName().Contains(TEXT("Portal"));
	}

	bool ApplyPortalMaterials(UStaticMeshComponent* Mesh, UObject* Outer)
	{
		if (!Mesh || !Outer)
		{
			return false;
		}

		UMaterialInterface* Stone = GetPortalStone();
		if (!Stone)
		{
			return false;
		}

		UStaticMesh* SM = Mesh->GetStaticMesh();
		const int32 Slots = FMath::Max(Mesh->GetNumMaterials(), 1);
		int32 StoneSlots = 0;
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			UMaterialInterface* Original = SM ? SM->GetMaterial(Slot) : nullptr;
			if (IsPackPortalEffect(Original))
			{
				Mesh->SetMaterial(Slot, Original);
				continue;
			}

			UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Stone, Outer);
			if (!Mid)
			{
				return false;
			}
			Mid->SetScalarParameterValue(TEXT("HealthAlpha"), 1.f);
			Mesh->SetMaterial(Slot, Mid);
			++StoneSlots;
		}

		if (StoneSlots == 0)
		{
			return false;
		}

		Mesh->SetEvaluateWorldPositionOffset(false);
		Mesh->MarkRenderStateDirty();
		UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Portal stone shader on %d/%d slot(s). Pack portal material left on the rest."), StoneSlots, Slots);
		return true;
	}

	void SetPortalHealthAlpha(UStaticMeshComponent* Mesh, float HealthAlpha)
	{
		if (!Mesh)
		{
			return;
		}

		const float Alpha = FMath::Clamp(HealthAlpha, 0.f, 1.f);
		const int32 Slots = Mesh->GetNumMaterials();
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			if (UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(Slot)))
			{
				Mid->SetScalarParameterValue(TEXT("HealthAlpha"), Alpha);
			}
		}
	}

	void ApplyTreeSway(UStaticMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return;
		}

		UMaterialInterface* Sway = GetTreeSway();
		if (!Sway)
		{
			return;
		}

		// override the mesh material. a HISMC would ignore this, these trees are static mesh components
		const int32 Slots = FMath::Max(Mesh->GetNumMaterials(), 1);
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			Mesh->SetMaterial(Slot, Sway);
		}
		Mesh->bDisallowNanite = true;
		Mesh->SetEvaluateWorldPositionOffset(true);
		Mesh->WorldPositionOffsetDisableDistance = 0;
		Mesh->MarkRenderStateDirty();
	}
}
