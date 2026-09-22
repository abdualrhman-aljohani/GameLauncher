<#
.SYNOPSIS
    ينزّل أصول واجهة لوحة التحكم مرة واحدة ويحفظها محلياً ليعمل البرنامج بدون إنترنت.

.DESCRIPTION
    الواجهة (panel_ui.html) تحمّل حالياً من الإنترنت: Tailwind (Play CDN)، Font Awesome، خطوط Google،
    ApexCharts. هذا السكربت يجهّز نسخاً محلية في مجلد ui_assets، والبرنامج يستخدمها تلقائياً إذا وُجد
    المجلد بجانب GameLauncher.exe، وإلا يرجع للـ CDN كما كان (لا شيء ينكسر لو ما شغّلت السكربت).

    الملفات الناتجة:
      ui_tailwind.js      نفس سكربت Tailwind Play CDN بالضبط (يترجم الـ classes وقت التشغيل كما الآن،
                          فلا خطر أن يختلف الشكل بسبب classes ديناميكية)
      ui_fa.css           Font Awesome مع الخطوط مدمجة داخل الملف (woff2 كـ data URI)
      ui_fonts.css        خطوط Cairo وTajawal وJetBrains Mono (العربي + اللاتيني فقط) مدمجة
      ui_apexcharts.js    ApexCharts (الإصدار الرئيسي المحدد أدناه)
      VERSIONS.txt        مصدر كل ملف وتاريخ التنزيل وبصمة SHA256

    المتطلبات: Windows 10/11 + PowerShell 5.1 (الموجود افتراضياً) + اتصال إنترنت وقت التشغيل فقط.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\build-ui-assets.ps1
    powershell -ExecutionPolicy Bypass -File tools\build-ui-assets.ps1 -OutDir "D:\GameLauncher\ui_assets"
#>
param(
    # المجلد الذي يبحث فيه البرنامج: ui_assets بجانب GameLauncher.exe
    [string]$OutDir = (Join-Path $PSScriptRoot '..\bin\Release\ui_assets'),
    [string[]]$ExtraOutDirs = @((Join-Path $PSScriptRoot '..\bin\Debug\ui_assets')),
    [string]$FontAwesomeVersion = '6.4.0',
    [string]$ApexChartsMajor = '6'
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }

$script:UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36'
$script:Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$script:Log = New-Object System.Collections.Generic.List[string]

function Download([string]$Url) {
    $wc = New-Object System.Net.WebClient
    $wc.Headers.Add('User-Agent', $script:UA)
    try { return ,$wc.DownloadData($Url) }
    finally { $wc.Dispose() }
}
function DownloadText([string]$Url) {
    return $script:Utf8NoBom.GetString((Download $Url))
}
function Sha256([byte[]]$Bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '') }
    finally { $sha.Dispose() }
}
function Save([string]$Name, [byte[]]$Bytes, [string]$Source) {
    $path = Join-Path $script:StageDir $Name
    [IO.File]::WriteAllBytes($path, $Bytes)
    $kb = [math]::Round($Bytes.Length / 1KB)
    Write-Host ("  {0,-18} {1,7} KB" -f $Name, $kb)
    $script:Log.Add(("{0}`n  source : {1}`n  size   : {2} bytes`n  sha256 : {3}`n" -f $Name, $Source, $Bytes.Length, (Sha256 $Bytes)))
}
function Require([bool]$Cond, [string]$Message) {
    if (-not $Cond) { throw $Message }
}

