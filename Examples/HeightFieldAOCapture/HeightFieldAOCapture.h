// Copyright (c) 2026 Jared Taylor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeightFieldAOCapture.generated.h"

class UBoxComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * Captures the static level from above and hands the heights to the renderer's height field AO (r.Mobile.HeightFieldAO)
 * and GI (r.Mobile.HeightFieldGI). Movable actors are left out, so characters, vehicles and the sky dome never occlude the street.
 *
 * By default the box is squared up around its centre and captured once; call Recapture after the level inside it changes.
 * With bFollowView, a window of WindowSize follows the view instead, captured a tile at a time into a toroidal texture, so only
 * the tiles the window moves onto are ever captured. The box then only sets the capture's top.
 */
UCLASS()
class AHeightFieldAOCapture : public AActor
{
	GENERATED_BODY()

public:
	AHeightFieldAOCapture();

	/** Height texels along each side. */
	UPROPERTY(EditAnywhere, Category=HeightFieldAO, meta=(ClampMin="64", ClampMax="8192"))
	int32 Resolution = 2048;

	/** Follow the view with a window of tiles instead of capturing the box once. */
	UPROPERTY(EditAnywhere, Category=HeightFieldAO)
	bool bFollowView = false;

	/** World size of the window along each side. Keep it well past the height field AO's MaxDistance from the view. */
	UPROPERTY(EditAnywhere, Category=HeightFieldAO, meta=(EditCondition="bFollowView", ClampMin="1000"))
	float WindowSize = 40000.f;

	/** Tiles along each side of the window. More tiles make each capture cheaper and each move capture less. */
	UPROPERTY(EditAnywhere, Category=HeightFieldAO, meta=(EditCondition="bFollowView", ClampMin="2", ClampMax="32"))
	int32 TilesPerAxis = 8;

	/** Tiles captured per tick while the window catches up. */
	UPROPERTY(EditAnywhere, Category=HeightFieldAO, meta=(EditCondition="bFollowView", ClampMin="1", ClampMax="64"))
	int32 TilesPerTick = 1;

	/** Captures again on the next tick. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category=HeightFieldAO)
	void Recapture();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

private:
	void Capture();
	void Clear();
	FBox GetSquareBounds() const;

	/** Moves the window to the view, then captures the tiles it is missing. */
	void UpdateWindow();
	void CaptureTile(const FIntPoint& Tile);
	void PublishWindow();
	void HideMovableActors();
	UTextureRenderTarget2D* MakeTarget(const TCHAR* Name, int32 Size) const;

	UPROPERTY(VisibleAnywhere, Category=HeightFieldAO)
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category=HeightFieldAO)
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY(Transient, VisibleInstanceOnly, Category=HeightFieldAO)
	TObjectPtr<UTextureRenderTarget2D> HeightTarget;

	/** Follow mode: each tile is captured here, then copied into its wrapped place in HeightTarget. */
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> TileTarget;

	/** Tile coordinates, max exclusive: the window around the view, and the part of it whose tiles are all captured. */
	FIntRect Window;
	FIntRect ValidTiles;
	TArray<FIntPoint> PendingTiles;
	bool bWindowStarted = false;
};
