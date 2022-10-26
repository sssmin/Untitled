#include "BaseCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "MeleeAnimInstance.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystem.h"
#include "Components/WidgetComponent.h"

#include "../AttackDamageType.h"
#include "../Interface/Interactable.h"
#include "../Type/Stats.h"
#include "../Type/Types.h"
#include "../Equipment/BaseWeapon.h"
#include "../Component/CombatComponent.h"
#include "../Component/StateManagerComponent.h"
#include "../Component/StatsComponent.h"
#include "../Component/TargetingComponent.h"

#include "../Rock.h"


ABaseCharacter::ABaseCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

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

	CombatComp = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComp"));
	StateManagerComp = CreateDefaultSubobject<UStateManagerComponent>(TEXT("StateManagerComp"));
	StatComp = CreateDefaultSubobject<UStatsComponent>(TEXT("StatComp"));
	TargetingComp = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComp"));
	LockOnWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("LockOnWidget"));
	
	LockOnWidget->SetupAttachment(GetMesh());
	PelvisBoneName = TEXT("pelvis");
	DestroyDeadTime = 4.f;

	WalkSpeed = 300.f;
	JogSpeed = 500.f;
	SprintSpeed = 700;
	SprintStaminaCost = 0.2f;
	bSprintKeyPressed = false;
	MouseSensitivity = 25.f;
	bHitFront = false;

    ResetCombat();
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	ResetCombat();
	OnTakeAnyDamage.AddDynamic(this, &ABaseCharacter::ReceiveDamage);
	if(StateManagerComp)
	{
		StateManagerComp->OnStateBegin.AddUObject(this, &ThisClass::CharacterStateBegin);
	}
	
	if(StatComp)
	{
		StatComp->InitStats();
		StatComp->PlusCurrentStatValue(EStats::HP, 0.00000001f); //위젯 띄우는거, 스탯 초기화, 테이블 초기화 순서때문에 게임 시작시 스탯 비어보임을 가리는 꼼수
		StatComp->PlusCurrentStatValue(EStats::STAMINA, 0.00000001f);
	}

	if(LockOnWidget)
	{
		LockOnWidget->SetVisibility(false);
		LockOnWidget->SetWidgetSpace(EWidgetSpace::Screen);
		LockOnWidget->SetDrawSize(FVector2D(14.f, 14.f));
	}
	
}

void ABaseCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ThisClass::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
	PlayerInputComponent->BindAction("ToggleCombat", IE_Pressed, this, &ThisClass::ToggleCombat);
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ThisClass::InteractButtonPressed);
	PlayerInputComponent->BindAction("Dodge", IE_Pressed, this, &ThisClass::Dodge);
	PlayerInputComponent->BindAction("ToggleWalk", IE_Pressed, this, &ThisClass::ToggleWalk);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ThisClass::SprintButtonPressed);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ThisClass::SprintButtonReleased);
	PlayerInputComponent->BindAction("HeavyAttack", IE_Released, this, &ThisClass::HeavyAttack);
	PlayerInputComponent->BindAction("ToggleLockOn", IE_Released, this, &ThisClass::ToggleLockOn);
	

	PlayerInputComponent->BindAction("Test", IE_Pressed, this, &ThisClass::Test);

	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &ABaseCharacter::TurnRight);
	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &ABaseCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &ABaseCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &ABaseCharacter::LookUp);
	PlayerInputComponent->BindAxis("CameraZoomInOut", this, &ABaseCharacter::CameraZoomInOut);
	
	
}


void ABaseCharacter::TurnRight(float Rate)
{
	AddControllerYawInput(Rate * MouseSensitivity * GetWorld()->GetDeltaSeconds());
}

void ABaseCharacter::LookUp(float Rate)
{
	AddControllerPitchInput(Rate * MouseSensitivity * GetWorld()->GetDeltaSeconds());
}

