// Copyright (c) 2026 Jared Taylor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HeightFieldAOCapture.generated.h"

class UBoxComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * Captures the static level inside its box from above, once, and hands the heights to the renderer's
 * height field AO (r.Mobile.HeightFieldAO). Movable actors are left out, so characters, vehicles and the sky dome
 * never occlude the street.
 *
 * The box is squared up around its centre. Call Recapture after the level inside it changes.
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

	UPROPERTY(VisibleAnywhere, Category=HeightFieldAO)
	TObjectPtr<UBoxComponent> Bounds;

	UPROPERTY(VisibleAnywhere, Category=HeightFieldAO)
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY(Transient, VisibleInstanceOnly, Category=HeightFieldAO)
	TObjectPtr<UTextureRenderTarget2D> HeightTarget;
};
