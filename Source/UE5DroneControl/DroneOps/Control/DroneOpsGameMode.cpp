// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneOpsGameMode.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "DroneOps/Core/SimpleCoordinateService.h"
#include "DroneOpsPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MultiDroneCharacter.h"
#include "EngineUtils.h"

ADroneOpsGameMode::ADroneOpsGameMode()
{
	// Set default player controller class
	PlayerControllerClass = ADroneOpsPlayerController::StaticClass();

	// Do not auto-spawn a default pawn. We only use pawns already placed in the level.
	DefaultPawnClass = nullptr;
}

void ADroneOpsGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("DroneOpsGameMode: BeginPlay"));

	// Initialize coordinate service
	InitializeCoordinateService();

	// Initialize drone registry
	InitializeDroneRegistry();
}

void ADroneOpsGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	PossessPlacedPawn(NewPlayer);
}

APawn* ADroneOpsGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	return nullptr;
}

void ADroneOpsGameMode::InitializeCoordinateService()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneOpsGameMode: No GameInstance found"));
		return;
	}

	UDroneRegistrySubsystem* Registry = GameInstance->GetSubsystem<UDroneRegistrySubsystem>();
	if (!Registry)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneOpsGameMode: DroneRegistrySubsystem not found"));
		return;
	}

	// For now, always use SimpleCoordinateService (Cesium will be added later)
	USimpleCoordinateService* CoordService = NewObject<USimpleCoordinateService>(this);
	if (CoordService)
	{
		Registry->SetCoordinateService(CoordService);
		UE_LOG(LogTemp, Log, TEXT("DroneOpsGameMode: SimpleCoordinateService initialized"));
	}
}

void ADroneOpsGameMode::InitializeDroneRegistry()
{
	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	UDroneRegistrySubsystem* Registry = GameInstance->GetSubsystem<UDroneRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}

	// Registry is ready for drone registration
	// Drones will be registered by MultiDroneManager or individual actors
	UE_LOG(LogTemp, Log, TEXT("DroneOpsGameMode: DroneRegistry ready for registration"));
}

APawn* ADroneOpsGameMode::FindUnpossessedPlacedPawn() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AMultiDroneCharacter> It(World); It; ++It)
	{
		AMultiDroneCharacter* DronePawn = *It;
		if (!IsValid(DronePawn) || DronePawn->IsPendingKillPending())
		{
			continue;
		}

		if (DronePawn->Controller == nullptr)
		{
			return DronePawn;
		}
	}

	return nullptr;
}

void ADroneOpsGameMode::PossessPlacedPawn(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	if (PlayerController->GetPawn())
	{
		return;
	}

	if (APawn* ExistingPawn = FindUnpossessedPlacedPawn())
	{
		PlayerController->Possess(ExistingPawn);
		UE_LOG(LogTemp, Log, TEXT("DroneOpsGameMode: Possessed placed pawn %s"), *ExistingPawn->GetName());
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("DroneOpsGameMode: No unpossessed placed AMultiDroneCharacter found for %s"), *PlayerController->GetName());
}
