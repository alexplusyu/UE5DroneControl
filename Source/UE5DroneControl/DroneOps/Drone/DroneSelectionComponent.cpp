// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneOps/Drone/DroneSelectionComponent.h"

UDroneSelectionComponent::UDroneSelectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneSelectionComponent::SetPrimarySelected(bool bSelected)
{
	bool bChanged = (bIsPrimarySelected != bSelected);
	bIsPrimarySelected = bSelected;
	if (bChanged)
	{
		BroadcastIfChanged();
	}
}

void UDroneSelectionComponent::SetSecondarySelected(bool bSelected)
{
	bool bChanged = (bIsSecondarySelected != bSelected);
	bIsSecondarySelected = bSelected;
	if (bChanged)
	{
		BroadcastIfChanged();
	}
}

void UDroneSelectionComponent::SetHovered(bool bHovered)
{
	bIsHovered = bHovered;
	// Hover does not fire OnSelectionStateChanged (it's a visual-only hint)
}

void UDroneSelectionComponent::BroadcastIfChanged()
{
	OnSelectionStateChanged.Broadcast(bIsPrimarySelected, bIsSecondarySelected);
}
