// Copyright 2019 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "Characters/GDProjectile.h"
#include "Pickupable.h"
#include "GDThrowableProjectile.generated.h"

UCLASS()
class GASDOCUMENTATION_API AGDThrowableProjectile : public AGDProjectile, public IPickupable
{
	GENERATED_BODY()
	
public:
	AGDThrowableProjectile();

	virtual void Pickedup(AGDHeroCharacter* PickedupBy) override;
	virtual void Throw(const FVector& StartLocation, const FVector& ForwardDirection, const TArray<FGameplayEffectSpecHandle>& EffectHandles) override;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	class USphereComponent* CollisionComponent;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere)
	class UStaticMeshComponent* MeshComponent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<TSubclassOf<UGameplayEffect>> ProjectileEffects;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Damage;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	class UParticleSystem* CollisionParticles;

protected:

	UFUNCTION()
	void DamageCharacter(AActor* OtherCharacter);

	UPROPERTY()
	EPickupState PickupState;

	UPROPERTY()
	TArray<FGameplayEffectSpecHandle> GameplayEffectHandles;

	UFUNCTION()
	void OnCollisionOccured(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(NetMulticast, Reliable)
	void SpawnEmitterEffect(const FVector& AtLocation);
};
