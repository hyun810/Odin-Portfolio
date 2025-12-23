// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Interface/ODRepGraphInterface.h"
#include "ODAIController.generated.h"

class AODPlayer;
class UAISenseConfig_Damage;
struct FAIStimulus;
class AODMonsterBase;
class UAISenseConfig_Sight;
/**
 * 
 */
UCLASS()
class OD_API AODAIController : public AAIController , public IODRepGraphInterface
{
	GENERATED_BODY()

#pragma region Constructor & OnPossess
public:
	AODAIController();

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;
#pragma endregion

#pragma region RepGraphCount
	virtual int32 GetRepGraphCount() override;
	virtual void SetRepGraphCount(const int32 InRepGraph) override;
private:
	UPROPERTY()
	int32 RepGraphCount;
#pragma endregion RepGraphCount

private:
	// 이벤트 기반 감각 처리를 담당하는 핵심 컴포넌트
	UPROPERTY(EditDefaultsOnly, Category="Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

	// 시야 범위와 각도를 동적으로 설정 가능한 시각 구성
	UPROPERTY(EditDefaultsOnly, Category="Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	// 피격 시 즉각 반응하기 위한 데미지 감지 구성
	UPROPERTY(EditDefaultsOnly, Category="Perception")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	// 현재 제어 중인 몬스터 (타입별 동적 행동 결정에 사용)
	UPROPERTY()
	TObjectPtr<AODMonsterBase> Monster;

	UPROPERTY(EditDefaultsOnly, Category = "AI Behavior")
	FName TargetBlackBoardKeyName = "TargetActor";

public:
	UFUNCTION()
	void TargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus);
	
	UFUNCTION()
	void StopBehaviorTree();

#pragma region AI Behavior
private:
	UPROPERTY(EditDefaultsOnly, Category="AI Behavior")
	TObjectPtr<UBehaviorTree> GruntBT;

	UPROPERTY(EditDefaultsOnly, Category="AI Behavior")
	TObjectPtr<UBehaviorTree> BossBT;
#pragma endregion

#pragma region Boss Target Logic
private:	
	UPROPERTY()
	TSet<TWeakObjectPtr<AODPlayer>> SightPlayers;

public:
	UFUNCTION()
	void ClearSightPlayers();
#pragma endregion

#pragma region Getter
public:
	bool GetCurrentTarget();

	UFUNCTION()
	UAIPerceptionComponent* GetPerceptionComponent();

	UFUNCTION()
	UAISenseConfig_Sight* GetSenseConfig();

	UFUNCTION()
	TSet<TWeakObjectPtr<AODPlayer>> GetSightPlayers();
#pragma endregion

#pragma region Setter
public:
	void SetCurrentTarget(AActor* NewTarget);
	

#pragma endregion
	
};
