# guard-generated.ps1
# PreToolUse 훅: Edit/Write/MultiEdit가 생성/캐시/바이너리 산출물을 건드리려 하면 차단한다.
# (CLAUDE.md 7절 "절대 건드리지 말 것" 강제)
#
# 입력: stdin으로 들어오는 Claude Code 훅 JSON (tool_input.file_path 사용)
# 종료코드: 0 = 허용, 2 = 차단(차단 사유를 stderr로 출력하면 Claude에게 전달됨)

$ErrorActionPreference = 'SilentlyContinue'

try {
    $raw = [Console]::In.ReadToEnd()
    if (-not $raw) { exit 0 }
    $payload = $raw | ConvertFrom-Json

    $path = $payload.tool_input.file_path
    if (-not $path) { exit 0 }

    # 차단 대상 패턴
    $blocked = @(
        '\.generated\.h$',
        '\.gen\.cpp$',
        '[\\/]Intermediate[\\/]',
        '[\\/]Binaries[\\/]',
        '[\\/]DerivedDataCache[\\/]',
        '[\\/]Saved[\\/]',
        '\.(uasset|umap)$'
    )

    foreach ($p in $blocked) {
        if ($path -match $p) {
            [Console]::Error.WriteLine("차단됨: '$path' 는 엔진이 생성하거나 관리하는 산출물입니다(CLAUDE.md 7절). 직접 편집하지 마세요. 소스 헤더/.cpp 또는 에디터에서 작업하세요.")
            exit 2
        }
    }
} catch {
    # 훅 자체 오류로 정상 작업을 막지 않는다
    exit 0
}
exit 0
