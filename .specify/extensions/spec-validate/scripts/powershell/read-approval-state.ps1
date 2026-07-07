# read-approval-state.ps1 - Read approval state for a feature
# Usage: read-approval-state.ps1 <feature-name>

param(
    [Parameter(Mandatory=$false, Position=0)]
    [string]$Feature
)

if ([string]::IsNullOrWhiteSpace($Feature)) {
    Write-Error "Error: No feature name provided. Usage: read-approval-state.ps1 <feature-name>"
    exit 1
}

$StateDir = if ($env:SPEC_VALIDATE_STATE_DIR) {
    $env:SPEC_VALIDATE_STATE_DIR
} else {
    $dir = Get-Location
    while ($dir.Path -ne [System.IO.Path]::GetPathRoot($dir.Path)) {
        if (Test-Path (Join-Path $dir.Path ".specify")) { break }
        $dir = Split-Path $dir.Path -Parent | Get-Item
    }
    Join-Path $dir.Path ".specify/extensions/spec-validate/status"
}

$StateFile = Join-Path $StateDir "$Feature.json"

if (-not (Test-Path $StateFile)) {
    $default = @{
        feature = $Feature
        spec_hash = $null
        tasks_hash = $null
        spec_self_validation = "not-run"
        tasks_self_validation = "not-run"
        spec_validated_at = $null
        tasks_validated_at = $null
        spec_critical_count = 0
        spec_missed_items = @()
        review_status = "not-required"
        review_requested_at = $null
        reviewer = $null
        review_comments = @()
        approval_status = "blocked"
        warnings = @()
        override = @{used=$false; reason=$null; by=$null}
        timeout_self_approval = @{used=$false; reason=$null}
    }
    $default | ConvertTo-Json -Depth 3
    exit 0
}

try {
    $content = Get-Content $StateFile -Raw
    $null = $content | ConvertFrom-Json
    Write-Output $content
} catch {
    Write-Error "Error: Malformed JSON in state file: $StateFile"
    exit 1
}
