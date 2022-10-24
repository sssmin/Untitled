#include "BTService_BossUpdateBehavior.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/TargetPoint.h"

#include "../Component/StateManagerComponent.h"
#include "../MonsterCharacter/EnemyCharacter.h"
#include "../PlayerController/EnemyAIController.h"
#include "../Interface/BossInterface.h"



void UBTService_BossUpdateBehavior::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	MonsterController = OwnerComp.GetAIOwner();
	
	if(MonsterController)
	{
		MonsterCharacter = Cast<AEnemyCharacter>(MonsterController->GetPawn());
		BlackBoardComp = Cast<AEnemyAIController>(MonsterController)->GetBBComp();

		if(MonsterCharacter && MonsterCharacter->GetStateManagerComp())
		{
			if((MonsterCharacter->GetStateManagerComp()->GetCurrentState() != ECurrentState::NOTHING) && (MonsterCharacter->GetStateManagerComp()->GetCurrentState() != ECurrentState::DEAD))
			{
				if(BlackBoardComp)
				{
					AActor* TargetActor = Cast<AActor>(BlackBoardComp->GetValueAsObject(TEXT("Target")));
					if (TargetActor)
					{								
						const float Dist = MonsterCharacter->GetDistanceTo(TargetActor);
						
						BossCase(Dist, TargetActor);
					}
					else
					{
						SetBossBehavior(EBossBehavior::PATROL);
					}
				}
			}
			else
			{
				SetBossBehavior(EBossBehavior::NONE);
			}
		}
	}
}


void UBTService_BossUpdateBehavior::BossCase(float Dist, AActor* Target)
{
	bool SpecialReady = Cast<AEnemyAIController>(MonsterController)->GetBBComp()->GetValueAsBool(TEXT("SpecialReady"));
	bool SpecialComplete = Cast<AEnemyAIController>(MonsterController)->GetBBComp()->GetValueAsBool(TEXT("SpecialComplete"));
	//Ready는 애니메이션 실행 전까지 true, 애니메이션 실행 완료 애님 노티파이 오면 false, 타이머 후 true로
	//Complete은 스킬 실행했으면 그 스킬은 꼭 수행하게 끔하는 변수. 애니메이션 실행 후 다 끝났을 때 true로.
	
	if(SpecialReady && SpecialComplete) 
	{								   
		if(Dist > 550.f)      
		{
			Cast<AEnemyAIController>(MonsterController)->GetBBComp()->SetValueAsVector(TEXT("DashLocation"), (Target->GetActorLocation() - (MonsterCharacter->GetActorForwardVector() * 200.f)));
				
            int32 RandValue = FMath::RandRange(0, 1);
            if(RandValue == 0)
                SetBossBehavior(EBossBehavior::SPECIAL1);
            else if(RandValue == 1)
                SetBossBehavior(EBossBehavior::SPECIAL2);
			BlackBoardComp->SetValueAsBool(TEXT("SpecialComplete"), false);			
		}
		else if(Dist > 300.f)
		{
			int32 RandValue = FMath::RandRange(0, 2);
            if(RandValue == 0)
                SetBossBehavior(EBossBehavior::SPECIAL1);
            else if(RandValue == 1)
                SetBossBehavior(EBossBehavior::SPECIAL2);
			else
				SetBossBehavior(EBossBehavior::SPECIAL3);
			BlackBoardComp->SetValueAsBool(TEXT("SpecialComplete"), false);

		}
		else if(Dist > 150.f)
		{
			SetBossBehavior(EBossBehavior::CHASE);
		}
		else
		{
			SetBossBehavior(EBossBehavior::BASIC_ATTACK);
		}
	}
	else if(SpecialReady && !SpecialComplete) //스킬 사용 하기 전. Ready는 아직 true인 경우.
	{
        if(Dist <= MonsterCharacter->GetAttackRange())
		{
			BlackBoardComp->SetValueAsBool(TEXT("InAttackRange"), true);
			Cast<AEnemyAIController>(MonsterController)->GetBBComp()->SetValueAsBool(TEXT("OriPosReturn"), false);
		}
		else
		{
			BlackBoardComp->SetValueAsBool(TEXT("InAttackRange"), false);
			
		}
	}
	else if(!SpecialReady) //스페셜 공격 불가능일 때. 쿨다운일 때.
	{
		if(Dist <= MonsterCharacter->GetAttackRange())
		{
			BlackBoardComp->SetValueAsBool(TEXT("InAttackRange"), true);
			Cast<AEnemyAIController>(MonsterController)->GetBBComp()->SetValueAsBool(TEXT("OriPosReturn"), false);
			SetBossBehavior(EBossBehavior::BASIC_ATTACK);
		}
		else
		{
			SetBossBehavior(EBossBehavior::CHASE);
			BlackBoardComp->SetValueAsBool(TEXT("InAttackRange"), false);
			TArray<ATargetPoint*> PatrolPoint = MonsterCharacter->GetPatrolPoints();
			const float CenterDist = MonsterCharacter->GetDistanceTo(Cast<AActor>(PatrolPoint[0]));
			if(CenterDist > 1540.f) 
			{					   
				Cast<AEnemyAIController>(MonsterController)->GetBBComp()->SetValueAsBool(TEXT("OriPosReturn"), true);
				SetBossBehavior(EBossBehavior::RETURN);
			}
		}
	}
}

void UBTService_BossUpdateBehavior::SetBossBehavior(EBossBehavior AIBehavior)
{
	uint8 EnumIdx = 0;

	switch (AIBehavior)
	{
	case EBossBehavior::NONE:
		EnumIdx = 0;
		break;
	case EBossBehavior::BASIC_ATTACK:
		EnumIdx = 1;
		break;
	case EBossBehavior::CHASE:
		EnumIdx = 2;
		break;
    case EBossBehavior::PATROL:
		EnumIdx = 3;
		break;
	case EBossBehavior::SPECIAL1:
		EnumIdx = 4;
		break;
	case EBossBehavior::SPECIAL2:
		EnumIdx = 5;
		break;
    case EBossBehavior::SPECIAL3:
		EnumIdx = 6;
		break;
    case EBossBehavior::RETURN:
		EnumIdx = 7;
		break;
	}
	BlackBoardComp->SetValueAsEnum(BossBehavior.SelectedKeyName, EnumIdx);
}
