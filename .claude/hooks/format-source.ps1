# format-source.ps1
# PostToolUse 훅: Edit/Write/MultiEdit로 수정된 C++ 소스에 clang-format을 적용한다.
# clang-format을 찾지 못하면 조용히 통과한다(비차단). 절대 빌드를 막지 않는다.
#
# 입력: stdin으로 들어오는 Claude Code 훅 JSON (tool_input.file_path 사용)
# 종료코드: 항상 0 (포매팅은 선택적 보정이지 차단 사유가 아님)

$ErrorActionPreference = 'SilentlyContinue'

try {
    $raw = [Console]::In.ReadToEnd()
    if (-not $raw) { exit 0 }
    $payload = $raw | ConvertFrom-Json

    $path = $payload.tool_input.file_path
    if (-not $path) { exit 0 }

    # C/C++ 소스만 대상
    if ($path -notmatch '\.(cpp|h|hpp|inl|cc|cxx)$') { exit 0 }

    # 생성/캐시 산출물은 건드리지 않음
    if ($path -match '(\.generated\.h$|[\\/](Intermediate|Binaries|DerivedDataCache|Saved)[\\/])') { exit 0 }
    if (-not (Test-Path $path)) { exit 0 }

    # clang-format 탐색: PATH → Visual Studio 번들 LLVM → 환경변수 지정 경로
    $clang = (Get-Command clang-format -ErrorAction SilentlyContinue).Source
    if (-not $clang) {
        $candidates = @(
            "$env:CLANG_FORMAT_PATH",
            "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe",
            "${env:ProgramFiles}\LLVM\bin\clang-format.exe"
        )
        foreach ($c in $candidates) {
            if ($c -and (Test-Path $c)) { $clang = $c; break }
        }
    }
    if (-not $clang) { exit 0 }  # 포매터 없음 → 통과

    & $clang -i -- $path | Out-Null
} catch {
    # 어떤 오류도 작업을 막지 않는다
}
exit 0
