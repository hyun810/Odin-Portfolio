// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpecHandle.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PlayAttackMontage.generated.h"

class AODAIController;
class UAbilitySystemComponent;
struct FGameplayAbilitySpecHandle;

USTRUCT(BlueprintType, meta=(DisplayName="공격 방식 가중 태그"))
struct FAttackTypeWeight
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;
	
	UPROPERTY()
	float Weight = 0.0f;
};

// CDF(누적 분포 함수) 기반 가중치 선택 시스템
struct FWeightCDF
{
	TArray<FAttackTypeWeight> AttackType;	// 공격 타입 목록
	TArray<float> Weight;					// 누적 가중치 배열
	float Sum = 0.0f;                       // 전체 가중치 합계    

    // CDF 구축: 각 Weight를 누적 합계로 변환
	void Rebuild()
	{
		Weight.SetNum(AttackType.Num());

		for (int32 i=0; i<AttackType.Num(); ++i)
		{
			const float W = FMath::Max(0.f, AttackType[i].Weight); //음수 방지
			Sum += W;
			Weight[i] = Sum;
		}
	}

	// 랜덤 값으로 공격 타입 선택
	int32 Pick(FRandomStream& Random) const
	{
		if (Sum <=0)
		{
			return INDEX_NONE;
		}
		
		// 랜덤 값 생성
		const float Result = Random.FRandRange(0, Sum);
		
		for (int32 i=0; i<Weight.Num(); ++i)
		{
			if (Result <= Weight[i])
			{
				return i;
			}
		}

		// 안전망: 마지막 인덱스 반환
		return Weight.Num() - 1;	
	}
};

/**
 * 
 */
UCLASS()
class OD_API UBTTask_PlayAttackMontage : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_PlayAttackMontage();

protected:
	virtual void OnInstanceCreated(UBehaviorTreeComponent& OwnerComp) override;
	
private:
	TWeakObjectPtr<UBehaviorTreeComponent> CachedBehaviorComp;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;
	TWeakObjectPtr<AODAIController> CachedAIController;
	FGameplayAbilitySpecHandle CachedSpecHandle;

	UPROPERTY()
	FRandomStream Random;
	
public:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UFUNCTION()
	void EndPlayMontage(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	bool HasCandidatesForTag(const FGameplayTag& Tag);
	
};
