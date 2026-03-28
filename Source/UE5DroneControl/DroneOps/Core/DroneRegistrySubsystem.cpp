// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneRegistrySubsystem.h"
#include "ICoordinateService.h"
#include "GameFramework/Pawn.h"

UDroneRegistrySubsystem::UDroneRegistrySubsystem()
{
}

void UDroneRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("DroneRegistrySubsystem initialized"));
}

void UDroneRegistrySubsystem::Deinitialize()
{
	DroneDescriptors.Empty();
	SenderPawns.Empty();
	ReceiverActors.Empty();
	TelemetryCache.Empty();
	ControlLocks.Empty();

	Super::Deinitialize();
}

void UDroneRegistrySubsystem::SetCoordinateService(TScriptInterface<ICoordinateService> Service)
{
	CoordinateService = Service;
	UE_LOG(LogTemp, Log, TEXT("Coordinate service set"));
}

TScriptInterface<ICoordinateService> UDroneRegistrySubsystem::GetCoordinateService() const
{
	return CoordinateService;
}

void UDroneRegistrySubsystem::RegisterDrone(const FDroneDescriptor& Descriptor)
{
	if (Descriptor.DroneId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid DroneId: %d"), Descriptor.DroneId);
		return;
	}

	DroneDescriptors.Add(Descriptor.DroneId, Descriptor);

	UE_LOG(LogTemp, Log, TEXT("Drone registered: %s (ID=%d, BitIndex=%d)"),
		*Descriptor.Name, Descriptor.DroneId, Descriptor.BitIndex);

	OnDroneRegistered.Broadcast(Descriptor.DroneId);
}

void UDroneRegistrySubsystem::RegisterSenderPawn(int32 DroneId, APawn* PawnRef)
{
	if (!PawnRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("Null pawn for DroneId: %d"), DroneId);
		return;
	}

	SenderPawns.Add(DroneId, PawnRef);
	UE_LOG(LogTemp, Log, TEXT("Sender pawn registered for DroneId: %d"), DroneId);
}

void UDroneRegistrySubsystem::RegisterReceiverActor(int32 DroneId, AActor* ReceiverRef)
{
	if (!ReceiverRef)
	{
		UE_LOG(LogTemp, Warning, TEXT("Null receiver for DroneId: %d"), DroneId);
		return;
	}

	ReceiverActors.Add(DroneId, ReceiverRef);
	UE_LOG(LogTemp, Log, TEXT("Receiver actor registered for DroneId: %d"), DroneId);
}

void UDroneRegistrySubsystem::UnregisterDrone(int32 DroneId)
{
	DroneDescriptors.Remove(DroneId);
	SenderPawns.Remove(DroneId);
	ReceiverActors.Remove(DroneId);
	TelemetryCache.Remove(DroneId);
	ControlLocks.Remove(DroneId);

	UE_LOG(LogTemp, Log, TEXT("Drone unregistered: %d"), DroneId);
}

void UDroneRegistrySubsystem::UpdateTelemetry(int32 DroneId, const FDroneTelemetrySnapshot& Snapshot)
{
	TelemetryCache.Add(DroneId, Snapshot);
	OnTelemetryUpdated.Broadcast(DroneId, Snapshot);
}

bool UDroneRegistrySubsystem::GetTelemetry(int32 DroneId, FDroneTelemetrySnapshot& OutSnapshot) const
{
	const FDroneTelemetrySnapshot* Found = TelemetryCache.Find(DroneId);
	if (Found)
	{
		OutSnapshot = *Found;
		return true;
	}
	return false;
}

void UDroneRegistrySubsystem::SetPrimarySelectedDrone(int32 DroneId)
{
	int32 OldDroneId = PrimarySelectedDroneId;
	PrimarySelectedDroneId = DroneId;

	// Update multi-selection to include primary
	if (DroneId > 0 && !MultiSelectedDroneIds.Contains(DroneId))
	{
		MultiSelectedDroneIds.Empty();
		MultiSelectedDroneIds.Add(DroneId);
	}

	UE_LOG(LogTemp, Log, TEXT("Primary selection changed: %d -> %d"), OldDroneId, DroneId);
	OnPrimarySelectionChanged.Broadcast(OldDroneId, DroneId);
}

