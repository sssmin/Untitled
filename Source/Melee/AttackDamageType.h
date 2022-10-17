#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "Type/DamageTypes.h"
#include "AttackDamageType.generated.h"


UCLASS()
class MELEE_API UAttackDamageType : public UDamageType
{
	GENERATED_BODY()
public:	
	UAttackDamageType();
	
private:
	EDamageType DamageType;

public: //get
	FORCEINLINE EDamageType GetDamageType() const { return DamageType; }
	
};
