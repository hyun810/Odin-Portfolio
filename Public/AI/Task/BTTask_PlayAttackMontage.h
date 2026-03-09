// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_PlayAttackMontage.generated.h"

class AODAIController;
class UAbilitySystemComponent;
struct FGameplayAbilitySpecHandle;

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