void UDroneRegistrySubsystem::SetMultiSelectedDrones(const TArray<int32>& DroneIds)
{
	MultiSelectedDroneIds = DroneIds;

	// Update primary to first in list if not already selected
	if (DroneIds.Num() > 0 && !DroneIds.Contains(PrimarySelectedDroneId))
	{
		SetPrimarySelectedDrone(DroneIds[0]);
	}
}

void UDroneRegistrySubsystem::ClearSelection()
{
	int32 OldDroneId = PrimarySelectedDroneId;
	PrimarySelectedDroneId = 0;
	MultiSelectedDroneIds.Empty();

	OnPrimarySelectionChanged.Broadcast(OldDroneId, 0);
}

void UDroneRegistrySubsystem::ApplyControlLock(int32 DroneId, EDroneControlLockReason LockReason)
{
	ControlLocks.Add(DroneId, LockReason);
	UE_LOG(LogTemp, Log, TEXT("Control lock applied to DroneId %d: %d"), DroneId, (int32)LockReason);
	OnControlLockChanged.Broadcast(DroneId, LockReason, true);
}

void UDroneRegistrySubsystem::ReleaseControlLock(int32 DroneId)
{
	if (ControlLocks.Contains(DroneId))
	{
		ControlLocks.Remove(DroneId);
		UE_LOG(LogTemp, Log, TEXT("Control lock released for DroneId: %d"), DroneId);
		OnControlLockChanged.Broadcast(DroneId, EDroneControlLockReason::None, false);
	}
}

bool UDroneRegistrySubsystem::IsControlLocked(int32 DroneId, EDroneControlLockReason& OutReason) const
{
	const EDroneControlLockReason* Found = ControlLocks.Find(DroneId);
	if (Found)
	{
		OutReason = *Found;
		return true;
	}
	OutReason = EDroneControlLockReason::None;
	return false;
}

int32 UDroneRegistrySubsystem::GetSelectedDroneMask() const
{
	uint32 Mask = 0;

	for (int32 DroneId : MultiSelectedDroneIds)
	{
		const FDroneDescriptor* Descriptor = DroneDescriptors.Find(DroneId);
		if (Descriptor && Descriptor->BitIndex >= 0 && Descriptor->BitIndex < 32)
		{
			Mask |= (1u << Descriptor->BitIndex);
		}
	}

	return static_cast<int32>(Mask);
}

int32 UDroneRegistrySubsystem::GetDroneBitIndex(int32 DroneId) const
{
	const FDroneDescriptor* Descriptor = DroneDescriptors.Find(DroneId);
	return Descriptor ? Descriptor->BitIndex : -1;
}

int32 UDroneRegistrySubsystem::GetDroneByBitIndex(int32 BitIndex) const
{
	for (const auto& Pair : DroneDescriptors)
	{
		if (Pair.Value.BitIndex == BitIndex)
		{
			return Pair.Key;
		}
	}
	return 0;
}

TArray<FDroneDescriptor> UDroneRegistrySubsystem::GetAllDroneDescriptors() const
{
	TArray<FDroneDescriptor> Result;
	DroneDescriptors.GenerateValueArray(Result);
	return Result;
}

bool UDroneRegistrySubsystem::GetDroneDescriptor(int32 DroneId, FDroneDescriptor& OutDescriptor) const
{
	const FDroneDescriptor* Found = DroneDescriptors.Find(DroneId);
	if (Found)
	{
		OutDescriptor = *Found;
		return true;
	}
	return false;
}

APawn* UDroneRegistrySubsystem::GetSenderPawn(int32 DroneId) const
{
	const TObjectPtr<APawn>* Found = SenderPawns.Find(DroneId);
	return Found ? Found->Get() : nullptr;
}

AActor* UDroneRegistrySubsystem::GetReceiverActor(int32 DroneId) const
{
	const TObjectPtr<AActor>* Found = ReceiverActors.Find(DroneId);
	return Found ? Found->Get() : nullptr;
}

bool UDroneRegistrySubsystem::IsDroneRegistered(int32 DroneId) const
{
	return DroneDescriptors.Contains(DroneId);
}