void ABaseCharacter::Jump()
{
	TArray<ECurrentState> CharacterStates;
	CharacterStates.Add(ECurrentState::DODGING);
	CharacterStates.Add(ECurrentState::DISABLED);
	
	if(StateManagerComp && StateManagerComp->IsCurrentStateEqualToThis(CharacterStates) && StateManagerComp->GetCurrentAction() == ECurrentAction::CHARGED_ATTACK) return;
	StopAnimMontage();
	Super::Jump();
	ResetCombat();
}

void ABaseCharacter::MoveForward(float Value)
{
	if(StateManagerComp && StateManagerComp->GetCurrentState() == ECurrentState::ATTACKING) return;
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		if(bSprintKeyPressed) SetMovementType(EMovementType::SPRINTING);
		AddMovementInput(Direction, Value);
	}
}

void ABaseCharacter::MoveRight(float Value)
{
	if(StateManagerComp && StateManagerComp->GetCurrentState() == ECurrentState::ATTACKING) return;
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);
	}
}

void ABaseCharacter::ToggleCombat()
{
	if(!CanToggleCombat() || (CombatComp && !CombatComp->GetEquippedWeapon())) return;

	EWeaponType WeaponType = CombatComp->GetEquippedWeapon()->GetWeaponType();

	if(StateManagerComp)
	{
		StateManagerComp->SetCurrentState(ECurrentState::GENERAL_STATE);
		
		UAnimMontage* EnterCombatMontage = nullptr;
		UAnimMontage* ExitCombatMontage = nullptr;
		switch (WeaponType)
		{
		case EWeaponType::LIGHT_SWORD:
			EnterCombatMontage = CombatComp->GetLSEnterCombatMontage();
			ExitCombatMontage = CombatComp->GetLSExitCombatMontage();
			break;
		case EWeaponType::DUAL_SWORD:
			EnterCombatMontage = CombatComp->GetDSEnterCombatMontage();
			ExitCombatMontage = CombatComp->GetDSEnterCombatMontage();
			break;
		}

		if (StateManagerComp->GetCurrentCombatState() == ECurrentCombatState::NONE_COMBAT_STATE)
		{
			
			PerformAction(EnterCombatMontage, ECurrentState::GENERAL_STATE, ECurrentAction::ENTER_COMBAT);
			StateManagerComp->SetCurrentCombatState(ECurrentCombatState::COMBAT_STATE);
			if(GetMesh() && GetMesh()->GetAnimInstance())
			{
				Cast<UMeleeAnimInstance>(GetMesh()->GetAnimInstance())->SetCombatState(true);
			}
			if(TargetingComp) TargetingComp->UpdateRotationMode();
		}
		else if(StateManagerComp->GetCurrentCombatState() == ECurrentCombatState::COMBAT_STATE)
		{
			PerformAction(ExitCombatMontage, ECurrentState::GENERAL_STATE, ECurrentAction::EXIT_COMBAT);
			CombatComp->ResetAttackCount();
			StateManagerComp->SetCurrentCombatState(ECurrentCombatState::NONE_COMBAT_STATE);
			if(GetMesh() && GetMesh()->GetAnimInstance())
			{
				Cast<UMeleeAnimInstance>(GetMesh()->GetAnimInstance())->SetCombatState(false);
			}
			if(TargetingComp)
			{
				TargetingComp->DisableLockOn();
				TargetingComp->UpdateRotationMode();
			}
		}
	}
}

void ABaseCharacter::InteractButtonPressed()
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
		EDrawDebugTrace::None, 
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

void ABaseCharacter::HeavyAttack()
{
	if(CombatComp)
		CombatComp->HeavyAttack();
}

void ABaseCharacter::Dodge()
{
	if(!CanDodge() || (CombatComp && !CombatComp->GetEquippedWeapon())) return;
	
	CombatComp->PerformDodge();
}


