// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UE5DroneControlCharacter.h"
#include "DroneOps/Interfaces/DroneSelectableInterface.h"
#include "MultiDroneCharacter.generated.h"

class UDroneSelectionComponent;
class UDroneCommandSenderComponent;

/**
 * Multi-drone sender pawn.
 * Implements IDroneSelectableInterface so DroneOpsPlayerController can
 * identify and select it via a hit-test.
 */
UCLASS()
class UE5DRONECONTROL_API AMultiDroneCharacter : public AUE5DroneControlCharacter, public IDroneSelectableInterface
{
	GENERATED_BODY()

public:
	AMultiDroneCharacter();

	virtual void BeginPlay() override;

	// ---- Identity ----

	// Display name shown in UI (e.g. "UAV1")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	FString DroneName = TEXT("UAV");

	// Integer DroneId (1 / 2 / 3 …) – primary key in DroneRegistrySubsystem
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	int32 DroneId = 1;

	// MAVLink system id (2 / 3 / 4 …)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	int32 MavlinkSystemId = 2;

	// Bit position in the 32-bit selection mask (0 / 1 / 2 …)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	int32 BitIndex = 0;

	// UI / outline theme colour
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	FLinearColor ThemeColor = FLinearColor::White;

	// ROS2 topic prefix (e.g. "/px4_1")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	FString TopicPrefix = TEXT("/px4_1");

	// UE5 telemetry receive port (8888 / 8890 / 8892)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	int32 UEReceivePort = 8888;

	// Legacy 0-based index kept for backward compat (optional)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drone Identity")
	int32 DroneIndex = 0;

	// ---- Components ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDroneSelectionComponent> SelectionComponent;

	// CommandSenderComponent is optional here; the canonical sender lives on
	// MultiDroneManager.  We keep a reference so blueprints can call it directly.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDroneCommandSenderComponent> CommandSenderComponent;

	// ---- IDroneSelectableInterface ----
	virtual int32 GetDroneId_Implementation() const override { return DroneId; }
	virtual void OnPrimarySelected_Implementation() override;
	virtual void OnSecondarySelected_Implementation(bool bSelected) override;
	virtual void OnHoveredChanged_Implementation(bool bHovered) override;
	virtual void OnDeselected_Implementation() override;
};
