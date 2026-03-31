// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneOpsPlayerController.h"
#include "InputCoreTypes.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/ICoordinateService.h"
#include "DroneOps/Interfaces/DroneSelectableInterface.h"
#include "DroneOps/Drone/DroneSelectionComponent.h"
#include "DroneOps/Drone/DroneCommandSenderComponent.h"
#include "MultiDroneManager.h"
#include "UE5DroneControlCharacter.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

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

	// === DEBUG: Confirm this controller is active ===
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green,
			TEXT(">>> DroneOpsPlayerController ACTIVE <<<"));
	}
	UE_LOG(LogTemp, Warning, TEXT("=== DroneOpsPlayerController::BeginPlay CALLED ==="));

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

	// Show mouse cursor and allow clicking on game world
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ADroneOpsPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Input is handled via direct key state checks in Tick
	// to avoid Enhanced Input vs Legacy binding conflicts.
}

void ADroneOpsPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Direct key state checks (bypasses Enhanced/Legacy binding issues)
	if (WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Left Click Detected!"));
		}
		OnPrimaryClick();
	}
	if (WasInputKeyJustPressed(EKeys::MiddleMouseButton))
	{
		OnShowInfo();
	}
	if (WasInputKeyJustPressed(EKeys::F))
	{
		OnFreeCamToggle();
	}

	// Update hovered drone
	int32 NewHoveredId = 0;
	AActor* HoveredActor = GetActorUnderCursor();
	if (HoveredActor && HoveredActor->GetClass()->ImplementsInterface(UDroneSelectableInterface::StaticClass()))
	{
		NewHoveredId = IDroneSelectableInterface::Execute_GetDroneId(HoveredActor);
	}

	// Notify old hovered actor
	if (HoveredDroneId != NewHoveredId)
	{
		if (HoveredDroneId > 0 && DroneRegistry)
		{
			AActor* OldActor = DroneRegistry->GetReceiverActor(HoveredDroneId);
			if (OldActor && OldActor->GetClass()->ImplementsInterface(UDroneSelectableInterface::StaticClass()))
			{
				IDroneSelectableInterface::Execute_OnHoveredChanged(OldActor, false);
			}
		}
		// Notify new hovered actor
		if (NewHoveredId > 0 && HoveredActor)
		{
			IDroneSelectableInterface::Execute_OnHoveredChanged(HoveredActor, true);
		}
		HoveredDroneId = NewHoveredId;
	}
}

void ADroneOpsPlayerController::OnPrimaryClick()
{
	// === Step 1: Get terrain hit location (ECC_Visibility) ===
	FVector WorldLocation;
	if (!GetWorldLocationUnderCursor(WorldLocation))
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Ray miss - no hit under cursor"));
		return;
	}

	// === Step 2: Try ECC_Pawn channel first to detect drone capsules directly ===
	AActor* DroneActor = nullptr;
	FHitResult PawnHit;
	if (GetHitResultUnderCursor(ECC_Pawn, false, PawnHit))
	{
		AActor* PawnActor = PawnHit.GetActor();
		if (PawnActor && PawnActor->GetClass()->ImplementsInterface(UDroneSelectableInterface::StaticClass()))
		{
			DroneActor = PawnActor;
		}
	}

	// === Step 3: If Pawn trace missed, try spherical proximity search ===
	if (!DroneActor)
	{
		const float ClickRadius = 200.0f; // Detection radius around click point
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(GetPawn());
		
		GetWorld()->OverlapMultiByChannel(
			Overlaps,
			WorldLocation,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(ClickRadius),
			QueryParams
		);

		float BestDist = ClickRadius + 1.0f;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OverlapActor = Overlap.GetActor();
			if (OverlapActor && OverlapActor->GetClass()->ImplementsInterface(UDroneSelectableInterface::StaticClass()))
			{
				float Dist = FVector::Dist(WorldLocation, OverlapActor->GetActorLocation());
				if (Dist < BestDist)
				{
					BestDist = Dist;
					DroneActor = OverlapActor;
				}
			}
		}
	}

	// === Debug output ===
	if (GEngine)
	{
		if (DroneActor)
		{
			int32 Id = IDroneSelectableInterface::Execute_GetDroneId(DroneActor);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
				FString::Printf(TEXT("DRONE FOUND: %s (ID=%d)"), *DroneActor->GetName(), Id));
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
				FString::Printf(TEXT("Map click at (%.0f, %.0f, %.0f) - no drone nearby"),
					WorldLocation.X, WorldLocation.Y, WorldLocation.Z));
		}
	}

	// === Step 4: Dispatch ===
	if (DroneActor)
	{
		HandleDroneClick(DroneActor);
	}
	else
	{
		HandleMapClick(WorldLocation);
	}
}