bool ABaseCharacter::CanToggleCombat()
{
	TArray<ECurrentState> CharacterStates;
	CharacterStates.Add(ECurrentState::ATTACKING);
	CharacterStates.Add(ECurrentState::DODGING);
	CharacterStates.Add(ECurrentState::DEAD);
	CharacterStates.Add(ECurrentState::DISABLED);
	bool ReturnValue = false;
	if(StateManagerComp && GetCharacterMovement())
		ReturnValue = !StateManagerComp->IsCurrentStateEqualToThis(CharacterStates);
	return ReturnValue;
}

bool ABaseCharacter::CanDodge()
{
	if(StatComp && CombatComp && (StatComp->GetCurrentStatValue(EStats::STAMINA) < CombatComp->GetDodgeStaminaCost())) return false;
	TArray<ECurrentState> CharacterStates;
	CharacterStates.Add(ECurrentState::DODGING);
	CharacterStates.Add(ECurrentState::DEAD);
	CharacterStates.Add(ECurrentState::DISABLED);
	CharacterStates.Add(ECurrentState::GENERAL_STATE);
	bool ReturnValue = false;
	if(StateManagerComp && GetCharacterMovement())
		ReturnValue = (!StateManagerComp->IsCurrentStateEqualToThis(CharacterStates))  && (!GetCharacterMovement()->IsFalling());
	return ReturnValue;
}

FRotator ABaseCharacter::GetDesiredRotation() //구르기시 캐릭터가 움직이고 있는 방향의 회전값을 반환
{
	if(GetCharacterMovement())
	{
		FVector LastVector = GetCharacterMovement()->GetLastInputVector();
		bool VectorResult = UKismetMathLibrary::NotEqual_VectorVector(LastVector, FVector(0.f, 0.f, 0.f), 0.001f);
		if(VectorResult)
		{
			return UKismetMathLibrary::MakeRotFromX(GetLastMovementInputVector());
		}
	}
	return GetActorRotation();
}

void ABaseCharacter::ResetCombat()
{
	if(CombatComp) CombatComp->ResetAttackCount();
	if(StateManagerComp)
	{
		StateManagerComp->ResetState();
		StateManagerComp->ResetAction();
	}
}

void ABaseCharacter::ReceiveDamage(
	AActor* DamagedActor, 
	float EnemyATK, 
	const UDamageType* DamageType, 
	AController* InstigatedBy, 
	AActor* DamageCauser)
{
	if(InstigatedBy)
	{
		float DotProductResult = GetDotProductTo(InstigatedBy->GetPawn()); //맞은 캐릭터와 때린 캐릭터간의 내적
		bHitFront = FMath::IsWithin(DotProductResult, -0.1f, 1.f);
	}

	if (DamageType) //다른 대미지 타입 구현 안해놔서 임시.
	{
		ApplyHitReaction(EDamageType::MELEE_DAMAGE);
		ApplyImpactEffect(EDamageType::MELEE_DAMAGE, GetActorLocation());
	}
	else if(Cast<UAttackDamageType>(DamageType))
	{
		ApplyHitReaction(Cast<UAttackDamageType>(DamageType)->GetDamageType());
		ApplyImpactEffect(Cast<UAttackDamageType>(DamageType)->GetDamageType(), GetActorLocation());
	}
		

	if(StateManagerComp)
		StateManagerComp->SetCurrentState(ECurrentState::DISABLED);
	
	CalcReceiveDamage(EnemyATK);
}

void ABaseCharacter::CalcReceiveDamage(float EnemyATK) //받는 총 대미지 계산
{
	//대미지 계산
	if(StatComp)
	{
		const float Def = StatComp->GetCurrentStatValue(EStats::DEF);
		const float Result = FMath::Clamp((EnemyATK * FMath::RandRange(0.8, 1.2)) * (1 - (Def / (100 + Def))), 0, INT_MAX);
		StatComp->PlusCurrentStatValue(EStats::HP, Result * -1); //HP 적용
		if(StatComp->GetCurrentStatValue(EStats::HP) <= 0)
		{
			if(StateManagerComp)
				StateManagerComp->SetCurrentState(ECurrentState::DEAD);
		}
	}
}