# نبني أولاً في مجلد مؤقت، ولا نستبدل المجلد الحقيقي إلا بعد نجاح كل الخطوات
$script:StageDir = Join-Path ([IO.Path]::GetTempPath()) ('gl_ui_assets_' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $script:StageDir | Out-Null

try {
    Write-Host 'GameLauncher UI assets' -ForegroundColor Cyan

    # 1) Tailwind Play CDN (نفس السكربت الذي تحمّله الواجهة الآن)
    Write-Host '[1/4] Tailwind'
    $twUrl = 'https://cdn.tailwindcss.com'
    $tw = Download $twUrl
    Require ($tw.Length -gt 100000) 'Tailwind: الملف أصغر من المتوقع'
    Require ($script:Utf8NoBom.GetString($tw).Contains('tailwind')) 'Tailwind: المحتوى غير متوقع'
    Save 'ui_tailwind.js' $tw $twUrl

    # 2) Font Awesome (CSS + خطوط woff2 مدمجة)
    Write-Host '[2/4] Font Awesome'
    $faBase = "https://cdnjs.cloudflare.com/ajax/libs/font-awesome/$FontAwesomeVersion"
    $script:FaBase = $faBase
    $fa = DownloadText "$faBase/css/all.min.css"
    # نبقي woff2 فقط (كل متصفحات WebView2 تدعمه) ونحذف بدائل ttf لتقليل الحجم
    $fa = [regex]::Replace($fa, ',url\(\.\./webfonts/[^)]+\.ttf\)\s*format\("truetype"\)', '')
    $faEval = [System.Text.RegularExpressions.MatchEvaluator]{
        param($m)
        $b = Download ("$($script:FaBase)/webfonts/" + $m.Groups[1].Value)
        return 'url(data:font/woff2;base64,' + [Convert]::ToBase64String($b) + ')'
    }
    $fa = [regex]::Replace($fa, 'url\(\.\./webfonts/([^)]+\.woff2)\)', $faEval)
    Require (-not $fa.Contains('../webfonts/')) 'Font Awesome: بقيت روابط خطوط لم تُدمج'
    Save 'ui_fa.css' ($script:Utf8NoBom.GetBytes($fa)) "$faBase/css/all.min.css"

    # 3) خطوط Google (نطلب CSS بـ User-Agent حديث ليعطينا woff2 مقسّماً حسب unicode-range)
    Write-Host '[3/4] Google Fonts (arabic + latin)'
    $gfUrl = 'https://fonts.googleapis.com/css2?family=Cairo:wght@300;400;600;700;900&family=Tajawal:wght@400;500;700&family=JetBrains+Mono:wght@400;600&display=swap'
    $gf = DownloadText $gfUrl
    $keep = @('arabic', 'latin')
    $fontCache = @{}
    $blocks = New-Object System.Collections.Generic.List[string]
    $pattern = '(?s)/\*\s*([a-z\-]+)\s*\*/\s*(@font-face\s*\{.*?\})'
    foreach ($m in [regex]::Matches($gf, $pattern)) {
        if ($keep -notcontains $m.Groups[1].Value) { continue }
        $block = $m.Groups[2].Value
        $u = [regex]::Match($block, 'url\((https://[^)]+\.woff2)\)')
        Require $u.Success 'Google Fonts: تعذّر إيجاد رابط الخط'
        $fontUrl = $u.Groups[1].Value
        if (-not $fontCache.ContainsKey($fontUrl)) {
            $fontCache[$fontUrl] = [Convert]::ToBase64String((Download $fontUrl))
        }
        $block = $block.Replace($fontUrl, 'data:font/woff2;base64,' + $fontCache[$fontUrl])
        $blocks.Add($block)
    }
    Require ($blocks.Count -ge 3) 'Google Fonts: عدد الكتل أقل من المتوقع'
    Save 'ui_fonts.css' ($script:Utf8NoBom.GetBytes(($blocks -join "`n"))) $gfUrl

    # 4) ApexCharts (نفس الإصدار الرئيسي المثبّت في الواجهة)
    Write-Host '[4/4] ApexCharts'
    $axUrl = "https://cdn.jsdelivr.net/npm/apexcharts@$ApexChartsMajor"
    $ax = Download $axUrl
    Require ($ax.Length -gt 100000) 'ApexCharts: الملف أصغر من المتوقع'
    Require ($script:Utf8NoBom.GetString($ax).Contains('ApexCharts')) 'ApexCharts: المحتوى غير متوقع'
    Save 'ui_apexcharts.js' $ax $axUrl

    $header = "GameLauncher UI assets — downloaded $([DateTime]::UtcNow.ToString('yyyy-MM-dd HH:mm')) UTC`n`n"
    [IO.File]::WriteAllText((Join-Path $script:StageDir 'VERSIONS.txt'), $header + ($script:Log -join "`n"), $script:Utf8NoBom)

    # نجحت كل الخطوات: انسخ للوجهات
    foreach ($dest in (@($OutDir) + $ExtraOutDirs)) {
        $full = [IO.Path]::GetFullPath($dest)
        $parent = Split-Path $full -Parent
        # الوجهات الإضافية (مثل Debug) نتجاهلها لو مجلد البناء غير موجود
        if ($dest -ne $OutDir -and -not (Test-Path $parent)) { continue }
        New-Item -ItemType Directory -Path $full -Force | Out-Null
        Copy-Item -Path (Join-Path $script:StageDir '*') -Destination $full -Force
        Write-Host "  -> $full" -ForegroundColor Green
    }
    Write-Host 'تم. أعد تشغيل GameLauncher لتستخدم الواجهة النسخ المحلية.' -ForegroundColor Green
}
finally {
    Remove-Item -Path $script:StageDir -Recurse -Force -ErrorAction SilentlyContinue
}
