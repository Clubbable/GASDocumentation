// Copyright 2019 Dan Kestranek.


#include "GDThrowableProjectile.h"
#include "GDHeroCharacter.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GDCharacterBase.h"
#include "GDAbilitySystemComponent.h"
#include <Components/SphereComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Kismet/GameplayStatics.h>

AGDThrowableProjectile::AGDThrowableProjectile()
	:Damage(25.0f)
	,PickupState(EPickupState::OnGround)
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(FName("StaticMeshComponent"));
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(FName("CollisionComponent"));
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AGDThrowableProjectile::OnCollisionOccured);

	RootComponent = MeshComponent;
	CollisionComponent->SetupAttachment(RootComponent);
	MeshComponent->SetMobility(EComponentMobility::Movable);
}

void AGDThrowableProjectile::Pickedup(AGDHeroCharacter* PickedupBy)
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	if (PickedupBy)
	{
		PickedupBy->PickupProjectile(this);
		Instigator = PickedupBy;
		SetActorHiddenInGame(true);
		SetActorLocation(FVector::OneVector * 100000.0f);
		PickupState = EPickupState::PickedUp;
	}
}

void AGDThrowableProjectile::Throw(const FVector& StartLocation, const FVector& ForwardDirection, const TArray<FGameplayEffectSpecHandle>& EffectHandles)
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	GameplayEffectHandles = EffectHandles;

	SetActorHiddenInGame(false);
	SetActorLocation(StartLocation);

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = ForwardDirection * 1250.0f;
	}

	PickupState = EPickupState::Thrown;
}

void AGDThrowableProjectile::DamageCharacter(AActor* OtherCharacter)
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	if (AGDCharacterBase* GDCharacter{ Cast<AGDCharacterBase>(OtherCharacter) })
	{
		if (GDCharacter != Instigator && GDCharacter->IsAlive() && GDCharacter->GetAbilitySystemComponent())
		{
			for (const FGameplayEffectSpecHandle& EffectHandle : GameplayEffectHandles)
			{
				if (EffectHandle.IsValid())
				{
					GDCharacter->GetAbilitySystemComponent()->ApplyGameplayEffectSpecToSelf(*EffectHandle.Data.Get());
				}
			}
		}
	}

	Destroy();
}

void AGDThrowableProjectile::OnCollisionOccured(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	if (PickupState == EPickupState::OnGround)
	{
		if (AGDHeroCharacter* HeroInstigator{ Cast<AGDHeroCharacter>(OtherActor) })
		{
			Pickedup(HeroInstigator);
		}
	}
	else if(PickupState == EPickupState::Thrown)
	{
		if (OtherActor)
		{
			SpawnEmitterEffect(OtherActor->GetActorLocation());
		}

		DamageCharacter(OtherActor);
	}
}

void AGDThrowableProjectile::SpawnEmitterEffect_Implementation(const FVector& AtLocation)
{
	UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), CollisionParticles, AtLocation);
}