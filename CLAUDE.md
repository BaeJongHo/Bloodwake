# CLAUDE.md — 언리얼 엔진 5.7 프로젝트 규약

이 문서는 Claude Code가 본 언리얼 엔진(UE) 5.7 C++ 게임 프로젝트에서 작업할 때 **항상 준수해야 하는 규약**이다.
모든 서브에이전트(`.claude/agents/`)와 스킬(`.claude/skills/`)은 이 규약을 기준으로 동작한다.

> 이 저장소는 **하네스 템플릿**이다. 실제 UE 프로젝트에 적용하는 방법은 [docs/INSTALL.md](docs/INSTALL.md)를 참고한다.
> 적용 시 아래의 `<...>` 자리표시자를 프로젝트 실제 값으로 교체한다.

---

## 1. 프로젝트 기본 정보 (적용 시 채워 넣기)

| 항목 | 값 |
| --- | --- |
| 프로젝트명 | `<ProjectName>` |
| 주 게임 모듈 | `Source/<ProjectName>/` |
| 에디터 전용 모듈 | `Source/<ProjectName>Editor/` (있는 경우) |
| 엔진 버전 | UE 5.7 |
| 엔진 설치 경로 | `<UE_5.7_ENGINE_PATH>` (예: `C:\Program Files\Epic Games\UE_5.7`) |
| `.uproject` 경로 | `<PROJECT_ROOT>\<ProjectName>.uproject` |

---

## 2. 빌드 / 실행 명령 (Windows · PowerShell 기준)

작업 검증은 반드시 **실제 빌드**로 확인한다. 컴파일되지 않은 코드는 "완료"가 아니다.

```powershell
# (1) 프로젝트 파일 생성 — .Build.cs / 모듈 구조를 바꾼 뒤 실행
& "<UE_5.7_ENGINE_PATH>\Engine\Build\BatchFiles\Build.bat" `
  -projectfiles -project="<PROJECT_ROOT>\<ProjectName>.uproject" -game -engine

# (2) 에디터 타깃 빌드 (가장 자주 사용) — Development Editor
& "<UE_5.7_ENGINE_PATH>\Engine\Build\BatchFiles\Build.bat" `
  <ProjectName>Editor Win64 Development `
  -project="<PROJECT_ROOT>\<ProjectName>.uproject" -waitmutex

# (3) 에디터 실행
& "<UE_5.7_ENGINE_PATH>\Engine\Binaries\Win64\UnrealEditor.exe" `
  "<PROJECT_ROOT>\<ProjectName>.uproject"

# (4) 쿠킹/패키징 (배포 빌드)
& "<UE_5.7_ENGINE_PATH>\Engine\Build\BatchFiles\RunUAT.bat" `
  BuildCookRun -project="<PROJECT_ROOT>\<ProjectName>.uproject" `
  -noP4 -platform=Win64 -clientconfig=Shipping -cook -build -stage -pak -archive `
  -archivedirectory="<PROJECT_ROOT>\Build"
