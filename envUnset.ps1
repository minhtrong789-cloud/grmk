$repoDir = $PSScriptRoot

$paths = $env:PATH -split ';'
if ($repoDir -in $paths) {
    $newPaths = $paths | Where-Object { $_ -ne $repoDir -and $_ -ne "" }
    $env:PATH = $newPaths -join ';'
    Write-Host " [OK] Removed from PATH: $repoDir" -ForegroundColor Green
}
else {
    Write-Host " [INFO] '$repoDir' is not in your PATH." -ForegroundColor Yellow
}
