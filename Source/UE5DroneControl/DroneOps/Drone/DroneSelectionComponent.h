// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneSelectionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSelectionStateChanged, bool, bIsPrimary, bool, bIsSecondary);

/**
 * Component for drone selection state (primary / secondary / hovered)
 * Attach to any Actor that should be selectable as a drone.
 */
UCLASS(ClassGroup = "DroneOps", meta = (BlueprintSpawnableComponent))
class UE5DRONECONTROL_API UDroneSelectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneSelectionComponent();

	// The DroneId this component belongs to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	int32 DroneId = 0;

	// Theme color for this drone (used by outline / highlight)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Selection")
	FLinearColor ThemeColor = FLinearColor::White;

	// ---- State accessors ----

	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetPrimarySelected(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetSecondarySelected(bool bSelected);

	UFUNCTION(BlueprintCallable, Category = "Selection")
	void SetHovered(bool bHovered);

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsPrimarySelected() const { return bIsPrimarySelected; }

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsSecondarySelected() const { return bIsSecondarySelected; }

	UFUNCTION(BlueprintPure, Category = "Selection")
	bool IsHovered() const { return bIsHovered; }

	// Fired when primary or secondary state changes
	UPROPERTY(BlueprintAssignable, Category = "Selection")
	FOnSelectionStateChanged OnSelectionStateChanged;

private:
	UPROPERTY()
	bool bIsPrimarySelected = false;

	UPROPERTY()
	bool bIsSecondarySelected = false;

	UPROPERTY()
	bool bIsHovered = false;

	void BroadcastIfChanged();
};
