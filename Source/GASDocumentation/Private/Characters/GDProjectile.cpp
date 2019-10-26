// Copyright 2019 Dan Kestranek.


#include "GDProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

// Sets default values
AGDProjectile::AGDProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bReplicateMovement = true;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(FName("ProjectileMovement"));
}

// Called when the game starts or when spawned
void AGDProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

