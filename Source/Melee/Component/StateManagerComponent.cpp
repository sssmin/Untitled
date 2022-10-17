#include "StateManagerComponent.h"

#include "../Type/Types.h"
#include "CombatComponent.h"

UStateManagerComponent::UStateManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentMovementType = EMovementType::JOGGING;
	CurrentState = ECurrentState::NOTHING;
	CurrentAction = ECurrentAction::NOTHING;
	CurrentCombatState = ECurrentCombatState::NONE_COMBAT_STATE;

}

void UStateManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner())
	{
		UCombatComponent* CombatComp = Cast<UCombatComponent>(GetOwner()->GetComponentByClass(UCombatComponent::StaticClass()));	
		if(CombatComp)
		{
			CombatComp->GetCurrentCombatState.BindUObject(this, &ThisClass::GetCurrentCombatState);
			CombatComp->GetCurrentState.BindUObject(this, &ThisClass::GetCurrentState);
			CombatComp->IsCurrentStateEqualToThis.BindUObject(this, &ThisClass::IsCurrentStateEqualToThis);
			CombatComp->OnUpdateCurrentState.BindUObject(this, &ThisClass::SetCurrentState);
			CombatComp->OnUpdateCurrentAction.BindUObject(this, &ThisClass::SetCurrentAction);
			CombatComp->GetCurrentMovementType.BindUObject(this, &ThisClass::GetMovementType);
		}
			
	}
}

void UStateManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UStateManagerComponent::SetCurrentState(ECurrentState State)
{
	if(CurrentState != State)
	{
		OnStateEnd.Broadcast(CurrentState);
		CurrentState = State;
		OnStateBegin.Broadcast(CurrentState);
	}
}

void UStateManagerComponent::ResetState()
{
	SetCurrentState(ECurrentState::NOTHING);
}

void UStateManagerComponent::ResetAction()
{
	SetCurrentAction(ECurrentAction::NOTHING);
}

bool UStateManagerComponent::IsCurrentStateEqualToThis(TArray<ECurrentState> StatesToCheck)
{
	return StatesToCheck.Contains(CurrentState);
}

void UStateManagerComponent::SetCurrentAction(ECurrentAction Action)
{
	if(CurrentAction != Action)
	{
		OnActionEnd.Broadcast(CurrentAction);
		CurrentAction = Action;
		OnActionBegin.Broadcast(CurrentAction);
	}
}

bool UStateManagerComponent::IsCurrentActionEqualToThis(TArray<ECurrentAction> StatesToCheck)
{
	return StatesToCheck.Contains(CurrentAction);
}

void UStateManagerComponent::SetMovementType(EMovementType Type)
{
	CurrentMovementType = Type;
	if (CurrentMovementType == EMovementType::SPRINTING)
	{
		OnSprint.ExecuteIfBound(false);
	}
	else
	{
		OnSprint.ExecuteIfBound(true);
	}
	
}

void UStateManagerComponent::SetCurrentCombatState(ECurrentCombatState CombatState)
{
	CurrentCombatState = CombatState;
	if (CurrentCombatState == ECurrentCombatState::COMBAT_STATE)
	{
		OnCombatState.ExecuteIfBound(true);
	}
	else
	{
		OnCombatState.ExecuteIfBound(false);
	}
	
}

ECurrentCombatState UStateManagerComponent::GetCurrentCombatState()
{
	return CurrentCombatState;
}