```

- 빌드 로그가 길면 마지막 30~50줄만 확인하고, `error`/`warning` 키워드로 필터링한다.
- `-waitmutex`는 에디터가 켜진 채 빌드할 때 충돌을 막는다. **에디터 실행 중 핫리로드용 빌드에는 반드시 포함**한다.
- 헤더만 바꾼 경우라도 `.generated.h` 의존성 때문에 전체 재컴파일이 필요할 수 있다.

---

## 3. C++ 코딩 규약 (Epic 표준 기반)

### 3.1 네이밍 접두사
| 접두사 | 대상 | 예 |
| --- | --- | --- |
| `U` | `UObject` 파생 클래스 | `UHealthComponent` |
| `A` | `AActor` 파생 클래스 | `APlayerCharacter` |
| `F` | 일반 구조체 / 비-UObject 클래스 | `FDamageEvent` |
| `E` | 열거형 (`enum class` 권장) | `EWeaponState` |
| `I` | 인터페이스 | `IInteractable` |
| `T` | 템플릿 | `TArray`, `TSubclassOf` |
| `b` | bool 변수 | `bIsDead`, `bCanJump` |

- 함수·변수는 `PascalCase`. 지역 변수도 `PascalCase`를 따른다(언더스코어/카멜케이스 금지).
- 헤더 파일명 = 주 클래스명(접두사 제외). 예: `UHealthComponent` → `HealthComponent.h`.

### 3.2 UPROPERTY / UFUNCTION
- 에디터 노출/직렬화/리플리케이션이 필요하면 `UPROPERTY`를 단다.
- **GC 추적이 필요한 모든 `UObject*` 멤버는 반드시 `UPROPERTY()`로 선언**한다(3.3 참고).
- 블루프린트 노출은 최소 권한 원칙: 꼭 필요할 때만 `BlueprintReadOnly`/`BlueprintCallable`을 사용하고, 쓰기 노출(`BlueprintReadWrite`)은 신중히.
- 흔한 지정자:
  - `UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="...")`
  - `UPROPERTY(VisibleAnywhere, BlueprintReadOnly)` — 컴포넌트 핸들 등
  - `UFUNCTION(BlueprintCallable, Category="...")`
  - 리플리케이션: `UPROPERTY(ReplicatedUsing=OnRep_Foo)` + `GetLifetimeReplicatedProps` 갱신 필수.
- 모든 `UPROPERTY`/`UFUNCTION`에는 의미 있는 `Category`를 부여한다.

### 3.3 메모리 · 가비지 컬렉션 (UE에서 가장 중요)
- `UObject` 포인터 멤버는 **`TObjectPtr<T>` + `UPROPERTY()`** 로 선언한다(UE5 표준). 원시 포인터로 두면 GC가 추적하지 못해 댕글링이 발생한다.
  ```cpp
  UPROPERTY(VisibleAnywhere)
  TObjectPtr<UHealthComponent> HealthComponent;
  ```
- 소유하지 않는 약한 참조는 `TWeakObjectPtr<T>`를 사용한다.
- 객체 생성:
  - 일반 `UObject` → `NewObject<T>(Outer)`
  - 액터 → `GetWorld()->SpawnActor<T>(...)`
  - 컴포넌트 → 생성자에서 `CreateDefaultSubobject<T>(TEXT("Name"))`
- `delete`로 `UObject`를 해제하지 않는다. 명시적 파괴는 `Actor->Destroy()` / `MarkAsGarbage()`.
- `TArray<TObjectPtr<T>>`, `TMap` 등 컨테이너도 `UPROPERTY()`가 있어야 원소가 GC 추적된다.

### 3.4 include 규칙 (IWYU)
- UE 5.7은 IWYU(Include What You Use)를 강제한다. `Engine.h` 같은 모놀리식 헤더 금지.
- 헤더에서는 **전방 선언**을 우선하고, 실제 사용하는 `.cpp`에서 구체 헤더를 include 한다.
- 각 `.cpp`의 첫 include는 자기 짝 헤더(`#include "HealthComponent.h"`)여야 한다.
- `*.generated.h`는 항상 **해당 클래스 헤더의 마지막 include**여야 한다.

### 3.5 로깅 / 어서션
- `printf`/`std::cout` 금지. 로깅은 `UE_LOG`:
  ```cpp
  UE_LOG(LogTemp, Warning, TEXT("Health = %f"), CurrentHealth);
  ```
  - 모듈 전용 로그 카테고리(`DECLARE_LOG_CATEGORY_EXTERN`)를 정의해 `LogTemp` 남용을 피한다.
- 문자열 리터럴은 항상 `TEXT("...")`로 감싼다.
- 불변식 검사: 치명적 위반은 `check()`, 복구 가능/디버그 경고는 `ensure()` / `ensureMsgf()`.

---

## 4. 성능 규약

> **추측 최적화 금지.** 항상 측정(`stat unit`, `stat game`, `stat gpu`, Unreal Insights) 후 병목부터 처리한다.
> 아래는 측정 이전에도 기본으로 지켜야 할 사항이다.

### 4.1 Tick 최소화 (이벤트 우선)
- 액터/컴포넌트의 Tick은 **기본 비활성**으로 둔다. 생성자에서:
  - 액터: `PrimaryActorTick.bCanEverTick = false;`
  - 컴포넌트: `PrimaryComponentTick.bCanEverTick = false;`
  - 매 프레임 갱신이 정말 필요할 때만 `true`로 켠다.
- 매 프레임이 필요 없는 로직은 **이벤트 기반**으로 대체한다:
  - 주기적 작업 → 타이머 `GetWorld()->GetTimerManager().SetTimer(...)` (틱보다 긴 간격)
  - 상태 변화 → 델리게이트/콜백, `OnRep_` 함수, 오버랩·히트 이벤트(`OnComponentBeginOverlap` 등)
  - 입력 → Enhanced Input 액션 바인딩(매 틱 폴링 대신)
