// Copyright 2019 Dan Kestranek.


#include "GDHeroCharacter.h"
#include "Abilities/AttributeSets/GDAttributeSetBase.h"
#include "AI/GDHeroAIController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GASDocumentationGameMode.h"
#include "GDAbilitySystemComponent.h"
#include "GDPlayerController.h"
#include "GDPlayerState.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "UI/GDFloatingStatusBarWidget.h"
#include "UObject/ConstructorHelpers.h"
#include "WidgetComponent.h"
#include "GDThrowableProjectile.h"

AGDHeroCharacter::AGDHeroCharacter(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(FName("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->SetRelativeLocation(FVector(0, 0, 68.492264));

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(FName("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom);
	FollowCamera->FieldOfView = 80.0f;

	GunComponent = CreateDefaultSubobject<USkeletalMeshComponent>(FName("Gun"));

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	// Makes sure that the animations play on the Server so that we can use bone and socket transforms
	// to do things like spawning projectiles and other FX.
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(FName("NoCollision"));

	UIFloatingStatusBarComponent = CreateDefaultSubobject<UWidgetComponent>(FName("UIFloatingStatusBarComponent"));
	UIFloatingStatusBarComponent->SetupAttachment(RootComponent);
	UIFloatingStatusBarComponent->SetRelativeLocation(FVector(0, 0, 120));
	UIFloatingStatusBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
	UIFloatingStatusBarComponent->SetDrawSize(FVector2D(500, 500));

	UIFloatingStatusBarClass = StaticLoadClass(UObject::StaticClass(), nullptr, TEXT("/Game/GASDocumentation/UI/UI_FloatingStatusBar_Hero.UI_FloatingStatusBar_Hero_C"));
	if (!UIFloatingStatusBarClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s() Failed to find UIFloatingStatusBarClass. If it was moved, please update the reference location in C++."), TEXT(__FUNCTION__));
	}

	AIControllerClass = AGDHeroAIController::StaticClass();

	DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"));
}

// Called to bind functionality to input
void AGDHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &AGDHeroCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveBackward", this, &AGDHeroCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AGDHeroCharacter::MoveRight);
	PlayerInputComponent->BindAxis("MoveLeft", this, &AGDHeroCharacter::MoveRight);

	PlayerInputComponent->BindAxis("LookUp", this, &AGDHeroCharacter::LookUp);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AGDHeroCharacter::LookUpRate);
	PlayerInputComponent->BindAxis("Turn", this, &AGDHeroCharacter::Turn);
	PlayerInputComponent->BindAxis("TurnRate", this, &AGDHeroCharacter::TurnRate);

	CachedInputComponent = PlayerInputComponent;

	if (AbilitySystemComponent)
	{
		// Bind to AbilitySystemComponent
		AbilitySystemComponent->BindAbilityActivationToInputComponent(CachedInputComponent, FGameplayAbilityInputBinds(FString("ConfirmTarget"),
			FString("CancelTarget"), FString("EGDAbilityInputID"), static_cast<int32>(EGDAbilityInputID::Confirm), static_cast<int32>(EGDAbilityInputID::Cancel)));
	}
}

// Server only
void AGDHeroCharacter::PossessedBy(AController * NewController)
{
	Super::PossessedBy(NewController);

	AGDPlayerState* PS = GetPlayerState<AGDPlayerState>();
	if (PS)
	{
		// Set the ASC on the Server. Clients do this in OnRep_PlayerState()
		AbilitySystemComponent = Cast<UGDAbilitySystemComponent>(PS->GetAbilitySystemComponent());

		// AI won't have PlayerControllers so we can init again here just to be sure. No harm in initing twice for heroes that have PlayerControllers.
		PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

		// Set the AttributeSetBase for convenience attribute functions
		AttributeSetBase = PS->GetAttributeSetBase();

		// If we handle players disconnecting and rejoining in the future, we'll have to change this so that possession from rejoining doesn't reset attributes.
		// For now assume possession = spawn/respawn.
		InitializeAttributes();

		AddStartupEffects();

		AddCharacterAbilities();

		AGDPlayerController* PC = Cast<AGDPlayerController>(GetController());
		if (PC)
		{
			PC->CreateHUD();
		}

		InitializeFloatingStatusBar();


		// Respawn specific things that won't affect first possession.

		// Forcibly set the DeadTag count to 0
		if (AbilitySystemComponent)
		{
			AbilitySystemComponent->SetTagMapCount(DeadTag, 0);
		}

		// Set Health/Mana/Stamina to their max. This is only necessary for *Respawn*.
		SetHealth(GetMaxHealth());
		SetMana(GetMaxMana());
		SetStamina(GetMaxStamina());
	}
}

USpringArmComponent * AGDHeroCharacter::GetCameraBoom()
{
	return CameraBoom;
}

UCameraComponent * AGDHeroCharacter::GetFollowCamera()
{
	return FollowCamera;
}

float AGDHeroCharacter::GetStartingCameraBoomArmLength()
{
	return StartingCameraBoomArmLength;
}

FVector AGDHeroCharacter::GetStartingCameraBoomLocation()
{
	return StartingCameraBoomLocation;
}

void AGDHeroCharacter::LaunchPickedupProjectile(const TArray<FGameplayEffectSpecHandle>& EffectHandles)
{
	if (Role < ROLE_Authority || !HasPickedupAnyProjectiles())
	{
		return;
	}

	IPickupable* SelectedPickup = PickedupProjectiles.Pop();

	if (SelectedPickup && GetGunComponent())
	{
		FTransform MuzzleTransform = GetGunComponent()->GetSocketTransform(FName("Muzzle"));
		SelectedPickup->Throw(MuzzleTransform.GetLocation(), UKismetMathLibrary::GetForwardVector(MuzzleTransform.GetRotation().Rotator()), EffectHandles);
	}
}

bool AGDHeroCharacter::HasPickedupAnyProjectiles() const
{
	return PickedupProjectiles.Num() > 0;
}

TArray<TSubclassOf<UGameplayEffect>> AGDHeroCharacter::GetCurrentProjectileEffects() const
{
	if (PickedupProjectiles.Num() > 0)
	{
		if (AGDThrowableProjectile* ThrowableProjectile{ Cast<AGDThrowableProjectile>(PickedupProjectiles.Top()) })
		{
			return ThrowableProjectile->ProjectileEffects;
		}
	}

	return TArray<TSubclassOf<UGameplayEffect>>();
}

float AGDHeroCharacter::GetCurrentProjectileDamage() const
{
	if (PickedupProjectiles.Num() > 0)
	{
		if (AGDThrowableProjectile* ThrowableProjectile{ Cast<AGDThrowableProjectile>(PickedupProjectiles.Top()) })
		{
			return ThrowableProjectile->Damage;
		}
	}

	return 0.0f;
}

void AGDHeroCharacter::PickupProjectile(IPickupable* PickedupItem)
{
	if (Role < ROLE_Authority)
	{
		return;
	}

	if (PickedupItem)
	{
		PickedupProjectiles.Add(PickedupItem);
	}
}

UGDFloatingStatusBarWidget * AGDHeroCharacter::GetFloatingStatusBar()
{
	return UIFloatingStatusBar;
}

USkeletalMeshComponent * AGDHeroCharacter::GetGunComponent() const
{
	return GunComponent;
}

void AGDHeroCharacter::FinishDying()
{
	if (Role == ROLE_Authority)
	{
		AGASDocumentationGameMode* GM = Cast<AGASDocumentationGameMode>(GetWorld()->GetAuthGameMode());

		if (GM)
		{
			GM->HeroDied(GetController());
		}
	}

	Super::FinishDying();
}

/**
* On the Server, Possession happens before BeginPlay.
* On the Client, BeginPlay happens before Possession.
* So we can't use BeginPlay to do anything with the AbilitySystemComponent because we don't have it until the PlayerState replicates from possession.
*/
void AGDHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Only needed for Heroes placed in world and when the player is the Server.
	// On respawn, they are set up in PossessedBy.
	// When the player a client, the floating status bars are all set up in OnRep_PlayerState.
	InitializeFloatingStatusBar();

	GunComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("GunSocket"));

	StartingCameraBoomArmLength = CameraBoom->TargetArmLength;
	StartingCameraBoomLocation = CameraBoom->RelativeLocation;
}

