# 에이전트 파이프라인 설계

이 하네스의 핵심은 UE 5.7 기능 개발을 **역할이 분리된 5개 서브에이전트의 파이프라인**으로 처리하는 것이다.
단일 에이전트가 설계·구현·빌드·리뷰·문서화를 모두 하면 컨텍스트가 뒤섞이고 GC 같은 UE 특화 함정을 놓치기 쉽다.
각 단계를 분리하면 (1) 단계별로 권한(도구)을 좁혀 안전하고, (2) 리뷰가 독립적이라 더 객관적이며, (3) 실패 지점이 명확해진다.

## 전체 흐름

```
                          /ue-feature "<기능 설명>"
                                   │
                    ┌──────────────┴───────────────┐
                    │   오케스트레이터 (메인 세션)   │
                    └──────────────┬───────────────┘
                                   │ 각 단계 진입 전 사용자 승인(게이트), 산출물을 단계 간 전달
   ┌───────────────┐   ┌───────────────┐   ┌───────────────┐   ┌───────────────┐   ┌───────────────┐
   │ ue-architect  │─▶ │ ue-implementer│─▶ │  ue-builder   │─▶ │  ue-reviewer  │─▶ │ ue-doc-writer │
   │   설계 (RO)   │   │   구현 (RW)   │   │   빌드 (RW)   │   │   리뷰 (RO)   │   │  문서화 (md)  │
   └───────────────┘   └───────▲───────┘   └───────────────┘   └───────┬───────┘   └───────────────┘
       ▲ 승인          ▲ 승인  │           ▲ 승인               ▲ 승인  │            ▲ 승인
       │               │       └──── 🔴 Blocking 수정 (최대 1회) ◀──────┘
       └─────────── 각 단계 실행 전 gate-pipeline 훅이 사용자 승인을 요청 ───────────┘
```

`RO` = 읽기 전용 도구, `RW` = 편집 가능 도구.

## 승인 게이트

`/ue-feature` 실행 중 각 서브에이전트가 시작되기 **직전**에, `Task` 도구에 걸린 PreToolUse 훅([gate-pipeline.ps1](../.claude/hooks/gate-pipeline.ps1))이 `permissionDecision: ask`를 반환해 사용자 승인을 요청한다. 승인해야만 그 단계(= 다음 에이전트)로 넘어가고, 거부하면 멈춘다. 파이프라인 5개 에이전트(`ue-architect`/`ue-implementer`/`ue-builder`/`ue-reviewer`/`ue-doc-writer`)에만 적용되며, 그 외 서브에이전트는 간섭하지 않는다.

## 단계별 역할

| 단계 | 에이전트 | 도구 권한 | 입력 → 출력 |
| --- | --- | --- | --- |
| 1 설계 | [`ue-architect`](../.claude/agents/ue-architect.md) | Read, Grep, Glob, WebSearch, WebFetch, PowerShell, Write | 기능 요청 → 구현 계획 (+ `Feature\architect\`에 저장) |
| 2 구현 | [`ue-implementer`](../.claude/agents/ue-implementer.md) | + Edit, Write, PowerShell, Bash | 구현 계획 → 코드 변경 |
| 3 빌드 | [`ue-builder`](../.claude/agents/ue-builder.md) | Read, Grep, Glob, Edit, PowerShell, Bash | 변경 → 빌드 성공 |
| 4 리뷰 | [`ue-reviewer`](../.claude/agents/ue-reviewer.md) | Read, Grep, Glob, PowerShell, Bash | 변경 → 리뷰 리포트 |
| 5 문서화 | [`ue-doc-writer`](../.claude/agents/ue-doc-writer.md) | Read, Grep, Glob, PowerShell, Bash, Write | 전체 산출물 → 개발 문서 (+ `Feature\doc\`에 저장) |

> 산출 문서는 프로젝트 루트 기준으로 저장된다: 1단계 설계 문서 → `Feature\architect\{날짜}_{기능명}.md`, 5단계 개발 문서 → `Feature\doc\{날짜}_{기능명}.md` (경로는 각 에이전트 정의에서 변경 가능).

## 설계 원칙

- **권한 최소화**: 설계·리뷰 단계는 파일을 못 바꾸게 막아 "검토하다 몰래 고치는" 오염을 방지한다.
- **산출물 계약**: 각 에이전트는 정해진 형식으로 결과를 내고, 오케스트레이터가 그 전문을 다음 단계에 넘긴다.
- **유한 루프**: 빌드 수정은 최대 5회, 리뷰 보정 루프는 최대 1회. 막히면 멈추고 사람에게 보고한다.
- **버전 정합성**: 1단계에서 UE 5.7 API를 공식 문서로 검증해 버전 변경으로 깨지는 코드를 예방한다.

## 사용법

```
/ue-feature 플레이어가 'F' 키로 상호작용할 수 있는 InteractableComponent 추가
```

작은 수정(오타, 한 줄 로직)은 파이프라인 없이 메인 세션에서 직접 처리해도 된다.
개별 에이전트만 쓰고 싶으면 Claude에게 "ue-reviewer로 이 변경만 리뷰해줘"처럼 직접 지정할 수도 있다.

## 커스터마이즈

- 멀티플레이가 없는 프로젝트면 `ue-reviewer`의 E(리플리케이션) 섹션 비중을 낮춘다.
- 빌드가 매우 오래 걸리면 `ue-builder`가 `run_in_background`로 돌리도록 이미 지시돼 있다(필요 시 타임아웃만 조정).
- 테스트(자동화 스펙, Gauntlet)가 있다면 4단계 뒤에 `ue-tester` 에이전트를 추가하는 식으로 확장할 수 있다.