- 틱이 불가피하면 빈도를 낮춘다: `PrimaryActorTick.TickInterval = 0.1f;` 처럼 간격을 주거나, 필요 없을 때 `SetActorTickEnabled(false)` / `SetComponentTickEnabled(false)`로 끈다.
- 매 틱에서 비싼 탐색을 반복하지 않는다: `GetAllActorsOfClass`, `FindComponentByClass`, `Cast`, `GetFirstPlayerController` 등은 `BeginPlay`에서 **한 번 캐시**한다(`TObjectPtr`/`TWeakObjectPtr`).

### 4.2 오브젝트 풀링
- 자주 스폰/파괴되는 객체(발사체, 히트 이펙트, 적, 데미지 텍스트 등)는 매번 `SpawnActor`/`Destroy` 대신 **풀에서 재사용**한다. 스폰·GC 비용과 프레임 힛치를 줄인다.
- 반환(비활성화): `SetActorHiddenInGame(true)` + `SetActorEnableCollision(false)` + `SetActorTickEnabled(false)`. 꺼낼 때 역순으로 활성화하고 내부 상태를 초기화한다.
- 풀은 `UWorldSubsystem` / `UGameInstanceSubsystem`이나 전용 매니저로 관리한다(전역 접근 · 수명 일원화).

### 4.3 일반 최적화
- 컨테이너는 예상 크기를 `Reserve()`로 예약해 루프 중 재할당을 피한다. 루프에서 컨테이너·구조체를 값 복사하지 말고 `const&`/참조로 받는다.
- 매 프레임 동적 할당·문자열 포매팅(`FString::Printf`)을 자제한다. 잦은 로그는 `Verbose` 이하로 두고 쉬핑에서 빠지게 한다.
- 런타임에 머티리얼 파라미터를 바꾸려면 `CreateDynamicMaterialInstance` 결과를 **캐시**해 재사용한다(매 호출 생성 금지).
- 충돌/오버랩은 필요한 채널만 켠다. 불필요한 `SetGenerateOverlapEvents(true)`·복잡 충돌 사용을 피한다.
- 무거운 루프·수학 연산은 블루프린트보다 **C++로** 구현한다.
- 타이머·델리게이트 핸들은 객체 파괴 시 해제한다(`ClearTimer`, `RemoveDynamic`) — 누수와 댕글링 콜백 방지.

---

## 5. 모듈 구조 규약
- 런타임 로직은 게임 런타임 모듈에, 에디터 전용 코드(커스텀 디테일 패널, 에셋 액션 등)는 `*Editor` 모듈에 둔다.
- 새 모듈 의존성은 해당 모듈의 `*.Build.cs`의 `PublicDependencyModuleNames` / `PrivateDependencyModuleNames`에 추가한다.
- `.Build.cs`를 수정하면 **2절 (1) 프로젝트 파일 생성을 다시 실행**한다.
- 모듈 간 순환 의존을 만들지 않는다.

---

## 6. 에셋 / 콘텐츠 규약

> `.uasset`/`.umap`은 Claude가 직접 편집할 수 없다(7절). 에셋 자체 작업은 에디터에서 수행하되,
> **네이밍 · 폴더 구조 · 참조 규칙**은 코드 작성과 리뷰에서도 일관되게 강제한다.

### 6.1 에셋 네이밍 접두사
커뮤니티 표준(UE5 Style Guide) 기준이며 팀 사정에 맞게 조정할 수 있으나, **프로젝트 내 일관성**을 최우선한다. 이름은 PascalCase, 공백·특수문자 금지.

| 접두사 | 에셋 타입 |
| --- | --- |
| `BP_` | 블루프린트 클래스 |
| `BPI_` | 블루프린트 인터페이스 |
| `WBP_` | 위젯 블루프린트(UMG) |
| `ABP_` | 애님 블루프린트 |
| `SM_` | 스태틱 메시 |
| `SK_` | 스켈레탈 메시 |
| `SKEL_` | 스켈레톤 |
| `AS_` / `AM_` / `BS_` | 애님 시퀀스 / 애님 몽타주 / 블렌드 스페이스 |
| `M_` / `MI_` / `MF_` | 머티리얼 / 머티리얼 인스턴스 / 머티리얼 함수 |
| `T_` | 텍스처 (용도 접미사 `_D`, `_N`, `_ORM` 등) |
| `NS_` / `NE_` | 나이아가라 시스템 / 이미터 |
| `SW_` / `SC_` | 사운드 웨이브 / 사운드 큐 |
| `DT_` / `DA_` | 데이터 테이블 / 데이터 에셋 |
| `IA_` / `IMC_` | Enhanced Input 액션 / 매핑 컨텍스트 |
| `GA_` / `GE_` | Gameplay Ability / Gameplay Effect (GAS) |
| `PM_` | 피직스 머티리얼 |
| `Curve_` | 커브 에셋 |