void ABaseCharacter::Dead()
{
	EnableRagdoll();
	ApplyHitReactionPhysicsVelocity(2000.f);
	if(CombatComp && CombatComp->GetEquippedWeapon())
	{
		CombatComp->GetEquippedWeapon()->SimulateWeaponPhysics();
	}
	GetWorldTimerManager().SetTimer(DestroyDeadTimerHandle, this, &ThisClass::DestroyDead, DestroyDeadTime);
}

void ABaseCharacter::EnableRagdoll()
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_None);
	FAttachmentTransformRules Rules = FAttachmentTransformRules::KeepWorldTransform;
	CameraBoom->AttachToComponent(GetMesh(), Rules, PelvisBoneName);
	CameraBoom->bDoCollisionTest = false;
	GetMesh()->SetCollisionProfileName(TEXT("ragdoll"));
	GetMesh()->SetAllBodiesBelowSimulatePhysics(PelvisBoneName, true);
	GetMesh()->SetAllBodiesBelowPhysicsBlendWeight(PelvisBoneName, 1.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	
}

void ABaseCharacter::ApplyHitReactionPhysicsVelocity(float InitSpeed)
{
	const FVector NewVel = GetActorForwardVector() * (InitSpeed * -1.f);
	
	GetMesh()->SetPhysicsLinearVelocity(NewVel, false, PelvisBoneName);
}

void ABaseCharacter::Test()
{	
	//테스트할 함수 넣기. Key Mapping : 5
	const FVector Start = GetActorLocation() + GetActorForwardVector() * 50.f;
	const FVector End = Start + GetActorForwardVector() * 100.f;
	const FVector HalfSize = FVector(50.f, 50.f, 100.f);
	const FRotator Oritentation = GetActorRotation();


	TArray<TEnumAsByte<EObjectTypeQuery>> CollisionObjectType;
	TEnumAsByte<EObjectTypeQuery> Pawn = UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn);
	CollisionObjectType.Add(Pawn);

	TArray<AActor*> ActorsToIgnore;

	ActorsToIgnore.Add(GetOwner());
	EDrawDebugTrace::Type DebugTrace = EDrawDebugTrace::ForDuration;
	TArray<FHitResult> OutHitResult;

	UKismetSystemLibrary::BoxTraceMultiForObjects(
		this,
		Start,
		End,
		HalfSize,
		Oritentation,
		CollisionObjectType,
		false,
		ActorsToIgnore,
		EDrawDebugTrace::ForDuration,
		OutHitResult,
		true,
		FLinearColor::Red,
		FLinearColor::Blue,
		5.f
	);
	

}

bool ABaseCharacter::CanRecieveDamage()
{
	if(StateManagerComp && StateManagerComp->GetCurrentState() != ECurrentState::DEAD)
		return true;
	else 
		return false;
}

void ABaseCharacter::DestroyDead()
{
	if(CombatComp && CombatComp->GetEquippedWeapon())
	{
		CombatComp->GetEquippedWeapon()->Destroy();
	}
	Destroy();
}

void ABaseCharacter::CharacterStateBegin(ECurrentState State)
{
	switch (State)
	{
	case ECurrentState::NOTHING:
		
		break;
	case ECurrentState::ATTACKING:
		
		break;
	case ECurrentState::DODGING:
		
		break;
	case ECurrentState::GENERAL_STATE:
		
		break;
	case ECurrentState::DEAD:
			Dead();
		break;
	case ECurrentState::DISABLED:
		
		break;
	}
}

