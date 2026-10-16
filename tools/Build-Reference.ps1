# Build-Reference.ps1 - builds the reference web site (ONE html file) from docs\Reference\*.md.
#   Normal (file you can open by double-click):  powershell -NoProfile -ExecutionPolicy Bypass -File tools\Build-Reference.ps1
#       -> docs\Reference\reference.html   (a full HTML document; not committed: see .gitignore)
#   Fragment (for publishing as a claude.ai Artifact: no <html>/<head>):  ... -Fragment -Out <path>
#   Zip (to hand out; works WITHOUT internet):  ... -Zip
#       -> dist\KREngine-Reference-v<version>.zip  (reference.html + README.txt + THIRD_PARTY_LICENSES.txt; dist\ is not committed)
#       The 3 libraries the page loads from a CDN (marked, highlight.js, Mermaid) are embedded into reference.html.
#       They are downloaded once into build\reference-libs\ and checked with SHA256 (pinned versions; a changed file stops the build).
#       Web fonts are not embedded (Japanese fonts are huge): offline, the PC's own fonts (Yu Gothic UI, Meiryo, Consolas) are used.
# The markdown files are the source. Edit them, run this again.
# (ASCII only on purpose: Windows PowerShell 5.1 reads BOM-less scripts as ANSI. Japanese text lives in the .md / _template.html / _zip files.)
param(
    [string]$EngineRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Out = '',
    [switch]$Fragment,
    [switch]$Zip
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

if ($Fragment -and -not $Zip) {
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
if (-not $Zip) {
    [IO.File]::WriteAllText($Out, $result, $utf8)
    Write-Output ('wrote ' + $Out + '  (' + $files.Count + ' pages, engine ' + $version + ', ' + [math]::Round($result.Length / 1KB) + ' KB)')
    return
}

# ---------- -Zip: embed the libraries, then pack reference.html + README.txt + THIRD_PARTY_LICENSES.txt ----------
# pinned versions = the same URLs as _template.html. If you change a version there, update the URL and the SHA256 here
$libs = @(
    @{ File = 'marked.min.js';    Url = 'https://cdnjs.cloudflare.com/ajax/libs/marked/4.3.0/marked.min.js';         Sha = 'c68075672d976e4777390560baa112194855bd4404b13647da4855aae1f9360c' },
    @{ File = 'highlight.min.js'; Url = 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0/highlight.min.js'; Sha = '837a6fa5b0c736b52bbde2b2b6190f305da3fc9ed41681db5321507057b5c846' },
    @{ File = 'mermaid.min.js';   Url = 'https://cdnjs.cloudflare.com/ajax/libs/mermaid/10.9.1/mermaid.min.js';      Sha = '61b335a46df05a7ce1c98378f60e5f3e77a7fb608a1056997e8a649304a936d6' }
)
$cache = Join-Path $EngineRoot 'build\reference-libs'
New-Item -ItemType Directory -Force -Path $cache | Out-Null
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
foreach ($lib in $libs) {
    $path = Join-Path $cache $lib.File
    $ok = (Test-Path -LiteralPath $path) -and ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -eq $lib.Sha.ToUpper())
    if (-not $ok) {
        Write-Output ('downloading ' + $lib.Url)
        $tmp = $path + '.download'
        Invoke-WebRequest -Uri $lib.Url -OutFile $tmp -UseBasicParsing
        $hash = (Get-FileHash -LiteralPath $tmp -Algorithm SHA256).Hash
        if ($hash -ne $lib.Sha.ToUpper()) { Remove-Item -LiteralPath $tmp -Force; throw ('SHA256 mismatch for ' + $lib.Url + ' (got ' + $hash + '): the file is not the pinned one, stopped') }
        Move-Item -LiteralPath $tmp -Destination $path -Force
    }
    $tag = '<script src="' + $lib.Url + '"></script>'
    if (-not $result.Contains($tag)) { throw ('_template.html no longer loads ' + $lib.Url + ': update the list in Build-Reference.ps1') }
    # "</script" inside the library would end the <script> element early: "<\/script" means the same in JavaScript
    $code = [IO.File]::ReadAllText($path, $utf8).Replace('</script', '<\/script')
    $result = $result.Replace($tag, '<script>/* ' + $lib.File + ' (embedded; license: THIRD_PARTY_LICENSES.txt) */' + "`n" + $code + "`n" + '</script>')
}
if ($result -match '<script[^>]+src="https?:') { throw 'a <script src="http..."> is still left: the page would need internet' }

$name = 'KREngine-Reference-v' + $version
$dist = Join-Path $EngineRoot 'dist'
$stage = Join-Path $dist $name
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }   # our own output folder from the last run
New-Item -ItemType Directory -Force -Path $stage | Out-Null
[IO.File]::WriteAllText((Join-Path $stage 'reference.html'), $result, $utf8)
Copy-Item -LiteralPath (Join-Path $refDir '_zip\README.txt') -Destination $stage
Copy-Item -LiteralPath (Join-Path $refDir '_zip\THIRD_PARTY_LICENSES.txt') -Destination $stage
$zipPath = Join-Path $dist ($name + '.zip')
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -Force
Write-Output ('wrote ' + $zipPath + '  (' + $files.Count + ' pages, engine ' + $version + ', html ' + [math]::Round($result.Length / 1KB) + ' KB, zip ' + [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1KB) + ' KB; works offline)')