- 레벨/맵은 `Maps/` 폴더에 두고 용도를 알 수 있게 명명한다.
- 텍스처 등은 접미사로 용도를 구분한다(`T_Rock_D`=Diffuse, `_N`=Normal, `_ORM`=Occlusion/Roughness/Metallic 패킹 등).

### 6.2 폴더 구조
- 모든 프로젝트 에셋은 `Content/<ProjectName>/` 아래에 둔다. **`Content` 루트에 에셋을 흩뿌리지 않는다.**
- 타입이 아닌 **기능(feature) 중심**으로 먼저 묶고, 그 안에서 타입별로 나눈다.
  ```
  Content/<ProjectName>/
    Characters/Player/      (BP_, SK_, ABP_, AS_ ...)
    Weapons/Rifle/
    UI/HUD/
    Maps/
    Audio/
    Core/                   (공용 BP, 데이터, 머티리얼)
  ```
- 마켓플레이스·서드파티 에셋은 자기 최상위 폴더를 유지한다(프로젝트 에셋과 섞지 않는다).
- 실험·임시 작업은 `Developers/` 폴더에서 하고, 정식 위치로 옮긴 뒤 정리한다(쿠킹 제외 대상).

### 6.3 레퍼런스 / 리다이렉터 관리
- 에셋을 이동·이름변경하면 언리얼이 **리다이렉터(redirector)**를 남긴다. 방치하면 참조가 꼬이고 쿠킹 경고가 발생한다.
  - 콘텐츠 브라우저에서 대상 폴더 우클릭 → **"Fix Up Redirectors in Folder"** 로 정리한다.
  - 대량 처리는 커맨드릿(플래그는 버전에 맞게 확인): `UnrealEditor-Cmd.exe <project> -run=ResavePackages -fixupredirects`.
- **하드 vs 소프트 레퍼런스**: 항상 메모리에 올릴 필요가 없는 큰 에셋(레벨별 콘텐츠, 선택적 에셋)은 `TSoftObjectPtr<T>` / `TSoftClassPtr<T>`로 참조해 로딩·쿠킹 비용을 줄인다. 비동기 로드는 `FStreamableManager`를 쓴다.
- C++에서 에셋을 참조할 때 **경로 문자열 하드코딩을 지양**한다. `UPROPERTY(EditDefaultsOnly)`로 에디터에서 지정하거나 `DataAsset`/Config로 관리한다.
- 참조 점검: **Reference Viewer**(참조 그래프)와 **Size Map**(메모리)으로 순환 참조·과도한 하드 레퍼런스를 정기 점검한다.

---

## 7. 절대 건드리지 말 것 (금지 경로)
다음은 생성/캐시 산출물이다. 직접 편집·생성·삭제하지 않는다(`guard-generated` 훅이 차단한다).
- `*.generated.h`, `*.gen.cpp`
- `Intermediate/`, `Binaries/`, `DerivedDataCache/`, `Saved/`
- `.uasset` / `.umap` (바이너리 — Claude가 직접 편집 불가, 에디터에서 작업)

---

## 8. 작업 방식 (파이프라인)
기능 단위 작업은 `/ue-feature` 커맨드가 구동하는 **5단계 에이전트 파이프라인**을 따른다.
상세는 [docs/PIPELINE.md](docs/PIPELINE.md) 참고.

```
요청 ─▶ ue-architect(설계) ─▶ ue-implementer(구현) ─▶ ue-builder(빌드) ─▶ ue-reviewer(리뷰) ─▶ ue-doc-writer(문서화) ─▶ 완료
```

- 각 에이전트 실행 직전 `gate-pipeline` 훅이 사용자 승인을 요청한다. 승인해야 다음 단계로 넘어간다.
- 1단계 설계 문서는 `Feature\architect\`, 5단계 개발 문서는 `Feature\doc\`(둘 다 프로젝트 루트 기준)에 md로 저장된다.
- 작은 수정은 파이프라인 없이 직접 처리해도 된다.
- 커밋/푸시는 사용자가 명시적으로 요청할 때만 수행한다. 커밋 메시지 끝에는 Co-Authored-By 트레일러를 붙인다.

---

## 9. 커밋 규약
- 한 커밋은 하나의 논리적 변경. 생성 산출물(7절)은 절대 커밋하지 않는다.
- 메시지: 한 줄 요약(한국어 가능) + 필요 시 본문. 예: `feat(combat): 근접 공격 히트 판정 추가`.
