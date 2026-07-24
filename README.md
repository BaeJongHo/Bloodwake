# Bloodwake

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?style=flat&logo=unrealengine&logoColor=white)](https://www.unrealengine.com/)
[![C++](https://img.shields.io/badge/C++-00599C?style=flat&logo=cplusplus&logoColor=white)](https://en.wikipedia.org/wiki/C%2B%2B)
[![AI Assisted](https://img.shields.io/badge/AI%20Assisted-Claude%20Code-D97757?style=flat)](https://github.com/BaeJongHo/Unreal_Harness)
[![Solo Dev](https://img.shields.io/badge/Development-Solo-blue?style=flat)]()

다크 소울 / 엘든 링 계보의 **소울라이크 전투 액션 RPG**입니다. 1인 개발로 진행 중인 개인 사이드 프로젝트이며, 락온 전투·가드/패링·보스전으로 이어지는 소울라이크 핵심 전투 루프를 Unreal Engine 5.7 + C++로 구현하고 있습니다.

---

## 개발 프로세스 — Claude Code + Unreal_Harness

이 프로젝트의 모든 C++ 기능은 제가 직접 설계한 오픈소스 하네스 **[Unreal_Harness](https://github.com/BaeJongHo/Unreal_Harness)** 의 5단계 에이전트 파이프라인으로 개발했습니다.

```
설계(architect) → 구현(implementer) → 빌드(builder) → 리뷰(reviewer) → 문서화(doc-writer)
```

- **권한 분리**: 설계·리뷰 단계는 읽기 전용으로 제한해 "검토 중 임의 수정"을 차단하고, 구현·빌드 단계에만 편집 권한을 부여했습니다.
- **승인 게이트**: 각 단계로 넘어가기 직전 사람이 직접 승인해야 다음 단계가 진행됩니다. AI가 알아서 끝까지 진행하지 않습니다.
- **기능 단위 문서화**: 기능마다 설계 의도(architect 문서)와 구현·리뷰·빌드 결과(doc 문서)가 남습니다. 어떤 코드를 AI가 작성했고, 왜 그 방식을 선택했으며, 리뷰에서 어떤 문제가 발견되어 어떻게 고쳤는지까지 전부 추적할 수 있습니다.
- **UE 특화 안전장치**: GC 추적 누락(`UPROPERTY`+`TObjectPtr`), 생성 파일 편집 등 UE 고유 실수를 규약·훅·리뷰어가 삼중으로 점검합니다.

이 프로세스로 **19개 기능을 39개 커밋에 걸쳐 약 6주간(2026-06-06 ~ 2026-07-19)** 개발했습니다. 각 기능 문서에는 기술 선택 이유, 코드 리뷰에서 지적된 이슈와 처리 여부(수정 완료 / 현행 유지 / 후속 과제), 빌드 검증 결과까지 기록되어 있어 AI가 작성한 코드에 대해서도 제가 직접 이해·검수·설명할 수 있습니다.

> 설계·구현 문서(`Feature/`, `.claude/`, `CLAUDE.md`)는 진행 중인 개인 작업 문서라 리포지토리에는 포함하지 않았습니다(`.gitignore`). 필요 시 인터뷰 등에서 직접 공유해 드릴 수 있습니다.

---

## 전투 · 게임플레이 시스템

### 캐릭터 기반
- **스탯 시스템** — Health / Stamina / Focus 3종을 관리하는 `UBWAttributeComponent`, 델리게이트 기반 HUD 연동
- **상태 관리** — `FGameplayTagContainer` 기반 `UBWStateComponent`로 Sprint / Roll / Attack 등 행동 상태를 단일 지점에서 관리
- **Sprint(Hold) / Roll(Tap)** — 동일 입력 키를 홀드·탭으로 분기하는 Enhanced Input 구조
- **메인 HUD** — 자원 현황을 실시간으로 표시

### 장비 · 상호작용
- 픽업(Interact) → 장비 스폰·소켓 부착
- 무기 · 방패 **듀얼 슬롯 독립 토글**(손 ↔ 등), 같은 슬롯 교체 시 기존 장비를 바닥에 드롭
- **몽타주 기반 장착/해제 연출** — AnimNotify 시점에 실제 소켓 이동
- **방어구 시스템** — Chest / Pants / Boots / Gloves 4부위, 방어력 스탯 누적

### 근접 전투
- **콤보 어택** — 약공격 · 강공격 · 대쉬 공격 · 특수 공격 4종, 몽타주·스태미나 수치는 DataTable로 외부화
- **무기 콜리전 · 데미지** — 소켓 간 Sweep 트레이스로 히트 윈도우 판정, 피격 방향별 리액션·VFX·사운드
- **양손 무기 / 맨손(Fists) 전투** — 무기 종류별 전용 소켓·몽타주 세트
- **락온(Lock-On) 타겟팅** — 화면 중앙 최근접 적 자동 탐색, 8방향 스트레이프 이동

### 가드 · 반격
- **방패 블로킹** — 가드 성공/실패 분기, 스태미나 소모, 후방 피격 시 폴백
- **패링 + 적 스턴** — 타이밍 기반 공격 무효화와 반격 기회 창출
- **무적 프레임(i-frame)** — 구르기 회피 구간 연동, 2중 안전망으로 무적 영구 지속 방지

### 회복
- **에스트병식 포션 회복** — 마시는 도중 피격 시 즉시 취소되는 소울라이크 표준 동작

### 적 AI
- **순찰 · 시야 인식** — BehaviorTree / Blackboard + AI Perception(Sight) 기반
- **데미지 감각** — 시야 밖 피격도 AI Perception 자극으로 인지해 추격 개시
- **HP 바 · 행동 패턴** — Idle / Patrol / Approach / MeleeAttack 4상태를 BT Service가 주기 평가
- **공격 행동** — `IBWCombatInterface` 공통 계약으로 플레이어·적 양쪽이 동일 인터페이스 사용

### 보스전
- 화면 상단 고정 2D 보스 HP 바
- 보스 전용 행동 결정 서비스, 공격 windup 구간 텔레그래프 회전
- **넉다운 공격 · 스트레이프 이동(간보기) · 공중 스페셜 어택**
- **보스 BGM** — 어그로 획득/해제, 사망에 연동한 페이드 인/아웃
- 사망 시 무기 낙하(물리 시뮬레이션)

### UI
- 장비(무기·방패) 아이콘 위젯 — 델리게이트 기반 자동 갱신

---

## 기술 스택

| 분류 | 기술 |
| --- | --- |
| 엔진 | Unreal Engine 5.7 |
| 언어 | C++ |
| AI | Behavior Tree, Blackboard, AI Perception (Sight / Damage) |
| 상태 관리 | GameplayTags |
| 애니메이션 | Animation Montage, AnimNotify / AnimNotifyState, MetaHuman |
| UI | UMG |
| 개발 도구 | Claude Code + 자체 설계 하네스 ([Unreal_Harness](https://github.com/BaeJongHo/Unreal_Harness)) |
| VCS | Git |

---

## 프로젝트 구조

```
Source/Bloodwake/
├── AI/          # BT Task/Service, AI Perception, 행동 판단
├── Audio/       # 보스 BGM 서브시스템
├── Character/   # 플레이어 / 적 / 보스 캐릭터
├── Combat/      # 공격, 피격, 콤보, 가드, 패링, i-frame
├── Core/        # 공용 인터페이스 · 타입
├── Equipment/   # 픽업, 무기/방패, 방어구
├── GameMode/    # 게임모드, HUD 초기화
├── Player/      # 플레이어 입력 · 이동
└── UI/          # HUD, 락온 마커, HP 바, 장비 위젯
```

---

## 개발자

**배종호 (Bae Jongho)** — Game Client Developer

- 📧 Email: jhtop96@gmail.com
- 📝 Blog: https://jhtop0419.tistory.com/
- 💼 Career: 넷마블 → 스마일게이트 RPG (로스트아크 모바일)
- 🛠 AI 개발 하네스: https://github.com/BaeJongHo/Unreal_Harness
