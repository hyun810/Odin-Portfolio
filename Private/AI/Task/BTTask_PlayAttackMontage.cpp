// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/Task/BTTask_PlayAttackMontage.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "AI/ODAIController.h"
#include "Character/ODMonsterBase.h"
#include "Components/ODCombatComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Tags/ODAbilityTags.h"

using namespace GameplayAbilityTags;

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


UBTTask_PlayAttackMontage::UBTTask_PlayAttackMontage()
{
	bCreateNodeInstance = true;
}

void UBTTask_PlayAttackMontage::OnInstanceCreated(UBehaviorTreeComponent& OwnerComp)
{
	Super::OnInstanceCreated(OwnerComp);

	//Seed값 생성 및 초기화
	const AActor* Owner = OwnerComp.GetAIOwner();
	const FString Key = FString::Printf(TEXT("%s|%s|%d"), *OwnerComp.GetWorld()->GetMapName(), Owner ? *Owner->GetName() : TEXT(""), 0);
	const uint32 Seed = FCrc::StrCrc32(*Key);

	Random.Initialize((int32)Seed);
}

EBTNodeResult::Type UBTTask_PlayAttackMontage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
#pragma region Precheck_Cache
	CachedBehaviorComp = &OwnerComp;
	
	CachedAIController = Cast<AODAIController>(OwnerComp.GetAIOwner());
	if (CachedAIController.IsValid())
	{
		 AODMonsterBase* Monster = Cast<AODMonsterBase>(CachedAIController->GetPawn());
		 if (!Monster) return EBTNodeResult::Failed;

		//어빌리티 사용
		CachedASC = Monster->GetAbilitySystemComponent();
		if (!CachedASC.IsValid()) return EBTNodeResult::Failed;
#pragma endregion
		
#pragma region Weights_Build_CDF
		//가중치를 활용한 Tag선택
		TArray<FAttackTypeWeight> TagWeights = {
			{FGameplayTag::RequestGameplayTag("OD.Ability.Combat.NormalAttack"), 0.7f},
			{FGameplayTag::RequestGameplayTag("OD.Ability.AttackType.Skill"), 0.3f}
		};

		FWeightCDF WeightCDF;
		for (const FAttackTypeWeight& W : TagWeights)
		{
			if (W.Tag.MatchesTagExact(OD_Ability_Combat_NormalAttack) || HasCandidatesForTag(W.Tag))
			{
				WeightCDF.AttackType.Add(W);
			}
		}

		if (WeightCDF.AttackType.Num() == 0)
		{
			WeightCDF.AttackType.Add({ OD_Ability_Combat_NormalAttack, 1.f }); // 안전망
		}
		
		WeightCDF.Rebuild();
#pragma endregion

#pragma region Tag_Select
		const int32 PickedIndex = WeightCDF.Pick(Random);
		if (PickedIndex == INDEX_NONE)
		{
			return EBTNodeResult::Failed;
		}

		const FGameplayTag SelectedTag = WeightCDF.AttackType[PickedIndex].Tag;
#pragma endregion

#pragma region Candidates_Filter
		TArray<FGameplayAbilitySpec*> Matching;
		for (FGameplayAbilitySpec& Spec : CachedASC->GetActivatableAbilities())
		{
			if (!Spec.Ability)
			{
				continue;
			}

			const FGameplayTagContainer& AbilityTags = Spec.Ability->AbilityTags;

			//const bool bMatch = AbilityTags.HasAny(SelectedTag) || Spec.DynamicAbilityTags.HasAny(SelectedTag);
			const bool bMatch = AbilityTags.HasTag(SelectedTag) || Spec.DynamicAbilityTags.HasTag(SelectedTag);
			if (!bMatch) continue;

			if (Spec.Ability->DoesAbilitySatisfyTagRequirements(*CachedASC, nullptr, nullptr, nullptr))
			{
				Matching.Add(&Spec);
			}
		}
#pragma endregion

#pragma region GA_TryActivate
		FGameplayAbilitySpec* ChosenSpec;
		if (Matching.Num() > 0)
		{
			const int32 Index = FMath:: RandRange(0, Matching.Num() - 1);
			ChosenSpec = Matching[Index];
		}
		else
		{
			return EBTNodeResult::Failed;
		}
		
		if (!ChosenSpec || ChosenSpec->IsActive())
		{
			//UE_LOG(LogTemp, Error, TEXT("!ChosenSpec && ChosenSpec->IsActive()"));
			return EBTNodeResult::Failed;
		}
		
		if (CachedASC->TryActivateAbility(ChosenSpec->Handle))
		{
			if (UAnimInstance* AnimInstance = Monster->GetMesh()->GetAnimInstance())
			{
				CachedAnimInstance = AnimInstance;
				if (!CachedAnimInstance.IsValid())
					return EBTNodeResult::Failed;
				
				UAnimMontage* AttackMontage = AnimInstance->GetCurrentActiveMontage();
				if (AttackMontage)
				{
					FOnMontageBlendingOutStarted BlendingOutDelegate;
					BlendingOutDelegate.BindUObject(this, &UBTTask_PlayAttackMontage::EndPlayMontage);

					AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, AttackMontage);
					
					return EBTNodeResult::InProgress;				
				}			
			}				
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("TryActive false"));
		}
	}
#pragma endregion
	
	return EBTNodeResult::Failed;
}

void UBTTask_PlayAttackMontage::EndPlayMontage(UAnimMontage* Montage, bool bInterrupted)
{
	if (UAnimInstance* AnimInstance = CachedAnimInstance.Get())
	{
		AnimInstance->Montage_Stop(0.f, Montage);
	}
	
	UBehaviorTreeComponent* BT = CachedBehaviorComp.Get();
	if (BT)
	{
		FinishLatentTask(*BT,bInterrupted ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
	}
}

//Tag기반 GA 검사.
bool UBTTask_PlayAttackMontage::HasCandidatesForTag(const FGameplayTag& Tag)
{
	if (!CachedASC.Get())
	{
		return false;
	}

	for (FGameplayAbilitySpec& Spec : CachedASC->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		const FGameplayTagContainer& AbilityTags = Spec.Ability->AbilityTags;
		const bool bMatch = AbilityTags.HasTag(Tag) || Spec.DynamicAbilityTags.HasTag(Tag);

		if (!bMatch)
		{
			continue;
		}

		if (Spec.Ability->DoesAbilitySatisfyTagRequirements(*CachedASC, nullptr, nullptr, nullptr))
		{
			return true;
		}
	}

	return false;
}
