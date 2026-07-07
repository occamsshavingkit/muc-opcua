# check-gate.ps1 - Evaluate implementation gate for a feature
# Usage: check-gate.ps1 <feature-name>
# Output: Gate result JSON per Contract 3

param(
    [Parameter(Mandatory=$false, Position=0)]
    [string]$Feature
)

if ([string]::IsNullOrWhiteSpace($Feature)) {
    Write-Error "Error: No feature name provided. Usage: check-gate.ps1 <feature-name>"
    exit 1
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Read approval state
try {
    $stateJson = & "$ScriptDir/read-approval-state.ps1" $Feature
    $state = $stateJson | ConvertFrom-Json
} catch {
    $result = @{
        outcome = "blocked"
        reason = "Failed to read approval state for feature: $Feature"
        actions_required = @("Run /speckit.spec-validate.validate to validate spec.md")
        warnings = @()
    }
    $result | ConvertTo-Json -Depth 3
    exit 1
}

function Output-Gate {
    param($Outcome, $Reason, $Actions, $Warnings)
    $result = @{
        outcome = $Outcome
        reason = $Reason
        actions_required = if ($Actions) { $Actions } else { @() }
        warnings = if ($Warnings) { $Warnings } else { @() }
    }
    $result | ConvertTo-Json -Depth 3
    if ($Outcome -eq "blocked") { exit 1 } else { exit 0 }
}

# Rule 1: No validation
if ($state.spec_self_validation -eq "not-run") {
    Output-Gate "blocked" "Spec has not been validated" @("Run /speckit.spec-validate.validate")
}

# Rule 2: Validation failed
if ($state.spec_self_validation -eq "failed") {
    Output-Gate "blocked" "Spec validation failed" @("Re-run /speckit.spec-validate.validate")
}

# Rule 3: Check staleness
$specIsStale = $false
$specDir = if ($env:SPEC_VALIDATE_SPEC_DIR) { $env:SPEC_VALIDATE_SPEC_DIR } else { "" }
if ($specDir -and (Test-Path "$specDir/spec.md") -and $state.spec_hash) {
    $currentHash = & "$ScriptDir/compute-hash.ps1" "$specDir/spec.md"
    if ($currentHash -ne $state.spec_hash) { $specIsStale = $true }
}

# Rule 4: Changes requested
if ($state.review_status -eq "changes-requested") {
    Output-Gate "blocked" "Reviewer requested changes" @("Address reviewer comments", "Re-run /speckit.spec-validate.validate")
}

# Rule 5: Stale + review approved
if ($specIsStale -and $state.review_status -eq "approved") {
    Output-Gate "allowed" "Self-validation is stale but peer review is approved" @() @("Self-validation is stale")
}

# Rule 6: Stale without approved review
if ($specIsStale) {
    Output-Gate "blocked" "Spec content has changed since validation" @("Re-run /speckit.spec-validate.validate")
}

# Rule 7: Review pending
if ($state.review_status -eq "pending") {
    Output-Gate "blocked" "Peer review is pending" @("Wait for reviewer", "Or use maintainer override")
}

# Rule 8: Timed out
if ($state.review_status -eq "timed-out") {
    if ($state.timeout_self_approval.used) {
        Output-Gate "allowed-with-warning" "Review timed out - author self-approved" @() @("Review timed out after 24h SLA")
    } else {
        Output-Gate "blocked" "Review timed out but no self-approval reason recorded" @("Record a self-approval reason")
    }
}

# Rule 9: Override
if ($state.override.used) {
    Output-Gate "allowed" "Maintainer override approved" @() @("Gate was overridden by maintainer")
}

# Rule 10: Validated + review met
if ($state.spec_self_validation -eq "passed" -and ($state.review_status -eq "not-required" -or $state.review_status -eq "approved")) {
    Output-Gate "allowed" "Spec validated and review requirements met" @()
}

# Fallback
Output-Gate "blocked" "Gate check did not match any allow condition" @("Run /speckit.spec-validate.status")
