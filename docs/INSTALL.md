# 하네스 적용 가이드 (INSTALL)

이 저장소는 **언리얼 엔진 5.7 + Claude Code** 하네스 템플릿이다. 실제 UE 프로젝트에 적용하는 방법.

## 1. 파일 복사
이 저장소의 다음을 대상 UE 프로젝트 루트(`.uproject`가 있는 폴더)에 복사한다.

```
CLAUDE.md
docs/
.claude/
  ├─ agents/        (ue-architect, ue-implementer, ue-builder, ue-reviewer)
  ├─ commands/      (ue-feature)
  ├─ skills/        (ue-new-class, ue-crash-triage)
  ├─ hooks/         (format-source.ps1, guard-generated.ps1)
  └─ settings.json
```

> 이미 `CLAUDE.md`가 있는 프로젝트라면 내용을 병합한다.

## 2. 자리표시자 채우기
`CLAUDE.md` 1절의 표와 본문 `<...>` 자리표시자를 실제 값으로 교체한다. 최소한 다음은 필수:
- `<ProjectName>` — 프로젝트/주 모듈 이름
- `<UE_5.7_ENGINE_PATH>` — 엔진 설치 경로 (예: `C:\Program Files\Epic Games\UE_5.7`)
- `<PROJECT_ROOT>` — `.uproject`가 있는 절대 경로
- `<MODULE_API>` — 모듈 API 매크로(예: `MYGAME_API`). 기존 클래스 헤더에서 확인 가능.

## 3. 동작 확인
1. 프로젝트에서 Claude Code를 실행한다.
2. 서브에이전트 인식: `ue-architect`, `ue-implementer`, `ue-builder`, `ue-reviewer`가 보이는지 확인.
3. 스킬 인식: `ue-new-class`, `ue-crash-triage`.
4. 파이프라인 시험:
   ```
   /ue-feature 테스트용으로 빈 ATestActor 액터 하나 추가
   ```

## 4. 훅 동작 (Windows · PowerShell)
- **gate-pipeline** (PreToolUse · `Task`): `/ue-feature` 파이프라인의 각 서브에이전트가 시작되기 직전에 사용자 승인을 요청한다. 승인해야 다음 단계로 넘어간다. 파이프라인 5개 에이전트에만 적용되고 그 외 서브에이전트는 통과한다.
- **guard-generated** (PreToolUse): `*.generated.h`, `Intermediate/`, `Binaries/`, `Saved/`, `.uasset` 편집 시도를 차단.
- **format-source** (PostToolUse): 수정된 `.cpp/.h`에 `clang-format -i` 적용. `clang-format`이 없으면 조용히 통과.
  - clang-format 경로를 직접 지정하려면 환경변수 `CLANG_FORMAT_PATH`에 실행 파일 경로를 설정한다.
  - 프로젝트에 `.clang-format` 규칙 파일을 두면 그 스타일을 따른다(없으면 LLVM 기본).
- 훅 명령은 프로젝트 루트 기준 상대경로(`.claude\hooks\...`)다. 만약 훅이 동작하지 않으면, 사용 중인 OS/셸에 맞게 `settings.json`의 `command`를 절대경로 또는 `$CLAUDE_PROJECT_DIR\.claude\hooks\...` 형태로 조정한다.
- macOS/Linux를 쓴다면 `.ps1` 훅을 `.sh`로 포팅하고 `settings.json`의 `command`를 `bash` 호출로 바꾼다.

## 5. 문서 산출 경로 (프로젝트 루트 기준)
파이프라인은 두 종류의 md 문서를 **프로젝트 루트 아래**에 저장한다:
- 1단계 `ue-architect` 설계 문서 → `Feature\architect\{날짜}_{기능명}.md`
- 5단계 `ue-doc-writer` 개발 문서 → `Feature\doc\{날짜}_{기능명}.md`

다른 위치에 저장하려면 각 에이전트 정의([ue-architect.md](../.claude/agents/ue-architect.md)의 "설계 문서 저장", [ue-doc-writer.md](../.claude/agents/ue-doc-writer.md)의 "산출 위치") 경로만 바꾸면 된다. 이 문서들을 git에서 제외하려면 `Feature/`를 `.gitignore`에 추가한다.

## 6. 권한
`settings.json`의 `permissions.allow`에는 안전한 git 읽기 명령과 **UE 빌드 도구(`Build.bat` / `RunUAT.bat` / `UnrealEditor.exe`)** 가 들어 있다. 빌드 도구는 엔진 경로가 환경마다 다르므로 **경로 무관 와일드카드**(`PowerShell(& "*Build.bat"*)` 등)로 등록돼 있어, `& "<엔진경로>\...\Build.bat" ...` 형태의 호출에 매칭된다.

- 위 패턴이 본인 호출 형태와 달라 여전히 프롬프트가 뜨면, **프롬프트의 "항상 허용"을 선택**하면 된다 — Claude Code가 정확한 규칙을 `settings.local.json`에 자동 추가한다(개인 설정이라 공유 안 됨).
- 쿠킹/패키징처럼 무겁거나 위험한 명령까지 자동 허용하고 싶지 않으면, 공유 `settings.json`에서 해당 패턴을 빼고 개인 `settings.local.json`에만 두는 것도 방법이다.
