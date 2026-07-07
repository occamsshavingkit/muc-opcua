# write-approval-state.ps1 - Write approval state for a feature
# Usage: write-approval-state.ps1 <feature-name> <json-string>

param(
    [Parameter(Mandatory=$false, Position=0)]
    [string]$Feature,
    [Parameter(Mandatory=$false, Position=1)]
    [string]$JsonData
)

if ([string]::IsNullOrWhiteSpace($Feature)) {
    Write-Error "Error: No feature name provided. Usage: write-approval-state.ps1 <feature-name> <json-string>"
    exit 1
}

if ([string]::IsNullOrWhiteSpace($JsonData)) {
    Write-Error "Error: No JSON data provided. Usage: write-approval-state.ps1 <feature-name> <json-string>"
    exit 1
}

# Validate JSON and required fields
try {
    $parsed = $JsonData | ConvertFrom-Json
    if (-not $parsed.spec_self_validation -or -not $parsed.approval_status) {
        throw "Missing required fields"
    }
} catch {
    Write-Error "Error: Invalid JSON or missing required fields (spec_self_validation, approval_status)"
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

if (-not (Test-Path $StateDir)) {
    New-Item -ItemType Directory -Path $StateDir -Force | Out-Null
}

$StateFile = Join-Path $StateDir "$Feature.json"
$parsed | ConvertTo-Json -Depth 5 | Set-Content -Path $StateFile -Encoding UTF8
Write-Host "State written to $StateFile" -ForegroundColor Green
