// Copyright 2019 Dan Kestranek.

#pragma once

#include "CoreMinimal.h"
#include "Characters/GDCharacterBase.h"
#include "Pickupable.h"
#include "GDHeroCharacter.generated.h"

/**
 * A player or AI controlled hero character.
 */
UCLASS()
class GASDOCUMENTATION_API AGDHeroCharacter : public AGDCharacterBase
{
	GENERATED_BODY()

public:
	AGDHeroCharacter(const class FObjectInitializer& ObjectInitializer);

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// Only called on the Server. Calls before Server's AcknowledgePossession.
	virtual void PossessedBy(AController* NewController) override;

	class USpringArmComponent* GetCameraBoom();

	class UCameraComponent* GetFollowCamera();

	UFUNCTION(BlueprintCallable, Category = "GASDocumentation|Camera")
	float GetStartingCameraBoomArmLength();

	UFUNCTION(BlueprintCallable, Category = "GASDocumentation|Camera")
	FVector GetStartingCameraBoomLocation();

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void LaunchPickedupProjectile(const TArray<FGameplayEffectSpecHandle>& EffectHandles);

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	bool HasPickedupAnyProjectiles() const;

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	TArray<TSubclassOf<UGameplayEffect>> GetCurrentProjectileEffects() const;

	UFUNCTION(BlueprintCallable, Category = "Projectile")
	float GetCurrentProjectileDamage() const;

	class UGDFloatingStatusBarWidget* GetFloatingStatusBar();

	USkeletalMeshComponent* GetGunComponent() const;

	virtual void FinishDying() override;

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASDocumentation|Camera")
	float BaseTurnRate = 45.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASDocumentation|Camera")
	float BaseLookUpRate = 45.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GASDocumentation|Camera")
	float StartingCameraBoomArmLength;

	UPROPERTY(BlueprintReadOnly, Category = "GASDocumentation|Camera")
	FVector StartingCameraBoomLocation;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "GASDocumentation|Camera")
	class USpringArmComponent* CameraBoom;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "GASDocumentation|Camera")
	class UCameraComponent* FollowCamera;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	USkeletalMeshComponent* GunComponent;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GASDocumentation|UI")
	TSubclassOf<class UGDFloatingStatusBarWidget> UIFloatingStatusBarClass;

	UPROPERTY()
	class UGDFloatingStatusBarWidget* UIFloatingStatusBar;

	TArray<IPickupable*> PickedupProjectiles;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "GASDocumentation|UI")
	class UWidgetComponent* UIFloatingStatusBarComponent;

	FGameplayTag DeadTag;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Mouse
	void LookUp(float Value);

	// Gamepad
	void LookUpRate(float Value);

	// Mouse
	void Turn(float Value);

	// Gamepad
	void TurnRate(float Value);

	// Mouse + Gamepad
	void MoveForward(float Value);

	// Mouse + Gamepad
	void MoveRight(float Value);

	// Creates and initializes the floating status bar for heroes.
	// Safe to call many times because it checks to make sure it only executes once.
	UFUNCTION()
	void InitializeFloatingStatusBar();

	// Client only
	virtual void OnRep_PlayerState() override;

	void PickupProjectile(IPickupable* PickedupItem);

private:

	friend class AGDThrowableProjectile;

	UPROPERTY()
	class UInputComponent* CachedInputComponent;
};
