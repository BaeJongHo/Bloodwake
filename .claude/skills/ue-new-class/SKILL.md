---
name: ue-new-class
description: 언리얼 엔진 5.7에서 새 C++ 클래스(Actor, ActorComponent, UObject, GameInstanceSubsystem, UserWidget 등)를 규약에 맞는 헤더/소스 보일러플레이트로 스캐폴딩한다. 사용자가 "새 액터/컴포넌트/서브시스템/UCLASS 만들어줘", "C++ 클래스 추가" 등을 요청할 때 사용한다.
---

# UE 5.7 새 C++ 클래스 스캐폴딩

언리얼 엔진 5.7 프로젝트에 새 `UCLASS`를 추가할 때 사용하는 워크플로다. 목표는 **에디터의 "New C++ Class" 마법사가 만드는 것과 동일하게 컴파일되는 골격**을 규약(`CLAUDE.md`)에 맞춰 생성하는 것이다.

## 절차

1. **베이스 클래스 결정**: 사용자 의도에 맞는 부모를 고른다.
   | 용도 | 베이스 | 접두사 |
   | --- | --- | --- |
   | 월드에 배치/스폰되는 객체 | `AActor` | `A` |
   | 액터에 붙는 기능 모듈 | `UActorComponent` (틱·트랜스폼 필요 시 `USceneComponent`) | `U` |
   | 데이터/로직 객체 | `UObject` | `U` |
   | 전역 시스템(게임/월드 수명) | `UGameInstanceSubsystem` / `UWorldSubsystem` | `U` |
   | UMG 위젯 | `UUserWidget` | `U` |
   | 폰/캐릭터 | `APawn` / `ACharacter` | `A` |

2. **배치 모듈·경로 확인**: 어느 모듈(`Source/<Module>/`)에 둘지 정한다. 엔진 전용 기능이면 `*Editor` 모듈인지 확인한다. 기존 유사 클래스 위치를 `Glob`으로 참고한다.

3. **헤더(.h) 생성**: 아래 템플릿을 베이스에 맞게 채운다(`references/templates.md` 참고). 핵심 규칙:
   - 첫 줄 `#pragma once`
   - 마지막 include는 반드시 `#include "<ClassName>.generated.h"`
   - 본문 첫 줄 `GENERATED_BODY()`
   - `UObject*` 멤버는 `UPROPERTY()` + `TObjectPtr<T>`

4. **소스(.cpp) 생성**: 첫 include는 자기 짝 헤더. 생성자·오버라이드 정의 작성. 컴포넌트는 `CreateDefaultSubobject`.

5. **빌드 의존성 확인**: 새로 참조하는 모듈(예: `UMG`, `EnhancedInput`, `GameplayAbilities`)이 있으면 모듈의 `*.Build.cs`에 추가하고, 그 경우 **프로젝트 파일 재생성이 필요함**을 사용자에게 알린다.

6. **컴파일**: `CLAUDE.md` 2절 빌드 명령으로 컴파일해 골격이 통과하는지 확인한다.

## 클래스 종류별 빠른 참조 골격

자주 쓰는 베이스의 최소 컴파일 골격은 [references/templates.md](references/templates.md)에 정리돼 있다. 베이스 클래스가 모호하면 사용자에게 한 번 확인한 뒤 진행한다.

## 주의
- `*.generated.h`를 직접 만들지 않는다. UHT가 빌드 시 생성한다.
- 액터/컴포넌트의 `Tick`은 기본 비활성(`PrimaryActorTick.bCanEverTick = false`). 정말 필요할 때만 켠다.
- 블루프린트에서 상속/배치할 클래스는 `UCLASS(Blueprintable)` / `UCLASS(BlueprintType)`를 적절히 부여한다.
