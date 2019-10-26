// Copyright 2019 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include <GameplayEffectTypes.h>
#include "Pickupable.generated.h"

UENUM(BlueprintType)
enum class EPickupState : uint8
{
	OnGround,
	PickedUp,
	Thrown
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UPickupable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GASDOCUMENTATION_API IPickupable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	virtual void Pickedup(class AGDHeroCharacter* PickedupBy) = 0;
	virtual void Throw(const FVector& StartLocation, const FVector& ForwardDirection, const TArray<FGameplayEffectSpecHandle>& EffectHandles) = 0;
};
