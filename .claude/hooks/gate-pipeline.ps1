# gate-pipeline.ps1
# PreToolUse 훅 (matcher: Task): ue-feature 파이프라인 서브에이전트가 실행되기 직전에
# 사용자 승인을 요구한다. 승인해야만 해당 에이전트(= 다음 단계)로 넘어간다.
#
# 동작: tool_input.subagent_type 이 파이프라인 에이전트면 permissionDecision=ask 를 반환.
#       그 외 서브에이전트(Explore 등)는 간섭하지 않고 통과.
#
# 입력: stdin Claude Code 훅 JSON
# 종료코드: 0 (결정은 stdout JSON으로 전달)

$ErrorActionPreference = 'SilentlyContinue'

try {
    $raw = [Console]::In.ReadToEnd()
    if (-not $raw) { exit 0 }
    $payload = $raw | ConvertFrom-Json

    $subagent = $payload.tool_input.subagent_type
    if (-not $subagent) { exit 0 }

    $pipeline = @('ue-architect', 'ue-implementer', 'ue-builder', 'ue-reviewer', 'ue-doc-writer')

    if ($pipeline -contains $subagent) {
        $decision = @{
            hookSpecificOutput = @{
                hookEventName            = 'PreToolUse'
                permissionDecision       = 'ask'
                permissionDecisionReason = "파이프라인 단계 '$subagent' 실행 승인이 필요합니다. 승인해야 다음 에이전트로 넘어갑니다."
            }
        }
        $decision | ConvertTo-Json -Compress -Depth 5 | Write-Output
    }
} catch {
    # 훅 오류로 작업을 막지 않는다(통과)
    exit 0
}
exit 0
