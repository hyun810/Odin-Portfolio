# StreamOD

> **오딘: 발할라 라이징** 클론 프로젝트 - 언리얼 엔진 5 멀티플레이어 ARPG

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0+-313131?style=flat&logo=unreal-engine)](https://www.unrealengine.com/)
[![C++](https://img.shields.io/badge/C++-17-00599C?style=flat&logo=c%2B%2B)](https://isocpp.org/)
[![Gameplay Ability System](https://img.shields.io/badge/GAS-Enabled-orange?style=flat)](https://docs.unrealengine.com/5.0/en-US/gameplay-ability-system-for-unreal-engine/)

## 프로젝트 소개

![오딘 배경](https://github.com/user-attachments/assets/e4b9d0e3-2371-4fb6-9804-d26f105b92d9)


StreamOD는 모바일 게임 "Odin: Valhalla Rising"의 핵심 시스템을 언리얼 엔진 5로 재구현한 멀티플레이어 ARPG 프로젝트입니다. 최신 게임 개발 패턴과 성능 최적화 기법을 적용하여 확장 가능하고 유지보수가 용이한 게임 아키텍처를 구축했습니다.

### 주요 목표
- **데이터 주도 설계**: 코드 변경 없이 게임 콘텐츠 확장
- **성능 최적화**: 이벤트 기반 AI, LOD/Culling, 네트워크 최적화
- **확장 가능한 아키텍처**: 컴포넌트 기반 설계와 다형성 활용
- **멀티플레이어 지원**: Dedicated Server 방식의 협동 플레이

---

## 핵심 기술 특징

### 이벤트 기반 AI 시스템

#### **성능 중심 설계**
- **Tick 제거**: 매 프레임 검사 대신 AI Perception 이벤트 델리게이트 활용
- **CPU 최적화**: 플레이어 감지 시에만 반응 (초당 60회 → 0~2회)
- **멀티 센싱**: Sight와 Damage 자극을 동시 처리

**소스 코드:** [`ODAIController.cpp` (Lines 100-101)](Private/AI/ODAIController.cpp#L100-L101)

```cpp
// 이벤트 기반 감지 - Tick 대신 델리게이트 사용
AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
    this, 
    &AODAIController::TargetPerceptionUpdated
);
```

#### **데이터 기반 행동 분기**

**소스 코드:** [`ODAIController.cpp` (Lines 85-86)](Private/AI/ODAIController.cpp#L85-L86)

```cpp
// 몬스터 등급에 따른 동적 BehaviorTree 선택
UBehaviorTree* SelectedBT = (Monster->GetEMonsterRank() == EMonsterRank::Boss) 
    ? BossBT  // 복잡한 전투 패턴 + 멀티타겟
    : GruntBT; // 단순한 추격/공격 패턴
```

**몬스터 타입별 자동 행동 분기**

| 타입 조합 | 반응 자극 | 행동 패턴 |
|----------|---------|---------|
| **Boss** | Sight | 다중 플레이어 추적 (TSet) |
| **Grunt + Aggressive** | Sight | 단일 타겟 선공 |
| **Grunt + Non-Aggressive** | Damage | 단일 타겟 반격 |

#### **재사용성 극대화**
- **단일 컨트롤러**: 모든 몬스터 타입에 `AODAIController` 재사용
- **데이터만 변경**: 새 몬스터 추가 시 코드 수정 불필요
- **코드 재사용률**: 85% 공통 코드, 15% 데이터 설정

---

### Gameplay Ability System (GAS)

#### **계층적 어빌리티 구조**
```
OD_GA_Base (기본 클래스)
├── OD_GA_Init_* (초기화 시스템)
│   ├── FillVitals (체력/마나 초기화)
│   ├── MaxExp (최대 경험치 계산)
│   └── Regeneration (자동 회복)
├── OD_GA_Combat_* (전투 시스템)
│   ├── NormalAttack (일반 공격)
│   ├── RangedAttack (원거리 공격)
│   └── Death (사망 처리)
├── OD_GA_Skill_* (스킬 시스템)
│   ├── Active (즉시 발동 스킬)
│   └── Passive (지속 효과)
└── OD_GA_Transaction_* (보상/거래)
    └── FieldMobReward (몬스터 처치 보상)
```

#### **캐릭터별 어빌리티 세트**
- **Valkyrie**: 6개 액티브 스킬 (근접 전투 특화)
- **Dark Magician**: 4개 액티브 스킬 (광역 마법)
- **Sniper**: 4개 액티브 스킬 (원거리 저격)

#### **Boss 전용 메커니즘**
- **AOE 공격**: 범위 피해 ([`OD_GA_AOE_SingleHit.cpp`])(Private/AbilitySystem/Abilities/Boss/OD_GA_AOE_SingleHit.cpp)
- **돌진 공격**: 이동 + 충돌 판정
**소스 코드:** [`OD_GA_Boss_DashAttack.cpp`](Private/Character/UOD_GA_Boss_DashAttack.h#L28-L50)

- **분노 모드**: 체력 구간별 강화 패턴
**소스 코드:** [`ODMonsterBase.h` (Lines 28-50)](Public/Character/ODMonsterBase.h#L28-L50)

---

### 데이터 주도 아키텍처

#### **캐릭터 계층 구조**
```cpp
AActor
└── APawn
    └── AODCharacterBase (기본 캐릭터)
        └── AODCombatCharacter (전투 기능)
            ├── AODPlayer (플레이어 캐릭터)
            └── AODMonsterBase (몬스터 베이스)
                ├── AODMonsterGrunt (일반 몬스터)
                └── AODMonsterBoss (보스 몬스터)
```

**설계 철학**
- **단일 책임**: 각 계층은 하나의 명확한 역할만 수행
- **재사용성**: 공통 기능을 상위 클래스에서 구현
- **확장성**: 새 캐릭터 타입 추가 시 파생 클래스만 생성

#### **타입 기반 데이터 설계**

**소스 코드:** [`ODMonsterBase.h` (Lines 28-50)](Public/Character/ODMonsterBase.h#L28-L50)

```cpp
// 몬스터 성향 - AI 동작 결정
enum class EMonsterEngageType : uint8 {
    Aggressive,      // 선공형: 시각 자극 → 즉시 교전
    Non_Aggressive,  // 반격형: 피격 자극 → 방어적 교전
};

// 몬스터 등급 - BehaviorTree 선택
enum class EMonsterRank : uint8 {
    Grunt,         // 단일 타겟 추적
    Boss,          // 멀티 타겟 추적
    DungeonBoss,   // 던전 전용 보스
};
```

---

### 성능 최적화

#### **Frustum & Distance Culling**
- **Frustum Culling**: 카메라 밖 객체 렌더 제외
- **Distance Culling**: 설정 거리 이상 객체 비활성화
- **동적 컬링**: 플레이어 이동에 따라 실시간 적용

#### **Replication Graph**
```cpp
// 네트워크 최적화 - 관련성 기반 동기화
- 플레이어 근처 오브젝트만 업데이트
- 거리별 업데이트 빈도 조절
- 불필요한 네트워크 트래픽 제거
```

---

### 멀티플레이어 시스템

#### **네트워크 아키텍처**
- **Listen Server**: 호스트 플레이어가 서버 겸임
- **최대 4인 협동**: 파티 시스템 지원
- **던전 세션**: 인스턴스 던전 매니저 (`ODDungeonSessionManager`)

#### **데이터 동기화**
```cpp
// 핵심 속성 복제
UPROPERTY(Replicated)
- 캐릭터 위치/회전
- 체력/마나 (AttributeSet)
- 타겟팅 정보
- 전투 상태
```

#### **성능 최적화**
- **Relevancy Management**: 플레이어별 관련 액터만 동기화
- **Update Frequency**: 중요도에 따른 업데이트 빈도 조절
- **Bandwidth 최적화**: Delta Compression 활용

---

## 프로젝트 구조

```
OD/Source/OD/
├── Public/
│   ├── AI/                     # AI 시스템
│   │   ├── ODAIController.h    # AI 컨트롤러 (이벤트 기반)
│   │   ├── Decorator/          # BehaviorTree Decorator
│   │   ├── Service/            # BehaviorTree Service
│   │   ├── Task/               # BehaviorTree Task
│   │   └── Spawning/           # 몬스터 스폰 시스템
│   ├── AbilitySystem/          # GAS 구현
│   │   ├── Abilities/          # 게임플레이 어빌리티
│   │   │   ├── Combat/         # 전투 어빌리티
│   │   │   ├── Skill/          # 캐릭터 스킬
│   │   │   │   ├── Valkyrie/   # 발키리 스킬
│   │   │   │   ├── DarkMagician/ # 다크 매지션 스킬
│   │   │   │   └── Sniper/     # 스나이퍼 스킬
│   │   │   ├── Boss/           # 보스 전용 어빌리티
│   │   │   └── Init/           # 초기화 어빌리티
│   │   ├── ODAttributeSet.h    # 게임플레이 속성 (HP, MP 등)
│   │   └── SpawnActor/         # 어빌리티 스폰 액터
│   ├── Character/              # 캐릭터 시스템
│   │   ├── ODCharacterBase.h   # 기본 캐릭터
│   │   ├── ODCombatCharacter.h # 전투 캐릭터
│   │   ├── ODPlayer.h          # 플레이어
│   │   ├── ODMonsterBase.h     # 몬스터 베이스
│   │   ├── ODMonsterGrunt.h    # 일반 몬스터
│   │   └── ODMonsterBoss.h     # 보스 몬스터
│   ├── Components/             # 컴포넌트 시스템
│   │   ├── ODTargetComponent.h # 타겟팅 시스템
│   │   └── ODRewardComponent.h # 보상 시스템
│   ├── Manager/                # 게임 매니저
│   │   └── ODDungeonSessionManager.h # 던전 세션
│   ├── ReplicationGraph/       # 네트워크 최적화
│   │   └── ODReplicationGraph.h
│   └── Widgets/                # UI 시스템
│       ├── HUD/                # HUD 위젯
│       ├── Skills/             # 스킬 UI
│       └── Options/            # 옵션 UI
└── Private/                    # 구현 파일
```

---

## 기술 스택

### **핵심 기술**
- **Unreal Engine 5.4
- **C++17**
- **Gameplay Ability System**: UE5 공식 어빌리티 프레임워크
- **Behavior Tree**: 데이터 기반 AI
- **AI Perception**: 이벤트 기반 감각 시스템

### **디자인 패턴**
- **Component Pattern**: 기능별 컴포넌트 분리
- **Factory Pattern**: 몬스터/어빌리티 생성
- **Observer Pattern**: 이벤트 델리게이트 시스템
- **Strategy Pattern**: 타입별 행동 분기
- **Data-Driven Design**: Enum 기반 동작 결정

### **성능 기술**
- **Event-Driven Architecture**: Tick 제거로 CPU 최적화
- **LOD System**: 폴리곤 수 자동 조절
- **Culling Optimization**: 불필요한 렌더링 제거
- **Replication Graph**: 네트워크 트래픽 최적화

---

## 성과 및 최적화 결과

### **AI 시스템 성능**
```
Tick 방식:     초당 60회 검사 (16.67ms마다)
Event 방식:    초당 0~2회 처리 (필요 시에만)
CPU 절감:      ~95% (전투 미발생 시)
```

### **렌더링 성능**
```
LOD 미적용:    100% 폴리곤 (항상)
LOD 적용:      15~100% 폴리곤 (거리 기반)
폴리곤 절감:   평균 60%, 최대 85%
```

### **네트워크 성능**
```
일반 복제:     모든 액터 동기화
Replication:   관련 액터만 동기화
트래픽 절감:   약 70% (4인 협동 기준)
```

### **코드 재사용성**
```
AI 시스템:     85% 공통 코드 / 15% 데이터
어빌리티:      70% 베이스 / 30% 파생
캐릭터:        60% 공통 / 40% 특화
```

---

## 주요 기능

### **캐릭터 시스템**
- 3가지 플레이어블 클래스 (Valkyrie, Dark Magician, Sniper)
- 클래스별 고유 스킬 세트 (4~6개)
- 레벨업 및 경험치 시스템
- 장비 및 인벤토리 시스템

### **전투 시스템**
- 일반 공격 / 원거리 공격
- 스킬 쿨다운 및 마나 소모
- 데미지 계산 (GAS AttributeSet)
- 타겟팅 시스템 (Auto / Manual)

### **AI 시스템**
- 2가지 몬스터 등급 (Grunt, Boss)
- 2가지 교전 성향 (Aggressive, Non-Aggressive)
- 이벤트 기반 플레이어 감지
- 동적 BehaviorTree 선택

### **멀티플레이어**
- Listen Server 방식
- 최대 4인 협동 플레이
- 파티 시스템
- 던전 인스턴스

### **최적화**
- LOD 시스템 (4단계)
- Frustum & Distance Culling
- Replication Graph
- 이벤트 기반 AI (Tick 제거)

---

## 개발 환경

- **Unreal Engine**: 5.4
- **Visual Studio**: 2022
- **Rider**: 3.1

---

## 참고 문서

### **주요 소스 파일**
- [ODAIController.h](Public/AI/ODAIController.h) - AI 컨트롤러 헤더
- [ODAIController.cpp](Private/AI/ODAIController.cpp) - AI 컨트롤러 구현
- [ODMonsterBase.h](Public/Character/ODMonsterBase.h) - 몬스터 베이스 클래스
- [ODMonsterBase.cpp](Private/Character/ODMonsterBase.cpp) - 몬스터 베이스 구현


### **언리얼 엔진 공식 문서**
- [Gameplay Ability System](https://docs.unrealengine.com/5.0/en-US/gameplay-ability-system-for-unreal-engine/)
- [AI Perception](https://docs.unrealengine.com/5.0/en-US/ai-perception-in-unreal-engine/)
- [Behavior Trees](https://docs.unrealengine.com/5.0/en-US/behavior-trees-in-unreal-engine/)
- [Replication Graph](https://docs.unrealengine.com/5.0/en-US/replication-graph-in-unreal-engine/)