void AGDHeroCharacter::LookUp(float Value)
{
	if (IsAlive())
	{
		AddControllerPitchInput(Value);
	}
}

void AGDHeroCharacter::LookUpRate(float Value)
{
	if (IsAlive())
	{
		AddControllerPitchInput(Value * BaseLookUpRate * GetWorld()->DeltaTimeSeconds);
	}
}

void AGDHeroCharacter::Turn(float Value)
{
	if (IsAlive())
	{
		AddControllerYawInput(Value);
	}
}

void AGDHeroCharacter::TurnRate(float Value)
{
	if (IsAlive())
	{
		AddControllerYawInput(Value * BaseTurnRate * GetWorld()->DeltaTimeSeconds);
	}
}

void AGDHeroCharacter::MoveForward(float Value)
{
	AddMovementInput(UKismetMathLibrary::GetForwardVector(FRotator(0, GetControlRotation().Yaw, 0)), Value);
}

void AGDHeroCharacter::MoveRight(float Value)
{
	AddMovementInput(UKismetMathLibrary::GetRightVector(FRotator(0, GetControlRotation().Yaw, 0)), Value);
}

void AGDHeroCharacter::InitializeFloatingStatusBar()
{
	// Only create once
	if (UIFloatingStatusBar || !AbilitySystemComponent)
	{
		return;
	}

	// Setup UI for Locally Owned Players only, not AI or the server's copy of the PlayerControllers
	AGDPlayerController* PC = Cast<AGDPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (PC && PC->IsLocalPlayerController())
	{
		if (UIFloatingStatusBarClass)
		{
			UIFloatingStatusBar = CreateWidget<UGDFloatingStatusBarWidget>(PC, UIFloatingStatusBarClass);
			if (UIFloatingStatusBar && UIFloatingStatusBarComponent)
			{
				UIFloatingStatusBarComponent->SetWidget(UIFloatingStatusBar);

				// Setup the floating status bar
				UIFloatingStatusBar->SetHealthPercentage(GetHealth() / GetMaxHealth());
				UIFloatingStatusBar->SetManaPercentage(GetMana() / GetMaxMana());
			}
		}
	}
}

