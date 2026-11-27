# Runs the parkour motion regression test in a headless editor and prints the result.
# Exit code: 0 = all cases pass, 1 = a case failed, 2 = could not run (editor open, timeout, no summary).
param([int]$TimeoutSeconds = 600)

$Root = Resolve-Path "$PSScriptRoot\..\.."
$Project = Join-Path $Root "ActionRoguelike.uproject"
$Script = Join-Path $PSScriptRoot "parkour_motion_regression.py"
$Log = Join-Path $Root "Saved\Logs\MotionRegression.log"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Write-Output "could-not-run: close the Unreal Editor first"
    exit 2
}
Remove-Item $Log -ErrorAction SilentlyContinue
$proc = Start-Process -FilePath $Editor -ArgumentList "`"$Project`"", "/Game/Maps/L_Parkour", "-RenderOffScreen", "-unattended", "-nosplash",
    "-ExecCmds=`"py $Script`"", "-ABSLOG=`"$Log`"" -PassThru -WindowStyle Hidden
if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
    $proc.Kill()
    Write-Output "could-not-run: timed out after $TimeoutSeconds s (log: $Log)"
    exit 2
}
$lines = Select-String -Path $Log -Pattern "\[MREG\] (CASE|SUMMARY)" | ForEach-Object { $_.Line -replace '^.*\[MREG\] ', '' }
$lines | ForEach-Object { Write-Output $_ }
$summary = $lines | Where-Object { $_ -like "SUMMARY*" } | Select-Object -Last 1
if (-not $summary -or $summary -like "*could-not-run*") { exit 2 }
if ($summary -match "fail 0 ") { exit 0 }
exit 1
