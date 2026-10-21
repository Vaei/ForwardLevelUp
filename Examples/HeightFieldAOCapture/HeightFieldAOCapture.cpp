// Copyright (c) 2026 Jared Taylor.

#include "HeightFieldAOCapture.h"

#include "Components/BoxComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "MobileHeightFieldAO.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeightFieldAOCapture)

AHeightFieldAOCapture::AHeightFieldAOCapture()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	Bounds->SetBoxExtent(FVector(10000.f, 10000.f, 3000.f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetMobility(EComponentMobility::Static);
	RootComponent = Bounds;

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(Bounds);
	CaptureComponent->SetUsingAbsoluteRotation(true);
	CaptureComponent->SetUsingAbsoluteScale(true);
	CaptureComponent->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
	CaptureComponent->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureComponent->bAutoCalculateOrthoPlanes = false;
	// Mobile writes nothing useful for SCS_SceneDepth, so the depth comes from the alpha channel
	CaptureComponent->CaptureSource = SCS_SceneColorSceneDepth;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_LegacySceneCapture;

	FEngineShowFlags& Flags = CaptureComponent->ShowFlags;
	Flags.SetSkeletalMeshes(false);
	Flags.SetParticles(false);
	Flags.SetTranslucency(false);
	Flags.SetFog(false);
	Flags.SetAtmosphere(false);
	Flags.SetDynamicShadows(false);
	Flags.SetAmbientOcclusion(false);
	Flags.SetDistanceFieldAO(false);
	Flags.SetPostProcessing(false);
	Flags.SetLighting(false);
}

void AHeightFieldAOCapture::Recapture()
{
	SetActorTickEnabled(true);
}

void AHeightFieldAOCapture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Recapture();
}

void AHeightFieldAOCapture::BeginPlay()
{
	Super::BeginPlay();
	Recapture();
}

void AHeightFieldAOCapture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clear();
	Super::EndPlay(EndPlayReason);
}

void AHeightFieldAOCapture::Destroyed()
{
	Clear();
	Super::Destroyed();
}

void AHeightFieldAOCapture::Tick(float DeltaSeconds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HeightFieldAOCapture::Tick);

	Super::Tick(DeltaSeconds);

	SetActorTickEnabled(false);
	Capture();
}

void AHeightFieldAOCapture::Capture()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HeightFieldAOCapture::Capture);

	UWorld* World = GetWorld();
	if (!World || !World->Scene || !FApp::CanEverRender())
	{
		return;
	}

	if (!HeightTarget || HeightTarget->SizeX != Resolution)
	{
		HeightTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("HeightTarget"), RF_Transient);
		HeightTarget->RenderTargetFormat = RTF_RGBA16f;
		HeightTarget->ClearColor = FLinearColor(UE_BIG_NUMBER, 0.f, 0.f, 0.f);
		HeightTarget->InitAutoFormat(Resolution, Resolution);
		HeightTarget->UpdateResourceImmediate(true);
	}

	const FBox Square = GetSquareBounds();
	CaptureComponent->TextureTarget = HeightTarget;
	CaptureComponent->OrthoWidth = Square.GetSize().X;
	CaptureComponent->SetWorldLocationAndRotation(FVector(Square.GetCenter().X, Square.GetCenter().Y, Square.Max.Z), FRotator(-90.f, 0.f, 0.f));

	CaptureComponent->HiddenActors.Reset();
	for (AActor* Actor : TActorRange<AActor>(World))
	{
		const USceneComponent* Root = Actor->GetRootComponent();
		if (Root && Root->Mobility == EComponentMobility::Movable)
		{
			CaptureComponent->HiddenActors.Add(Actor);
		}
	}

	CaptureComponent->CaptureScene();
	CaptureComponent->HiddenActors.Reset();

	SetMobileHeightFieldAO(World->Scene, HeightTarget, Square);
}

void AHeightFieldAOCapture::Clear()
{
	if (const UWorld* World = GetWorld(); World && World->Scene)
	{
		SetMobileHeightFieldAO(World->Scene, nullptr, FBox(ForceInit));
	}
}

FBox AHeightFieldAOCapture::GetSquareBounds() const
{
	const FBox Box = Bounds->Bounds.GetBox();
	const FVector Center = Box.GetCenter();
	const FVector Extent = Box.GetExtent();
	const double Side = FMath::Max(Extent.X, Extent.Y);
	return FBox(FVector(Center.X - Side, Center.Y - Side, Box.Min.Z), FVector(Center.X + Side, Center.Y + Side, Box.Max.Z));
}
