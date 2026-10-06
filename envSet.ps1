$repoDir = $PSScriptRoot

$paths = $env:PATH -split ';'
if ($repoDir -notin $paths) {
    $env:PATH = "$repoDir;$env:PATH"
    Write-Host " [OK] Added to PATH: $repoDir" -ForegroundColor Green
    Write-Host " You can now run 'grmk' from anywhere in this terminal session." -ForegroundColor Cyan
}
else {
    Write-Host " [INFO] 'grmk' is already in your PATH." -ForegroundColor Yellow
}
