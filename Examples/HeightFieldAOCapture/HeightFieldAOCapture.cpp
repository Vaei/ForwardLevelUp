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
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HeightFieldAOCapture)

namespace HeightFieldAO
{
	/** Texels nothing has been captured into read as the bottom of the world, so they never occlude. */
	const FLinearColor EmptyTexel(0.f, 0.f, 0.f, 65000.f);
}

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

	// Unlit, so the colour is the albedo that height field GI bounces
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
	bWindowStarted = false;
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

	if (bFollowView)
	{
		UpdateWindow();
		return;
	}

	SetActorTickEnabled(false);
	Capture();
}

UTextureRenderTarget2D* AHeightFieldAOCapture::MakeTarget(const TCHAR* Name, int32 Size) const
{
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(const_cast<AHeightFieldAOCapture*>(this), Name, RF_Transient);
	Target->RenderTargetFormat = RTF_RGBA16f;
	Target->ClearColor = HeightFieldAO::EmptyTexel;
	Target->InitAutoFormat(Size, Size);
	Target->UpdateResourceImmediate(true);
	return Target;
}

void AHeightFieldAOCapture::HideMovableActors()
{
	CaptureComponent->HiddenActors.Reset();
	for (AActor* Actor : TActorRange<AActor>(GetWorld()))
	{
		const USceneComponent* Root = Actor->GetRootComponent();
		if (Root && Root->Mobility == EComponentMobility::Movable)
		{
			CaptureComponent->HiddenActors.Add(Actor);
		}
	}
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
		HeightTarget = MakeTarget(TEXT("HeightTarget"), Resolution);
	}

	const FBox Square = GetSquareBounds();
	CaptureComponent->TextureTarget = HeightTarget;
	CaptureComponent->OrthoWidth = Square.GetSize().X;
	CaptureComponent->SetWorldLocationAndRotation(FVector(Square.GetCenter().X, Square.GetCenter().Y, Square.Max.Z), FRotator(-90.f, 0.f, 0.f));

	HideMovableActors();
	CaptureComponent->CaptureScene();
	CaptureComponent->HiddenActors.Reset();

	SetMobileHeightFieldAO(World->Scene, HeightTarget, Square);
}

void AHeightFieldAOCapture::UpdateWindow()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HeightFieldAOCapture::UpdateWindow);

	UWorld* World = GetWorld();
	if (!World || !World->Scene || !FApp::CanEverRender() || World->ViewLocationsRenderedLastFrame.IsEmpty())
	{
		return;
	}

	const int32 TileResolution = FMath::Max(Resolution / TilesPerAxis, 16);
	if (!bWindowStarted || !HeightTarget || HeightTarget->SizeX != TileResolution * TilesPerAxis)
	{
		HeightTarget = MakeTarget(TEXT("HeightTarget"), TileResolution * TilesPerAxis);
		TileTarget = MakeTarget(TEXT("TileTarget"), TileResolution);
		ValidTiles = FIntRect();
		Window = FIntRect();
		bWindowStarted = true;
	}

	const double TileSize = WindowSize / TilesPerAxis;
	const FVector& ViewLocation = World->ViewLocationsRenderedLastFrame[0];
	const FIntPoint ViewTile(FMath::FloorToInt32(ViewLocation.X / TileSize), FMath::FloorToInt32(ViewLocation.Y / TileSize));
	const FIntPoint WindowMin = ViewTile - FIntPoint(TilesPerAxis / 2);
	const FIntRect NewWindow(WindowMin, WindowMin + FIntPoint(TilesPerAxis));

	if (NewWindow != Window)
	{
		Window = NewWindow;
		ValidTiles.Clip(Window);
		if (ValidTiles.Area() <= 0)
		{
			ValidTiles = FIntRect();
		}

		PendingTiles.Reset();
		for (int32 X = Window.Min.X; X < Window.Max.X; X++)
		{
			for (int32 Y = Window.Min.Y; Y < Window.Max.Y; Y++)
			{
				if (!ValidTiles.Contains(FIntPoint(X, Y)))
				{
					PendingTiles.Add(FIntPoint(X, Y));
				}
			}
		}

		// Popped from the back, so the tiles nearest the view go first
		PendingTiles.Sort([ViewTile](const FIntPoint& A, const FIntPoint& B) { return (A - ViewTile).SizeSquared() > (B - ViewTile).SizeSquared(); });
		PublishWindow();
	}

	if (PendingTiles.IsEmpty())
	{
		return;
	}

	HideMovableActors();
	for (int32 Index = 0; Index < TilesPerTick && !PendingTiles.IsEmpty(); Index++)
	{
		CaptureTile(PendingTiles.Pop());
	}
	CaptureComponent->HiddenActors.Reset();

	if (PendingTiles.IsEmpty())
	{
		ValidTiles = Window;
		PublishWindow();
	}
}

void AHeightFieldAOCapture::CaptureTile(const FIntPoint& Tile)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HeightFieldAOCapture::CaptureTile);

	const double TileSize = WindowSize / TilesPerAxis;
	const int32 TileResolution = TileTarget->SizeX;
	const double Top = Bounds->Bounds.GetBox().Max.Z;

	CaptureComponent->TextureTarget = TileTarget;
	CaptureComponent->OrthoWidth = TileSize;
	CaptureComponent->SetWorldLocationAndRotation(FVector((Tile.X + 0.5) * TileSize, (Tile.Y + 0.5) * TileSize, Top), FRotator(-90.f, 0.f, 0.f));
	CaptureComponent->CaptureScene();

	// The texture's U runs along +Y and V along -X, so a tile's column follows its Y and its row its -X, wrapped
	const int32 Tiles = TilesPerAxis;
	const auto Wrap = [Tiles](int32 Value) { return ((Value % Tiles) + Tiles) % Tiles; };
	const FIntPoint WrappedTexel(Wrap(Tile.Y) * TileResolution, Wrap(-(Tile.X + 1)) * TileResolution);

	FTextureRenderTargetResource* Source = TileTarget->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* Dest = HeightTarget->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(DEHeightFieldAOCopyTile)([Source, Dest, WrappedTexel, TileResolution](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		FRDGTextureRef SourceTexture = RegisterExternalTexture(GraphBuilder, Source->GetRenderTargetTexture(), TEXT("HeightFieldAOTile"));
		FRDGTextureRef DestTexture = RegisterExternalTexture(GraphBuilder, Dest->GetRenderTargetTexture(), TEXT("HeightFieldAO"));

		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size = FIntVector(TileResolution, TileResolution, 1);
		CopyInfo.DestPosition = FIntVector(WrappedTexel.X, WrappedTexel.Y, 0);
		AddCopyTexturePass(GraphBuilder, SourceTexture, DestTexture, CopyInfo);
		GraphBuilder.Execute();
	});
}

void AHeightFieldAOCapture::PublishWindow()
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene)
	{
		return;
	}

	if (ValidTiles.Area() <= 0)
	{
		Clear();
		return;
	}

	const double TileSize = WindowSize / TilesPerAxis;
	const FBox2D Valid(FVector2D(ValidTiles.Min) * TileSize, FVector2D(ValidTiles.Max) * TileSize);
	SetMobileHeightFieldAOWrapped(World->Scene, HeightTarget, WindowSize, Bounds->Bounds.GetBox().Max.Z, Valid);
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
