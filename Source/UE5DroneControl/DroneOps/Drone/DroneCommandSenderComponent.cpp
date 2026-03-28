// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneOps/Drone/DroneCommandSenderComponent.h"
#include "DroneOps/Core/DroneRegistrySubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

// Wire format: 32 bytes, little-endian
#pragma pack(push, 1)
struct FMultiDroneUDPPacket
{
	double  Timestamp;  // 8
	float   X;          // 4  NED north [m]
	float   Y;          // 4  NED east  [m]
	float   Z;          // 4  NED down  [m]
	int32   Mode;       // 4
	int32   DroneMask;  // 4
	int32   Sequence;   // 4
	// total = 32
};
#pragma pack(pop)

UDroneCommandSenderComponent::UDroneCommandSenderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneCommandSenderComponent::BeginPlay()
{
	Super::BeginPlay();
	InitSocket();
}

void UDroneCommandSenderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseSocket();
	Super::EndPlay(EndPlayReason);
}

bool UDroneCommandSenderComponent::InitSocket()
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSub)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneCommandSender: No socket subsystem"));
		return false;
	}

	SenderSocket = SocketSub->CreateSocket(NAME_DGram, TEXT("DroneCommandSender"), false);
	if (!SenderSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneCommandSender: Failed to create UDP socket"));
		return false;
	}

	RemoteAddr = SocketSub->CreateInternetAddr();
	bool bIsValid = false;
	RemoteAddr->SetIp(*UnifiedControllerIP, bIsValid);
	RemoteAddr->SetPort(UnifiedControllerPort);

	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("DroneCommandSender: Invalid IP address: %s"), *UnifiedControllerIP);
		CloseSocket();
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("DroneCommandSender: Socket ready -> %s:%d"), *UnifiedControllerIP, UnifiedControllerPort);
	return true;
}

void UDroneCommandSenderComponent::CloseSocket()
{
	if (SenderSocket)
	{
		ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSub)
		{
			SocketSub->DestroySocket(SenderSocket);
		}
		SenderSocket = nullptr;
	}
}

bool UDroneCommandSenderComponent::SendPacket(const FMultiDroneControlPacket& Packet)
{
	if (!SenderSocket || !RemoteAddr.IsValid())
	{
		return false;
	}

	// Throttle
	const double Now = FPlatformTime::Seconds();
	if (MinSendIntervalSec > 0.0f && (Now - LastSendTime) < MinSendIntervalSec)
	{
		return false;
	}
	LastSendTime = Now;

	FMultiDroneUDPPacket Wire;
	Wire.Timestamp = Packet.Timestamp;
	Wire.X         = Packet.X;
	Wire.Y         = Packet.Y;
	Wire.Z         = Packet.Z;
	Wire.Mode      = Packet.Mode;
	Wire.DroneMask = Packet.DroneMask;
	Wire.Sequence  = Packet.Sequence;

	static_assert(sizeof(FMultiDroneUDPPacket) == 32, "Packet size must be 32 bytes");

	int32 BytesSent = 0;
	const bool bOk = SenderSocket->SendTo(
		reinterpret_cast<const uint8*>(&Wire),
		sizeof(Wire),
		BytesSent,
		*RemoteAddr
	);

	if (!bOk || BytesSent != sizeof(Wire))
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneCommandSender: SendTo failed (sent %d / %d bytes)"), BytesSent, (int32)sizeof(Wire));
		return false;
	}

	LastPacket = Packet;
	return true;
}

void UDroneCommandSenderComponent::SendSingleDroneCommand(int32 DroneId, FVector NedTarget, int32 Mode)
{
	if (DroneId <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneCommandSender: Invalid DroneId %d"), DroneId);
		return;
	}

	// Look up BitIndex from registry
	UDroneRegistrySubsystem* Registry = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			Registry = GI->GetSubsystem<UDroneRegistrySubsystem>();
		}
	}

	int32 BitIndex = 0;
	if (Registry)
	{
		BitIndex = Registry->GetDroneBitIndex(DroneId);
	}

	if (BitIndex < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DroneCommandSender: No BitIndex for DroneId %d"), DroneId);
		return;
	}

	const int32 Mask = (1 << BitIndex);
	SendMultiDroneCommand(Mask, NedTarget, Mode);
}

void UDroneCommandSenderComponent::SendMultiDroneCommand(int32 DroneMask, FVector NedTarget, int32 Mode)
{
	if (!SenderSocket && !InitSocket())
	{
		return;
	}

	FMultiDroneControlPacket Packet;
	Packet.Timestamp  = FPlatformTime::Seconds();
	Packet.X          = NedTarget.X;
	Packet.Y          = NedTarget.Y;
	Packet.Z          = NedTarget.Z;
	Packet.Mode       = Mode;
	Packet.DroneMask  = DroneMask;
	Packet.Sequence   = ++Sequence;

	if (SendPacket(Packet))
	{
		UE_LOG(LogTemp, Verbose, TEXT("DroneCommandSender: Sent mask=0x%X NED=(%.2f,%.2f,%.2f) Mode=%d Seq=%d"),
			DroneMask, NedTarget.X, NedTarget.Y, NedTarget.Z, Mode, Sequence);
	}
}

void UDroneCommandSenderComponent::SendHoverCommand(int32 DroneId)
{
	// Look up current NED position from registry, then send Mode=0
	UDroneRegistrySubsystem* Registry = nullptr;
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			Registry = GI->GetSubsystem<UDroneRegistrySubsystem>();
		}
	}

	FVector HoverNed = FVector::ZeroVector;
	if (Registry)
	{
		FDroneTelemetrySnapshot Snap;
		if (Registry->GetTelemetry(DroneId, Snap))
		{
			HoverNed = Snap.NedLocation;
		}
	}

	SendSingleDroneCommand(DroneId, HoverNed, 0);
}