void ABaseCharacter::PerformAction(UAnimMontage* Montage, ECurrentState State, ECurrentAction Action)
{
	if(CombatComp && CombatComp->GetEquippedWeapon() && StateManagerComp)
	{
		StateManagerComp->SetCurrentState(State);
		StateManagerComp->SetCurrentAction(Action);

		if(Montage)	PlayAnimMontage(Montage);
	}
}

void ABaseCharacter::ToggleWalk()
{
	if(StateManagerComp && StateManagerComp->GetMovementType() != EMovementType::WALKING)
		SetMovementType(EMovementType::WALKING);
	else
		SetMovementType(EMovementType::JOGGING);
	
}

void ABaseCharacter::SprintButtonPressed()
{
	bSprintKeyPressed = true;
	if(GetVelocity().Size() <= 0.f) return;
	SetMovementType(EMovementType::SPRINTING);
}

void ABaseCharacter::SprintButtonReleased()
{
	bSprintKeyPressed = false;
	SetMovementType(EMovementType::JOGGING);
}

void ABaseCharacter::SetMovementType(EMovementType Type)
{	
	if(GetCharacterMovement() && StateManagerComp)
	{
		StateManagerComp->SetMovementType(Type);
		switch(Type)
		{
			case EMovementType::WALKING:
				GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
				break;
			case EMovementType::JOGGING:
				GetCharacterMovement()->MaxWalkSpeed = JogSpeed;
				break;
			case EMovementType::SPRINTING:
				GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
				break;
		}
	}
	if (TargetingComp)
		TargetingComp->UpdateRotationMode();
}

void ABaseCharacter::Equip(ABaseEquippable* Equipment)
{
	if(CombatComp && Equipment)
	{
		CombatComp->OnEquipped(Equipment);
	}
}

void ABaseCharacter::ToggleLockOn()
{
	if(StateManagerComp && StateManagerComp->GetCurrentCombatState() == ECurrentCombatState::NONE_COMBAT_STATE) return;
	if(TargetingComp) TargetingComp->ToggleLockOn();
}

bool ABaseCharacter::CanBeTargeted()
{
	if(StateManagerComp)
	{
		TArray<ECurrentState> State;
		State.Add(ECurrentState::DEAD);
		return !StateManagerComp->IsCurrentStateEqualToThis(State);
	}
	return false;
}

void ABaseCharacter::OnTargeted(bool IsTargeted)
{
	if(LockOnWidget)
	{
		LockOnWidget->SetVisibility(IsTargeted);
	}
}

void ABaseCharacter::ApplyHitReaction(EDamageType DamageType)
{
	switch (DamageType)
	{
	case EDamageType::MELEE_DAMAGE:
		PerformHitReact();
		break;
	case EDamageType::KNOCKDOWN_DAMAGE:
		PerformKnockdown();
		break;
	}
}

void ABaseCharacter::ApplyImpactEffect(EDamageType DamageType, FVector HitLocation)
{
	if(ImpactSound && ImpactParticle)
	{
		switch (DamageType)
		{
		case EDamageType::MELEE_DAMAGE:
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, HitLocation); 
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticle, HitLocation);
			break;
		case EDamageType::KNOCKDOWN_DAMAGE:
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, HitLocation); //나중에 다른걸로 추가
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticle, HitLocation);
			break;
		}
	}	
}

void ABaseCharacter::PerformHitReact()
{
	if(bHitFront)
	{
		if(HitReactFrontMontage)
			PlayAnimMontage(HitReactFrontMontage);
	}
	else
	{
		if(HitReactBackMontage)
			PlayAnimMontage(HitReactBackMontage);
	}
	
}

void ABaseCharacter::PerformKnockdown()
{
	if(bHitFront)
	{
		if(KnockdownFrontMontage)
			PlayAnimMontage(KnockdownFrontMontage);
	}
	else
	{
		if(KnockdownBackMontage)
			PlayAnimMontage(KnockdownBackMontage);
	}
}

void ABaseCharacter::CameraZoomInOut(float Rate)
{
	
}