void ADroneOpsPlayerController::OnShowInfo()
{
	if (HoveredDroneId > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Show info for DroneId: %d"), HoveredDroneId);
		// TODO: Open info panel widget
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
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("No DroneRegistry"));
		return;
	}

	int32 SelectedDroneId = DroneRegistry->GetPrimarySelectedDrone();
	if (SelectedDroneId <= 0)
	{
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("No drone selected - click a drone first"));
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
	if (!DroneRegistry || !ClickedActor)
	{
		return;
	}

	// Check if the actor implements IDroneSelectableInterface
	if (!ClickedActor->GetClass()->ImplementsInterface(UDroneSelectableInterface::StaticClass()))
	{
		UE_LOG(LogTemp, Verbose, TEXT("Clicked actor %s is not a selectable drone"), *ClickedActor->GetName());
		return;
	}

	int32 ClickedDroneId = IDroneSelectableInterface::Execute_GetDroneId(ClickedActor);
	if (ClickedDroneId <= 0)
	{
		return;
	}

	// Deselect previous primary drone
	int32 OldDroneId = DroneRegistry->GetPrimarySelectedDrone();
	if (OldDroneId > 0 && OldDroneId != ClickedDroneId)
	{
		AActor* OldActor = DroneRegistry->GetReceiverActor(OldDroneId);
		if (!OldActor)
		{
			OldActor = Cast<AActor>(DroneRegistry->GetSenderPawn(OldDroneId));
		}
		if (OldActor && OldActor->GetClass()->ImplementsInterface(UDroneSelectableInterface::StaticClass()))
		{
			IDroneSelectableInterface::Execute_OnDeselected(OldActor);
		}
	}

	// Check if drone is available for selection (not offline/lost)
	EDroneControlLockReason LockReason;
	if (DroneRegistry->IsControlLocked(ClickedDroneId, LockReason)
		&& LockReason == EDroneControlLockReason::Offline)
	{
		UE_LOG(LogTemp, Warning, TEXT("Drone %d is offline, cannot select"), ClickedDroneId);
		return;
	}

	// Set new primary selection
	DroneRegistry->SetPrimarySelectedDrone(ClickedDroneId);
	IDroneSelectableInterface::Execute_OnPrimarySelected(ClickedActor);

	FDroneDescriptor Desc;
	FString DisplayName = FString::Printf(TEXT("Drone-%d"), ClickedDroneId);
	if (DroneRegistry->GetDroneDescriptor(ClickedDroneId, Desc))
	{
		DisplayName = Desc.Name;
	}

	UE_LOG(LogTemp, Log, TEXT("Primary selected: %s (ID=%d)"), *DisplayName, ClickedDroneId);
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

	// Create command record
	FDroneTargetCommand Command;
	Command.DroneId = DroneId;
	Command.TargetWorldLocation = TargetWorldLocation;
	Command.TargetNedLocation = NedLocation;
	Command.Mode = 1; // 1 = move to target
	Command.IssuedAt = GetWorld()->GetTimeSeconds();

	UE_LOG(LogTemp, Log, TEXT("Target command for Drone %d: World=(%.1f, %.1f, %.1f) NED=(%.2f, %.2f, %.2f)"),
		DroneId,
		TargetWorldLocation.X, TargetWorldLocation.Y, TargetWorldLocation.Z,
		NedLocation.X, NedLocation.Y, NedLocation.Z);

	// Find the DroneCommandSenderComponent on MultiDroneManager
	UDroneCommandSenderComponent* CommandSender = nullptr;
	TArray<AActor*> Managers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMultiDroneManager::StaticClass(), Managers);
	if (Managers.Num() > 0)
	{
		CommandSender = Managers[0]->FindComponentByClass<UDroneCommandSenderComponent>();
	}

	if (CommandSender)
	{
		CommandSender->SendSingleDroneCommand(DroneId, NedLocation, Command.Mode);
		UE_LOG(LogTemp, Log, TEXT("Command dispatched via DroneCommandSenderComponent for Drone %d"), DroneId);
	}
	else
	{
		// Fallback: try to send via the sender pawn's legacy SetClickTargetLocation
		APawn* SenderPawn = DroneRegistry->GetSenderPawn(DroneId);
		if (SenderPawn)
		{
			AUE5DroneControlCharacter* DroneChar = Cast<AUE5DroneControlCharacter>(SenderPawn);
			if (DroneChar)
			{
				DroneChar->SetClickTargetLocation(TargetWorldLocation, Command.Mode);
				UE_LOG(LogTemp, Log, TEXT("Command dispatched via legacy SetClickTargetLocation for Drone %d"), DroneId);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("No command sender available for Drone %d"), DroneId);
		}
	}
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
