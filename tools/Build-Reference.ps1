# Build-Reference.ps1 - builds the reference web site (ONE html file) from docs\Reference\*.md.
#   Normal (file you can open by double-click):  powershell -NoProfile -ExecutionPolicy Bypass -File tools\Build-Reference.ps1
#       -> docs\Reference\reference.html   (a full HTML document; not committed: see .gitignore)
#   Fragment (for publishing as a claude.ai Artifact: no <html>/<head>):  ... -Fragment -Out <path>
# The markdown files are the source. Edit them, run this again.
# (ASCII only on purpose: Windows PowerShell 5.1 reads BOM-less scripts as ANSI. Japanese text lives in the .md / _template.html files.)
param(
    [string]$EngineRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Out = '',
    [switch]$Fragment
)

Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'
$utf8 = New-Object Text.UTF8Encoding $false

$refDir = Join-Path $EngineRoot 'docs\Reference'
$template = [IO.File]::ReadAllText((Join-Path $refDir '_template.html'), $utf8)

# engine version: single source = CMakeLists.txt
$cmake = [IO.File]::ReadAllText((Join-Path $EngineRoot 'CMakeLists.txt'), $utf8)
$m = [regex]::Match($cmake, 'project\(KREngine VERSION (\d+\.\d+\.\d+)')
if (-not $m.Success) { throw 'engine version not found in CMakeLists.txt' }
$version = $m.Groups[1].Value

# pages: index first, then the others by name
$files = @(Get-ChildItem -LiteralPath $refDir -Filter '*.md' | Sort-Object { if ($_.BaseName -eq 'index') { '' } else { $_.BaseName } })
if ($files.Count -eq 0) { throw 'no .md files in docs\Reference' }
$sb = New-Object Text.StringBuilder
foreach ($f in $files) {
    $text = [IO.File]::ReadAllText($f.FullName, $utf8).Replace("`r`n", "`n")
    # images: ![alt](images/x.png) -> a data: URI (the generated html stays ONE file). The .md keeps the relative path (GitHub shows it as is)
    $text = [regex]::Replace($text, '!\[([^\]]*)\]\((images/[^)\s]+)\)', {
        param($mm)
        $imgPath = Join-Path $refDir ($mm.Groups[2].Value.Replace('/', '\'))
        if (-not (Test-Path -LiteralPath $imgPath)) { throw ('image not found: ' + $mm.Groups[2].Value) }
        $ext = [IO.Path]::GetExtension($imgPath).TrimStart('.').ToLower()
        $mime = if ($ext -eq 'jpg') { 'jpeg' } else { $ext }
        '![' + $mm.Groups[1].Value + '](data:image/' + $mime + ';base64,' + [Convert]::ToBase64String([IO.File]::ReadAllBytes($imgPath)) + ')'
    })
    if ($text -match '</script') { throw ('a markdown file contains "</script": ' + $f.Name) }
    [void]$sb.Append('<script type="text/markdown" data-page="' + $f.BaseName + '">' + "`n" + $text.TrimEnd() + "`n" + '</script>' + "`n")
}

$body = $template.Replace('@@PAGES@@', $sb.ToString()).Replace('@@VERSION@@', $version).Replace("`r`n", "`n")

if ($Fragment) {
    if (-not $Out) { throw '-Fragment needs -Out <path>' }
    $result = $body
}
else {
    if (-not $Out) { $Out = Join-Path $refDir 'reference.html' }
    # a full document: charset first (Japanese), the <title> moves into <head>
    $title = [regex]::Match($body, '<title>.*?</title>').Value
    $rest = $body.Replace($title, '')
    $result = '<!doctype html>' + "`n" + '<html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">' + $title + '</head><body>' + "`n" + $rest + '</body></html>' + "`n"
}
[IO.File]::WriteAllText($Out, $result, $utf8)
Write-Output ('wrote ' + $Out + '  (' + $files.Count + ' pages, engine ' + $version + ', ' + [math]::Round($result.Length / 1KB) + ' KB)')
