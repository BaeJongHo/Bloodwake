---
name: ue-crash-triage
description: 언리얼 엔진 5.7 크래시/콜스택/로그(.log, Crash 콜스택, check/ensure 실패, GPF)를 분석해 근본 원인을 추정하고 수정 방향을 제시한다. 사용자가 크래시 로그나 콜스택을 붙여넣거나 "에디터가 죽었어/크래시 분석해줘"라고 할 때 사용한다.
---

# UE 5.7 크래시 분류 (Crash Triage)

언리얼 엔진 크래시를 체계적으로 분석한다. 목표는 콜스택의 **첫 사용자 코드 프레임**과 **크래시 종류**로부터 근본 원인을 좁히는 것이다.

## 입력 수집
- 콜스택(붙여넣은 텍스트) 또는 로그 파일 경로. 로그는 보통 `<PROJECT_ROOT>/Saved/Logs/<ProjectName>.log`, 크래시는 `<PROJECT_ROOT>/Saved/Crashes/.../*.log`에 있다.
- 로그가 길면 `Error`, `Fatal`, `Assertion failed`, `Ensure condition failed`, `=== Critical error` 키워드 주변을 우선 본다.

## 분석 절차
1. **크래시 종류 식별** — 메시지 앞부분으로 분류한다.
   | 신호 | 의미 | 자주 있는 원인 |
   | --- | --- | --- |
   | `Access violation - code c0000005` / `EXCEPTION_ACCESS_VIOLATION` | 잘못된 메모리 접근(GPF) | null/댕글링 포인터 역참조, GC된 UObject 접근 |
   | `Assertion failed: ...` (`check()`) | 불변식 위반 | 전제 조건 깨짐, 잘못된 인덱스/상태 |
   | `Ensure condition failed: ...` | 복구 가능한 경고(보통 비치명) | 경계 상황, null 가능 객체 미확인 |
   | `Fatal error: ... Out of memory` | 메모리 고갈 | 누수, 과도한 로드, 무한 스폰 |
   | `LoadErrors` / `Failed to load` | 에셋 로드 실패 | 경로/참조 깨짐, 리다이렉터 |

2. **첫 사용자 코드 프레임 찾기** — 콜스택 위에서부터 엔진 프레임(`UnrealEditor-Core`, `...-Engine`)을 건너뛰고 **프로젝트 모듈/클래스가 처음 나오는 프레임**을 찾는다. 그 함수가 1차 용의자다.

3. **소스 확인** — 해당 파일을 `Read`해서 의심 지점을 본다. GPF면 그 함수에서 역참조하는 포인터들을 점검한다:
   - `GetWorld()`, `GetOwner()`, `GetGameInstance()` 등이 null일 수 있는 시점(생성자/`BeginPlay` 이전)에 호출됐는가?
   - `Cast<T>()` 결과를 null 체크 없이 썼는가?
   - **`UPROPERTY()` 없이 보관한 `UObject*`가 GC된 뒤 접근**됐는가? (가장 흔한 댕글링 원인 — `CLAUDE.md` 3.3)
   - `TWeakObjectPtr` 사용 전 `IsValid()` 누락?
   - 배열 인덱스 범위 초과(`Array[Index]`에서 `check` 실패)?

4. **가설·검증·수정 제안** — 근본 원인 가설을 세우고 코드 근거(`파일:줄`)로 뒷받침한다. 수정 방향을 제시한다:
   - null 가드 추가(`if (!IsValid(Ptr)) return;`)
   - 포인터 멤버에 `UPROPERTY()` 누락 보강
   - 호출 시점 이동(`BeginPlay`/`OnRegister`로)
   - 인덱스/상태 검증

## 산출물 형식
```
## 크래시 분류
- 종류: <GPF / check 실패 / OOM ...>
- 첫 사용자 프레임: <함수> (<파일:줄>)

## 근본 원인 (추정)
<가설 + 코드 근거>

## 수정 제안
<구체적 변경. 필요 시 ue-implementer로 넘길 수 있게 명확히>

## 재발 방지
<check/ensure 추가, 패턴 교정 등>
```

## 주의
- 콜스택 심볼이 없으면(`UnknownFunction`) Shipping/심볼 없는 빌드일 수 있음을 알리고, Development Editor 빌드로 재현을 권한다.
- 직접 수정까지 해도 되는 맥락이면 수정 후 `CLAUDE.md` 빌드 명령으로 컴파일 확인한다.
