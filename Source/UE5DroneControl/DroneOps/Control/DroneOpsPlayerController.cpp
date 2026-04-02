// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneOpsPlayerController.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "DroneOps/Interfaces/DroneSelectableInterface.h"
#include "DroneOps/Interfaces/DroneInfoProviderInterface.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ADroneOpsPlayerController::ADroneOpsPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	PrimaryActorTick.bCanEverTick = true;
}

void ADroneOpsPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Get drone registry subsystem
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance)
	{
		DroneRegistry = GameInstance->GetSubsystem<UDroneRegistrySubsystem>();
		if (DroneRegistry)
		{
			UE_LOG(LogTemp, Log, TEXT("DroneOpsPlayerController: Connected to DroneRegistry"));
		}
	}

	// Initialize camera mode
	CameraModeState.CameraMode = EDroneCameraMode::Follow;
	CameraModeState.FollowDroneId = 0;
}

void ADroneOpsPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		// Bind legacy input actions (will be replaced with Enhanced Input later)
		InputComponent->BindAction("PrimaryClick", IE_Pressed, this, &ADroneOpsPlayerController::OnPrimaryClick);
		InputComponent->BindAction("ShowInfo", IE_Pressed, this, &ADroneOpsPlayerController::OnShowInfo);
		InputComponent->BindAction("FreeCamToggle", IE_Pressed, this, &ADroneOpsPlayerController::OnFreeCamToggle);
	}
}

void ADroneOpsPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update hovered drone
	AActor* HoveredActor = GetActorUnderCursor();
	if (HoveredActor)
	{
		// Check if actor implements IDroneSelectable (which means it's a drone)
		if (IDroneSelectableInterface* Drone = Cast<IDroneSelectableInterface>(HoveredActor))
		{
			HoveredDroneId = Drone->GetDroneId();
		}
		else
		{
			HoveredDroneId = 0;
		}
	}
	else
	{
		HoveredDroneId = 0;
	}
}

void ADroneOpsPlayerController::OnPrimaryClick()
{
	// Get click location
	FVector WorldLocation;
	if (GetWorldLocationUnderCursor(WorldLocation))
	{
		// Check if we clicked on a drone
		AActor* ClickedActor = GetActorUnderCursor();
		if (ClickedActor)
		{
			HandleDroneClick(ClickedActor);
		}
		else
		{
			HandleMapClick(WorldLocation);
		}
	}
}

void ADroneOpsPlayerController::OnShowInfo()
{
	if (HoveredDroneId > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[FR-03] Opening info panel for DroneId: %d"), HoveredDroneId);
		// Broadcast the request to the HUD
		OnOpenDroneInfoPanelRequested.Broadcast(HoveredDroneId);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[FR-03] Middle click - no drone under cursor"));
	}
}

void ADroneOpsPlayerController::OnFreeCamToggle()
{
	if (CameraModeState.CameraMode == EDroneCameraMode::Follow)
	{
		CameraModeState.CameraMode = EDroneCameraMode::Free;
		UE_LOG(LogTemp, Log, TEXT("Switched to Free Camera"));
		// TODO: Spawn and possess free camera pawn
	}
	else
	{
		CameraModeState.CameraMode = EDroneCameraMode::Follow;
		UE_LOG(LogTemp, Log, TEXT("Switched to Follow Camera"));
		// TODO: Return to follow mode
	}
}

void ADroneOpsPlayerController::HandleMapClick(const FVector& WorldLocation)
{
	if (!DroneRegistry)
	{
		return;
	}

	int32 SelectedDroneId = DroneRegistry->GetPrimarySelectedDrone();
	if (SelectedDroneId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No drone selected"));
		return;
	}

	// Check if drone is locked
	EDroneControlLockReason LockReason;
	if (DroneRegistry->IsControlLocked(SelectedDroneId, LockReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("Drone %d is locked: %d"), SelectedDroneId, (int32)LockReason);
		return;
	}

	// Send target command
	SendTargetCommand(SelectedDroneId, WorldLocation);

	// Draw debug marker
	DrawDebugSphere(GetWorld(), WorldLocation, 50.0f, 12, FColor::Green, false, 2.0f);
}

void ADroneOpsPlayerController::HandleDroneClick(AActor* ClickedActor)
{
	if (!DroneRegistry)
	{
		return;
	}

	// TODO: Get DroneId from clicked actor (via interface or component)
	// For now, just log
	UE_LOG(LogTemp, Log, TEXT("Clicked on actor: %s"), *ClickedActor->GetName());

	// Example: DroneRegistry->SetPrimarySelectedDrone(DroneId);
}

void ADroneOpsPlayerController::SendTargetCommand(int32 DroneId, const FVector& TargetWorldLocation)
{
	if (!DroneRegistry)
	{
		return;
	}

	// Get coordinate service
	TScriptInterface<ICoordinateService> CoordService = DroneRegistry->GetCoordinateService();
	if (!CoordService.GetObject())
	{
		UE_LOG(LogTemp, Error, TEXT("No coordinate service available"));
		return;
	}

	// Convert to NED using the interface
	FVector NedLocation = ICoordinateService::Execute_WorldToNed(CoordService.GetObject(), TargetWorldLocation);

	// Create command
	FDroneTargetCommand Command;
	Command.DroneId = DroneId;
	Command.TargetWorldLocation = TargetWorldLocation;
	Command.TargetNedLocation = NedLocation;
	Command.Mode = 0; // Default mode
	Command.IssuedAt = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("Target command for Drone %d: World=(%.1f, %.1f, %.1f) NED=(%.2f, %.2f, %.2f)"),
		DroneId,
		TargetWorldLocation.X, TargetWorldLocation.Y, TargetWorldLocation.Z,
		NedLocation.X, NedLocation.Y, NedLocation.Z);

	// TODO: Send via DroneCommandSenderComponent
	// For now, this is just a placeholder
}

AActor* ADroneOpsPlayerController::GetActorUnderCursor() const
{
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		return HitResult.GetActor();
	}
	return nullptr;
}

bool ADroneOpsPlayerController::GetWorldLocationUnderCursor(FVector& OutLocation) const
{
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		OutLocation = HitResult.Location;
		return true;
	}
	return false;
}