// Client only
void AGDHeroCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	AGDPlayerState* PS = GetPlayerState<AGDPlayerState>();
	if (PS)
	{
		// Set the ASC for clients. Server does this in PossessedBy.
		AbilitySystemComponent = Cast<UGDAbilitySystemComponent>(PS->GetAbilitySystemComponent());

		if (CachedInputComponent)
		{
			SetupPlayerInputComponent(CachedInputComponent);
		}

		if (AbilitySystemComponent)
		{
			// Refresh ASC Actor Info for clients. Server will be refreshed by its AI/PlayerController when it possesses a new Actor.
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}

		// Set the AttributeSetBase for convenience attribute functions
		AttributeSetBase = PS->GetAttributeSetBase();

		// If we handle players disconnecting and rejoining in the future, we'll have to change this so that posession from rejoining doesn't reset attributes.
		// For now assume possession = spawn/respawn.
		InitializeAttributes();

		AGDPlayerController* PC = Cast<AGDPlayerController>(GetController());
		if (PC)
		{
			PC->CreateHUD();
		}

		// Simulated on proxies don't have their PlayerStates yet when BeginPlay is called so we call it again here
		InitializeFloatingStatusBar();


		// Respawn specific things that won't affect first possession.

		if (AbilitySystemComponent)
		{
			// Forcibly set the DeadTag count to 0
			AbilitySystemComponent->SetTagMapCount(DeadTag, 0);
		}

		// Set Health/Mana/Stamina to their max. This is only necessary for *Respawn*.
		SetHealth(GetMaxHealth());
		SetMana(GetMaxMana());
		SetStamina(GetMaxStamina());
	}
}
