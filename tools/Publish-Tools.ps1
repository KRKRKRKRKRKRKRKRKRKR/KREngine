# ツールの exe だけを、dist\ に取り出す(エクスプローラーから、ダブルクリックで動かせる形)
#   使い方:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\Publish-Tools.ps1 [-Build] [-Samples] [-Config Release]
#   -Build   : 先に、ビルドする(ツール)。つけなければ、今ある build の出力を、コピーするだけ
#   -Samples : サンプル(KRAnimated・KRBehavior・KRMinimal)も、取り出す
#   -Config  : Release(既定。最適化つき・コンソールなし)か Debug
# 出力:dist\<名前>\(exe・DLL・Models・Shaders・Enemies。pdb・lib・ログ・設定は、含めない)。dist\ は、Git に入れない
# 取り出した exe は、build を消しても動く。コードを変えたら、-Build で、取り出し直す
param(
    [string]$Config = "Release",
    [switch]$Build,
    [switch]$Samples
)

$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root "dist"

# 取り出す物:名前 = build の中の、exe のあるフォルダー(Config の手前まで)
$targets = [ordered]@{
    "KRAnimationEditor" = "tools\AnimationEditor"
    "KRBehaviorEditor"  = "tools\BehaviorEditor"
}
if ($Samples) {
    $targets["KRAnimated"] = "samples\Animated"
    $targets["KRBehavior"] = "samples\Behavior"
    $targets["KRMinimal"]  = "samples\Minimal"
}

if ($Build) {
    $cmakeDir = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue) -and (Test-Path $cmakeDir)) { $env:PATH += ";$cmakeDir" }
    & cmake --build (Join-Path $root "build") --config $Config --target @($targets.Keys)
    if ($LASTEXITCODE -ne 0) { Write-Error "ビルドに失敗しました"; exit 1 }
}

foreach ($name in $targets.Keys) {
    $from = Join-Path $root ("build\" + $targets[$name] + "\" + $Config)
    if (-not (Test-Path (Join-Path $from "$name.exe"))) {
        Write-Warning "$name.exe が見つかりません($from)。-Build を、つけてください"
        continue
    }
    $to = Join-Path $dist $name
    if (Test-Path $to) { Remove-Item $to -Recurse -Force }
    New-Item -ItemType Directory -Force $to | Out-Null
    Copy-Item (Join-Path $from "$name.exe") $to
    Copy-Item (Join-Path $from "*.dll") $to -ErrorAction SilentlyContinue
    foreach ($folder in "Models", "Shaders", "Enemies") {
        if (Test-Path (Join-Path $from $folder)) { Copy-Item (Join-Path $from $folder) $to -Recurse }
    }
    $size = (Get-ChildItem $to -Recurse -File | Measure-Object Length -Sum).Sum
    "{0}: {1}  ({2:N1} MB)" -f $name, $to, ($size / 1MB)
}
