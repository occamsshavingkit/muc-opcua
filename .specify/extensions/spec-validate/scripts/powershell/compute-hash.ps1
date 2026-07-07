# compute-hash.ps1 - Compute SHA-256 hash of a file
# Usage: compute-hash.ps1 <file-path>
# Output: sha256:<hex-hash> to stdout

param(
    [Parameter(Mandatory=$false, Position=0)]
    [string]$FilePath
)

if ([string]::IsNullOrWhiteSpace($FilePath)) {
    Write-Error "Error: No file path provided. Usage: compute-hash.ps1 <file-path>"
    exit 1
}

if (-not (Test-Path -Path $FilePath -PathType Leaf)) {
    Write-Error "Error: File not found or does not exist: $FilePath"
    exit 1
}

try {
    $hash = (Get-FileHash -Path $FilePath -Algorithm SHA256).Hash.ToLower()
    Write-Output "sha256:$hash"
} catch {
    Write-Error "Error: Failed to compute hash for $FilePath - $_"
    exit 1
}
