// death puff and tower vignette, built in code. substrate makes an editor-only material easy to get wrong

#include "Core/MatchVfx.h"

#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

#if WITH_EDITOR
#include "Distributions/DistributionFloatConstant.h"
#include "Distributions/DistributionFloatConstantCurve.h"
#include "Distributions/DistributionFloatUniform.h"
#include "Distributions/DistributionVectorConstant.h"
#include "Distributions/DistributionVectorConstantCurve.h"
#include "Distributions/DistributionVectorUniform.h"
#include "Engine/BlendableInterface.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionSubstrate.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Particles/Acceleration/ParticleModuleAcceleration.h"
#include "Particles/Color/ParticleModuleColorOverLife.h"
#include "Particles/Lifetime/ParticleModuleLifetime.h"
#include "Particles/Location/ParticleModuleLocationPrimitiveSphere.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/Size/ParticleModuleSize.h"
#include "Particles/Spawn/ParticleModuleSpawn.h"
#include "Particles/Velocity/ParticleModuleVelocity.h"
#endif

namespace MatchVfx
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

	static UMaterialExpressionConstant2Vector* Const2(FGraph& G, float X, float Y)
	{
		UMaterialExpressionConstant2Vector* Expr = G.Node<UMaterialExpressionConstant2Vector>();
		Expr->R = X;
		Expr->G = Y;
		return Expr;
	}

	static UMaterialExpressionScalarParameter* ScalarParam(FGraph& G, const TCHAR* Name, float DefaultValue)
	{
		UMaterialExpressionScalarParameter* Param = G.Node<UMaterialExpressionScalarParameter>();
		Param->ParameterName = Name;
		Param->DefaultValue = DefaultValue;
		Param->SliderMin = 0.f;
		Param->SliderMax = 1.f;
		Param->ExpressionGUID = FGuid::NewGuid();
		return Param;
	}

	static void BakeFloat(FRawDistributionFloat& Dist)
	{
		if (Dist.Distribution)
		{
			Dist.Distribution->bIsDirty = true;
			Dist.Initialize();
		}
	}

	static void BakeVector(FRawDistributionVector& Dist)
	{
		if (Dist.Distribution)
		{
			Dist.Distribution->bIsDirty = true;
			Dist.Initialize();
		}
	}

	static void SetFloatConst(FRawDistributionFloat& Dist, float Value)
	{
		if (UDistributionFloatConstant* Constant = Cast<UDistributionFloatConstant>(Dist.Distribution))
		{
			Constant->Constant = Value;
		}
		BakeFloat(Dist);
	}

	static void SetFloatRange(FRawDistributionFloat& Dist, float Min, float Max)
	{
		if (UDistributionFloatUniform* Uniform = Cast<UDistributionFloatUniform>(Dist.Distribution))
		{
			Uniform->Min = Min;
			Uniform->Max = Max;
		}
		BakeFloat(Dist);
	}

	static void SetVectorRange(FRawDistributionVector& Dist, const FVector& Min, const FVector& Max)
	{
		if (UDistributionVectorUniform* Uniform = Cast<UDistributionVectorUniform>(Dist.Distribution))
		{
			Uniform->Min = Min;
			Uniform->Max = Max;
			Uniform->bLockAxes = false;
		}
		BakeVector(Dist);
	}

	static void SetVectorConst(FRawDistributionVector& Dist, const FVector& Value)
	{
		if (UDistributionVectorConstant* Constant = Cast<UDistributionVectorConstant>(Dist.Distribution))
		{
			Constant->Constant = Value;
			Constant->bLockAxes = false;
		}
		BakeVector(Dist);
	}

	static void SetFadeCurve(FRawDistributionVector& Color, FRawDistributionFloat& Alpha,
		const FLinearColor& StartColor, const FLinearColor& EndColor, float StartAlpha, float EndAlpha)
	{
		if (UDistributionVectorConstantCurve* Curve = Cast<UDistributionVectorConstantCurve>(Color.Distribution))
		{
			Curve->ConstantCurve.Reset();
			const int32 Key0 = Curve->CreateNewKey(0.f);
			const int32 Key1 = Curve->CreateNewKey(1.f);
			Curve->SetKeyOut(0, Key0, StartColor.R);
			Curve->SetKeyOut(1, Key0, StartColor.G);
			Curve->SetKeyOut(2, Key0, StartColor.B);
			Curve->SetKeyOut(0, Key1, EndColor.R);
			Curve->SetKeyOut(1, Key1, EndColor.G);
			Curve->SetKeyOut(2, Key1, EndColor.B);
			Curve->SetKeyInterpMode(Key0, CIM_Linear);
			Curve->SetKeyInterpMode(Key1, CIM_Linear);
		}
		BakeVector(Color);

		if (UDistributionFloatConstantCurve* Curve = Cast<UDistributionFloatConstantCurve>(Alpha.Distribution))
		{
			Curve->ConstantCurve.Reset();
			const int32 Key0 = Curve->CreateNewKey(0.f);
			const int32 Key1 = Curve->CreateNewKey(1.f);
			Curve->SetKeyOut(0, Key0, StartAlpha);
			Curve->SetKeyOut(0, Key1, EndAlpha);
			Curve->SetKeyInterpMode(Key0, CIM_Linear);
			Curve->SetKeyInterpMode(Key1, CIM_Linear);
		}
		BakeFloat(Alpha);
	}

	// soft round sprite. particle color is the tint, sphere mask keeps the quad from reading as a card
	static UMaterial* BuildSpriteMaterial()
	{
		static TObjectPtr<UMaterial> Cached = nullptr;
		if (Cached)
		{
			return Cached;
		}

		FGraph G;
		G.Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_DeathBurstSprite"), RF_Transient);
		G.Mat->MaterialDomain = MD_Surface;
		G.Mat->BlendMode = BLEND_Translucent;
		G.Mat->SetShadingModel(MSM_Unlit);
		G.Mat->TwoSided = 1;
		G.Mat->bIsThinSurface = 1;
		G.Mat->SetUsageByFlag(MATUSAGE_ParticleSprites, true);

		UMaterialExpressionParticleColor* ParticleColor = G.Node<UMaterialExpressionParticleColor>();
		UMaterialExpressionTextureCoordinate* UV = G.Node<UMaterialExpressionTextureCoordinate>();
		UV->CoordinateIndex = 0;

		UMaterialExpressionSphereMask* Soft = G.Node<UMaterialExpressionSphereMask>();
		Soft->A.Connect(0, UV);
		Soft->B.Connect(0, Const2(G, 0.5f, 0.5f));
		Soft->AttenuationRadius = 0.5f;
		Soft->HardnessPercent = 18.f;

		UMaterialExpressionMultiply* Emissive = G.Node<UMaterialExpressionMultiply>();
		Emissive->A.Connect(0, ParticleColor);
		Emissive->B.Connect(0, Soft);

		UMaterialExpressionMultiply* Opacity = G.Node<UMaterialExpressionMultiply>();
		Opacity->A.Connect(4, ParticleColor);
		Opacity->B.Connect(0, Soft);

		UMaterialExpressionSubstrateShadingModels* Shading = G.Node<UMaterialExpressionSubstrateShadingModels>();
		Shading->ShadingModelOverride = MSM_Unlit;
		Shading->EmissiveColor.Connect(0, Emissive);
		Shading->BaseColor.Connect(0, Emissive);
		Shading->Opacity.Connect(0, Opacity);

		UMaterialEditorOnlyData* EditorOnly = G.Mat->GetEditorOnlyData();
		if (!EditorOnly)
		{
			return nullptr;
		}

		for (UMaterialExpression* Expr : G.Exprs)
		{
			EditorOnly->ExpressionCollection.AddExpression(Expr);
		}
		EditorOnly->FrontMaterial.Connect(0, Shading);
		EditorOnly->EmissiveColor.Connect(0, Emissive);
		EditorOnly->BaseColor.Connect(0, Emissive);
		EditorOnly->Opacity.Connect(0, Opacity);

		G.Mat->UpdateCachedExpressionData();
		G.Mat->PostEditChange();
		G.Mat->ForceRecompileForRendering();
		G.Mat->AddToRoot();
		Cached = G.Mat;
		return Cached;
	}

	struct FBurstDesc
	{
		const TCHAR* Name = TEXT("Burst");
		int32 Count = 8;
		int32 CountLow = 6;
		float LifeMin = 0.25f;
		float LifeMax = 0.45f;
		FVector SizeMin = FVector(8.f);
		FVector SizeMax = FVector(12.f);
		FVector VelMin = FVector(0.f);
		FVector VelMax = FVector(0.f);
		float Radius = 12.f;
		float RadialSpeed = 4.f;
		FVector Accel = FVector(0.f, 0.f, -40.f);
		FLinearColor Color0 = FLinearColor::White;
		FLinearColor Color1 = FLinearColor::White;
		float Alpha0 = 0.6f;
		float Alpha1 = 0.f;
	};

	static void AddBurstEmitter(UParticleSystem* System, UMaterialInterface* Sprite, const FBurstDesc& Desc)
	{
		UParticleSpriteEmitter* Emitter = NewObject<UParticleSpriteEmitter>(System);
		Emitter->SetEmitterName(Desc.Name);
		Emitter->InitialAllocationCount = 24;
		Emitter->CreateLODLevel(0);
		Emitter->SetToSensibleDefaults();

		UParticleLODLevel* LOD = Emitter->LODLevels[0];
		UParticleModuleRequired* Required = LOD->RequiredModule;
		Required->Material = Sprite;
		Required->bUseLocalSpace = true;
		Required->EmitterDuration = 0.2f;
		Required->EmitterLoops = 1;
		Required->bKillOnCompleted = false;
		Required->bKillOnDeactivate = false;
		Required->ScreenAlignment = PSA_Square;
		Required->InterpolationMethod = PSUVIM_None;
		Required->bEmitterDurationUseRange = false;
		Required->EmitterDelay = 0.f;

		UParticleModuleSpawn* Spawn = LOD->SpawnModule;
		SetFloatConst(Spawn->Rate, 0.f);
		Spawn->BurstList.Reset();
		FParticleBurst Burst;
		Burst.Count = Desc.Count;
		Burst.CountLow = Desc.CountLow;
		Burst.Time = 0.f;
		Spawn->BurstList.Add(Burst);
		Spawn->ParticleBurstMethod = EPBM_Instant;

		for (UParticleModule* Module : LOD->Modules)
		{
			if (UParticleModuleLifetime* Life = Cast<UParticleModuleLifetime>(Module))
			{
				SetFloatRange(Life->Lifetime, Desc.LifeMin, Desc.LifeMax);
			}
			else if (UParticleModuleSize* Size = Cast<UParticleModuleSize>(Module))
			{
				SetVectorRange(Size->StartSize, Desc.SizeMin, Desc.SizeMax);
			}
			else if (UParticleModuleVelocity* Vel = Cast<UParticleModuleVelocity>(Module))
			{
				SetVectorRange(Vel->StartVelocity, Desc.VelMin, Desc.VelMax);
				SetFloatConst(Vel->StartVelocityRadial, 0.f);
			}
			else if (UParticleModuleColorOverLife* Color = Cast<UParticleModuleColorOverLife>(Module))
			{
				SetFadeCurve(Color->ColorOverLife, Color->AlphaOverLife,
					Desc.Color0, Desc.Color1, Desc.Alpha0, Desc.Alpha1);
			}
		}

		UParticleModuleLocationPrimitiveSphere* Sphere = NewObject<UParticleModuleLocationPrimitiveSphere>(System);
		Sphere->LODValidity = 1;
		Sphere->SurfaceOnly = false;
		Sphere->Velocity = true;
		SetFloatConst(Sphere->StartRadius, Desc.Radius);
		SetFloatConst(Sphere->VelocityScale, Desc.RadialSpeed);
		LOD->Modules.Add(Sphere);

		UParticleModuleAcceleration* Accel = NewObject<UParticleModuleAcceleration>(System);
		Accel->LODValidity = 1;
		Accel->bApplyOwnerScale = false;
		SetVectorConst(Accel->Acceleration, Desc.Accel);
		LOD->Modules.Add(Accel);

		LOD->PeakActiveParticles = 32;
		Emitter->UpdateModuleLists();
		System->Emitters.Add(Emitter);
	}

	static UParticleSystem* BuildDeathBurst()
	{
		UMaterialInterface* Sprite = BuildSpriteMaterial();
		if (!Sprite)
		{
			UE_LOG(LogTemp, Warning, TEXT("[PortalProtect] Death burst sprite material failed. Kills will have no puff."));
			return nullptr;
		}

		UParticleSystem* System = NewObject<UParticleSystem>(GetTransientPackage(), TEXT("PS_EnemyDeathBurst"), RF_Transient);
		System->bAutoDeactivate = true;
		System->bAllowManagedTicking = false;
		System->LODMethod = PARTICLESYSTEMLODMETHOD_DirectSet;
		System->LODDistances.Add(0.f);
		System->LODSettings.Add(FParticleSystemLOD::CreateParticleSystemLOD());
		// dust can live about a second. a glance-away shouldn't freeze the puff immediately
		System->SecondsBeforeInactive = 2.f;

		// dust: bigger tan cloud. still a body-sized puff, not a screen blast
		FBurstDesc Dust;
		Dust.Name = TEXT("Dust");
		Dust.Count = 17;
		Dust.CountLow = 13;
		Dust.LifeMin = 0.5f;
		Dust.LifeMax = 1.05f;
		Dust.SizeMin = FVector(34.f);
		Dust.SizeMax = FVector(62.f);
		Dust.VelMin = FVector(-22.f, -22.f, 45.f);
		Dust.VelMax = FVector(22.f, 22.f, 100.f);
		Dust.Radius = 36.f;
		Dust.RadialSpeed = 12.f;
		Dust.Accel = FVector(0.f, 0.f, -70.f);
		Dust.Color0 = FLinearColor(0.62f, 0.48f, 0.32f);
		Dust.Color1 = FLinearColor(0.40f, 0.32f, 0.24f);
		Dust.Alpha0 = 0.5f;
		Dust.Alpha1 = 0.f;
		AddBurstEmitter(System, Sprite, Dust);

		// sparks: fewer and smaller than the dust, still the warm flick
		FBurstDesc Sparks;
		Sparks.Name = TEXT("Sparks");
		Sparks.Count = 10;
		Sparks.CountLow = 7;
		Sparks.LifeMin = 0.25f;
		Sparks.LifeMax = 0.5f;
		Sparks.SizeMin = FVector(8.f);
		Sparks.SizeMax = FVector(15.f);
		Sparks.VelMin = FVector(-24.f, -24.f, 120.f);
		Sparks.VelMax = FVector(24.f, 24.f, 210.f);
		Sparks.Radius = 14.f;
		Sparks.RadialSpeed = 32.f;
		Sparks.Accel = FVector(0.f, 0.f, -160.f);
		Sparks.Color0 = FLinearColor(1.35f, 0.85f, 0.22f);
		Sparks.Color1 = FLinearColor(0.85f, 0.22f, 0.05f);
		Sparks.Alpha0 = 0.9f;
		Sparks.Alpha1 = 0.f;
		AddBurstEmitter(System, Sprite, Sparks);

		System->AddToRoot();
		UE_LOG(LogTemp, Log, TEXT("[PortalProtect] Death burst ready (dust + sparks, short puff)."));
		return System;
	}

	// screen-space vignette. center stays the scene color. edges warm up as HealthAlpha falls
	static UMaterial* BuildVignetteMaterial()
	{
		static TObjectPtr<UMaterial> Cached = nullptr;
		if (Cached)
		{
			return Cached;
		}

		FGraph G;
		G.Mat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_TowerDamageVignette"), RF_Transient);
		G.Mat->MaterialDomain = MD_PostProcess;
		G.Mat->BlendableLocation = BL_SceneColorAfterTonemapping;
		G.Mat->BlendableOutputAlpha = false;

		UMaterialExpressionSceneTexture* Scene = G.Node<UMaterialExpressionSceneTexture>();
		Scene->SceneTextureId = PPI_PostProcessInput0;
		UMaterialExpressionComponentMask* SceneRGB = G.Node<UMaterialExpressionComponentMask>();
		SceneRGB->Input.Connect(0, Scene);
		SceneRGB->R = 1;
		SceneRGB->G = 1;
		SceneRGB->B = 1;
		SceneRGB->A = 0;

		UMaterialExpressionScreenPosition* Screen = G.Node<UMaterialExpressionScreenPosition>();
		UMaterialExpressionSubtract* Delta = G.Node<UMaterialExpressionSubtract>();
		Delta->A.Connect(0, Screen);
		Delta->B.Connect(0, Const2(G, 0.5f, 0.5f));

		// stretch X by aspect so the ring is round on a widescreen view
		UMaterialExpressionViewSize* ViewSize = G.Node<UMaterialExpressionViewSize>();
		UMaterialExpressionComponentMask* ViewX = G.Node<UMaterialExpressionComponentMask>();
		ViewX->Input.Connect(0, ViewSize);
		ViewX->R = 1;
		ViewX->G = 0;
		ViewX->B = 0;
		ViewX->A = 0;
		UMaterialExpressionComponentMask* ViewY = G.Node<UMaterialExpressionComponentMask>();
		ViewY->Input.Connect(0, ViewSize);
		ViewY->R = 0;
		ViewY->G = 1;
		ViewY->B = 0;
		ViewY->A = 0;
		UMaterialExpressionDivide* Aspect = G.Node<UMaterialExpressionDivide>();
		Aspect->A.Connect(0, ViewX);
		Aspect->B.Connect(0, ViewY);
		UMaterialExpressionAppendVector* AspectScale = G.Node<UMaterialExpressionAppendVector>();
		AspectScale->A.Connect(0, Aspect);
		AspectScale->B.Connect(0, Const1(G, 1.f));
		UMaterialExpressionMultiply* Scaled = G.Node<UMaterialExpressionMultiply>();
		Scaled->A.Connect(0, Delta);
		Scaled->B.Connect(0, AspectScale);
		UMaterialExpressionLength* Radius = G.Node<UMaterialExpressionLength>();
		Radius->Input.Connect(0, Scaled);

		UMaterialExpressionScalarParameter* Health = ScalarParam(G, TEXT("HealthAlpha"), 1.f);
		UMaterialExpressionScalarParameter* Hit = ScalarParam(G, TEXT("HitPulse"), 0.f);

		UMaterialExpressionOneMinus* Damage = G.Node<UMaterialExpressionOneMinus>();
		Damage->Input.Connect(0, Health);
		UMaterialExpressionSaturate* Damage01 = G.Node<UMaterialExpressionSaturate>();
		Damage01->Input.Connect(0, Damage);

		// full health keeps the falloff in the corners. empty health pulls it inward, still clear of center
		UMaterialExpressionLinearInterpolate* Inner = G.Node<UMaterialExpressionLinearInterpolate>();
		Inner->A.Connect(0, Const1(G, 0.32f));
		Inner->B.Connect(0, Const1(G, 0.72f));
		Inner->Alpha.Connect(0, Health);
		UMaterialExpressionLinearInterpolate* Outer = G.Node<UMaterialExpressionLinearInterpolate>();
		Outer->A.Connect(0, Const1(G, 0.95f));
		Outer->B.Connect(0, Const1(G, 1.15f));
		Outer->Alpha.Connect(0, Health);

		UMaterialExpressionSmoothStep* Vig = G.Node<UMaterialExpressionSmoothStep>();
		Vig->Min.Connect(0, Inner);
		Vig->Max.Connect(0, Outer);
		Vig->Value.Connect(0, Radius);

		UMaterialExpressionLinearInterpolate* EdgeScale = G.Node<UMaterialExpressionLinearInterpolate>();
		EdgeScale->A.Connect(0, Const1(G, 0.2f));
		EdgeScale->B.Connect(0, Const1(G, 1.f));
		EdgeScale->Alpha.Connect(0, Damage01);

		UMaterialExpressionMultiply* BaseEdge = G.Node<UMaterialExpressionMultiply>();
		BaseEdge->A.Connect(0, Vig);
		BaseEdge->B.Connect(0, EdgeScale);
		UMaterialExpressionMultiply* PulseEdge = G.Node<UMaterialExpressionMultiply>();
		PulseEdge->A.Connect(0, Vig);
		PulseEdge->B.Connect(0, Hit);
		UMaterialExpressionAdd* EdgeSum = G.Node<UMaterialExpressionAdd>();
		EdgeSum->A.Connect(0, BaseEdge);
		EdgeSum->B.Connect(0, PulseEdge);
		UMaterialExpressionSaturate* Edge = G.Node<UMaterialExpressionSaturate>();
		Edge->Input.Connect(0, EdgeSum);

		// dark warm multiply, plus a small red lift so it reads as heat and not just a black frame
		UMaterialExpressionLinearInterpolate* MulColor = G.Node<UMaterialExpressionLinearInterpolate>();
		MulColor->A.Connect(0, Const3(G, FLinearColor::White));
		MulColor->B.Connect(0, Const3(G, FLinearColor(0.45f, 0.07f, 0.04f, 1.f)));
		MulColor->Alpha.Connect(0, Edge);

		UMaterialExpressionMultiply* Tinted = G.Node<UMaterialExpressionMultiply>();
		Tinted->A.Connect(0, SceneRGB);
		Tinted->B.Connect(0, MulColor);

		UMaterialExpressionMultiply* Lift = G.Node<UMaterialExpressionMultiply>();
		Lift->A.Connect(0, Const3(G, FLinearColor(0.16f, 0.03f, 0.015f, 1.f)));
		Lift->B.Connect(0, Edge);

		UMaterialExpressionAdd* Result = G.Node<UMaterialExpressionAdd>();
		Result->A.Connect(0, Tinted);
		Result->B.Connect(0, Lift);

		// substrate uses this as the new scene color. opacity stays 1 so blend weight does the fade
		UMaterialExpressionSubstratePostProcess* Post = G.Node<UMaterialExpressionSubstratePostProcess>();
		Post->Color.Connect(0, Result);

		UMaterialEditorOnlyData* EditorOnly = G.Mat->GetEditorOnlyData();
		if (!EditorOnly)
		{
			return nullptr;
		}

		for (UMaterialExpression* Expr : G.Exprs)
		{
			EditorOnly->ExpressionCollection.AddExpression(Expr);
		}
		EditorOnly->FrontMaterial.Connect(0, Post);
		EditorOnly->EmissiveColor.Connect(0, Result);

		G.Mat->UpdateCachedExpressionData();
		G.Mat->PostEditChange();
		G.Mat->ForceRecompileForRendering();
		G.Mat->AddToRoot();
		Cached = G.Mat;
		return Cached;
	}
#endif

	UParticleSystem* GetDeathBurst()
	{
#if WITH_EDITOR
		static TObjectPtr<UParticleSystem> Cached = nullptr;
		if (!Cached)
		{
			Cached = BuildDeathBurst();
		}
		return Cached;
#else
		return nullptr;
#endif
	}

	void SpawnDeathBurst(UWorld* World, const FVector& Location, float Scale)
	{
		if (!World || !World->IsGameWorld())
		{
			return;
		}

		UParticleSystem* System = GetDeathBurst();
		if (!System)
		{
			return;
		}

		const float Clamped = FMath::Clamp(Scale, 0.2f, 2.f);
		UGameplayStatics::SpawnEmitterAtLocation(
			World, System, Location, FRotator::ZeroRotator, FVector(Clamped), true);
	}

	UMaterialInterface* GetTowerDamageVignette()
	{
#if WITH_EDITOR
		return BuildVignetteMaterial();
#else
		return nullptr;
#endif
	}
}
