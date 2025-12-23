// Fill out your copyright notice in the Description page of Project Settings.


#include "Ai/ODAIController.h"

#include "NavigationSystem.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Character/ODMonsterBase.h"
#include "Character/ODPlayer.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Sight.h"

AODAIController::AODAIController()
{
	bReplicates = false;

	RepGraphCount = 0;

#pragma region 거리별 최적화
	bAlwaysRelevant = false;
	bOnlyRelevantToOwner = false;
	bNetUseOwnerRelevancy = false;
	NetCullDistanceSquared = FMath::Square(3500.f);
#pragma endregion

	// 감각 시스템 컴포넌트 초기화
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>
							("AIPerceptionComponent");

	// 시각 감지 구성 : 적과 중립만 탐지하도록 최적화
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>("Sight Config");
	
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	// 아군을 제외함으로써 성능 향상
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	// 피격 감지 구성 : Non-Aggressive 몬스터의 반격 로직에 활용
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>("DamageConfig");

	// 두 가지 감각을 컴포넌트에 등록하여 멀티 센싱 활성화
	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->ConfigureSense(*DamageConfig);
}

void AODAIController::OnPossess(APawn* InPawn)
{
#pragma region validation
	Super::OnPossess(InPawn);

	if (!HasAuthority()) return;
	
	Monster = Cast<AODMonsterBase>(InPawn);
	if (!Monster) return;
#pragma endregion
	
#pragma region AiPerception Setting
	if (!SightConfig)
	{
		UAISenseConfig* SightFound = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Sight>();
		SightConfig = Cast<UAISenseConfig_Sight>(SightFound);
	}

	if (!DamageConfig)
	{
		UAISenseConfig* DamageFound = AIPerceptionComponent->GetSenseConfig<UAISenseConfig_Damage>();
		DamageConfig = Cast<UAISenseConfig_Damage>(DamageFound);
	}

	// 동일 컨트롤러로 다양한 몬스터 대응 (데이터 주도 설계)
	SightConfig->PeripheralVisionAngleDegrees = Monster->GetPerceptionInitValue().VisionAngleDegreeValue;
	SightConfig->SightRadius = Monster->GetPerceptionInitValue().SightRadiusValue;
	SightConfig->LoseSightRadius = Monster->GetPerceptionInitValue().LoseSightRadiusValue;
	SightConfig->SetMaxAge(10.f);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;

	AIPerceptionComponent->RequestStimuliListenerUpdate();
	AIPerceptionComponent->ConfigureSense(*SightConfig);
#pragma endregion

#pragma region BT Setting
	// 등급별 BT 자동 선택으로 AI 복잡도 차별화
	UBehaviorTree* BehaviorTree = (Monster->GetEMonsterRank() == EMonsterRank::Boss
				|| Monster->GetEMonsterRank() == EMonsterRank::DungeonBoss) ? BossBT : GruntBT;

	if (!BehaviorTree) return;
	
	UBlackboardComponent* BB = nullptr;
	const bool bBBCheck = UseBlackboard(BehaviorTree->BlackboardAsset, BB);
	if (!bBBCheck || !BB) return;

	BB->SetValueAsVector(TEXT("SpawnLocation"), Monster->GetActorLocation());
	
	RunBehaviorTree(BehaviorTree);
#pragma endregion

	// Tick 대신 이벤트 델리게이트 바인딩으로 CPU부화 최소화
	AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic
				(this, &AODAIController::TargetPerceptionUpdated);
}

void AODAIController::OnUnPossess()
{
	Super::OnUnPossess();
	
	if (AIPerceptionComponent)
	{
		// 메모리 누수 방지를 위한 델리게이트 해제
		AIPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic
				(this, &AODAIController::TargetPerceptionUpdated);
	}
}

void AODAIController::TargetPerceptionUpdated(AActor* TargetActor, FAIStimulus Stimulus)
{
	// 자극 타입 실별 : 시각과 피격을 구분하여 처리
	const auto SightID = UAISense::GetSenseID(UAISense_Sight::StaticClass());
	const auto DamageID = UAISense::GetSenseID(UAISense_Damage::StaticClass());

	AODPlayer* Player = Cast<AODPlayer>(TargetActor);
	if (!Player) return;

	// 몬스터 등급에 따른 다형적 행동
	if (Monster->GetEMonsterRank() == EMonsterRank::Boss
		|| Monster->GetEMonsterRank() == EMonsterRank::DungeonBoss)
	{
		// Boss 전용: 멀티 타겟 추적으로 전략적 난이도 향상
		if (Stimulus.WasSuccessfullySensed())
		{
			SightPlayers.Add(Player);
		}
		else
		{
			SightPlayers.Remove(Player);
		}
	}
	else if (Monster->GetEMonsterRank() == EMonsterRank::Grunt)
	{
		
		if (GetCurrentTarget() == true) return;

		// Aggressive: 선공형 AI - 시야 기반 자동 교전
		if (Monster->GetEngageType() == EMonsterEngageType::Aggressive
			&& Stimulus.Type == SightID)
		{
			if (Stimulus.WasSuccessfullySensed())
			{
				SetCurrentTarget(Player);
				Monster->SetTargetPlayerActor(Player);								
			}
		}
		// Non-Aggressive : 반격형 AI - 피격 기반 반격형 교전
		else if (Monster->GetEngageType() == EMonsterEngageType::Non_Aggressive
			&& Stimulus.Type == DamageID)
		{
			if (Stimulus.WasSuccessfullySensed())
			{
				SetCurrentTarget(Player);
				Monster->SetTargetPlayerActor(Player);
			}
		}
	}
}


void AODAIController::ClearSightPlayers()
{
	SightPlayers.Empty();
}

void AODAIController::StopBehaviorTree()
{
	UBehaviorTreeComponent* BehaviorTreeComponent = Cast<UBehaviorTreeComponent>(GetBrainComponent());

	if (BehaviorTreeComponent)
	{
		BehaviorTreeComponent->StopTree();
	}
}

#pragma region Getter
TSet<TWeakObjectPtr<AODPlayer>> AODAIController::GetSightPlayers()
{
	return SightPlayers;
}

UAIPerceptionComponent* AODAIController::GetPerceptionComponent()
{
	return AIPerceptionComponent;
}

UAISenseConfig_Sight* AODAIController::GetSenseConfig()
{
	return SightConfig;
}

int32 AODAIController::GetRepGraphCount()
{
	return RepGraphCount;
}

bool AODAIController::GetCurrentTarget()
{
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();

	if (BlackboardComponent)
	{
		if (BlackboardComponent->GetValueAsObject(TargetBlackBoardKeyName))
		{
			return true;
		}		
	}
	return false;
}
#pragma endregion

#pragma region Setter
void AODAIController::SetRepGraphCount(const int32 InRepGraph)
{
	RepGraphCount = InRepGraph;
}

void AODAIController::SetCurrentTarget(AActor* NewTarget)
{
	UBlackboardComponent* BlackboardComponent = GetBlackboardComponent();

	if (!BlackboardComponent) return;
	
	if (NewTarget)
	{
		BlackboardComponent->SetValueAsObject(TargetBlackBoardKeyName, NewTarget);
	}
}
#pragma endregion
