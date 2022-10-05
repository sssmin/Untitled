#include "MeleeCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "ToughSword.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interactable.h"
#include "MeleeAnimInstance.h"

//////////////////////////////////////////////////////////////////////////
// AMeleeCharacter

AMeleeCharacter::AMeleeCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	TurnRateGamepad = 50.f;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); 
	FollowCamera->bUsePawnControlRotation = false; 

	EquippedWeapon = nullptr;
	bCombatEnabled = false;
}

void AMeleeCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMeleeCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("ToggleCombat", IE_Pressed, this, &ThisClass::ToggleCombat);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ThisClass::InteractButtonPressed);
	

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AMeleeCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AMeleeCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMeleeCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AMeleeCharacter::LookUpAtRate);

	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMeleeCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMeleeCharacter::TouchStopped);
}

void AMeleeCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AMeleeCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AMeleeCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMeleeCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMeleeCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMeleeCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void AMeleeCharacter::ToggleCombat()
{
	if(EquippedWeapon)
	{
		if(EquippedWeapon->GetEnterCombatAM() && EquippedWeapon->GetExitCombatAM())
		{
			if(!bCombatEnabled) //손에 장착 상태가 아니면
			{
				PlayAnimMontage(EquippedWeapon->GetEnterCombatAM());
				//EquippedWeapon->SetIsAttachedToHand(true);
				bCombatEnabled = true;
				if(GetMesh() && GetMesh()->GetAnimInstance())
				{
					Cast<UMeleeAnimInstance>(GetMesh()->GetAnimInstance())->SetCombatEnabled(true);
					
				}
			}
			else
			{
				PlayAnimMontage(EquippedWeapon->GetExitCombatAM());
				//EquippedWeapon->SetIsAttachedToHand(false);
				bCombatEnabled = false;
				if(GetMesh() && GetMesh()->GetAnimInstance())
				{
					Cast<UMeleeAnimInstance>(GetMesh()->GetAnimInstance())->SetCombatEnabled(false);
					
				}
			}
		}
	}
}

void AMeleeCharacter::InteractButtonPressed()
{
	FVector Start = GetActorLocation();
	FVector End = Start + GetActorForwardVector() * 100.f;
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypesArray;
	ObjectTypesArray.Reserve(1);
	ObjectTypesArray.Emplace(ECollisionChannel::ECC_GameTraceChannel1);
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this);
	FHitResult OutHit;
	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this, 
		Start, 
		End, 
		30.f, 
		ObjectTypesArray, 
		false, 
		IgnoredActors, 
		EDrawDebugTrace::ForDuration, 
		OutHit, 
		true,
		FLinearColor::Red,
		FLinearColor::Green,
		10.f);

		
		if(OutHit.GetActor())
		{
			IInteractable* Interactable = Cast<IInteractable>(OutHit.GetActor());
			if(Interactable)
			{
				Interactable->Interact(this);
			}
		}
		
}

void AMeleeCharacter::SetEquippedWeapon(ABaseWeapon* NewWeapon)
{
	if(EquippedWeapon)
	{
		EquippedWeapon->OnUnequipped();
		EquippedWeapon->Destroy();
	}
	EquippedWeapon = NewWeapon;
}

