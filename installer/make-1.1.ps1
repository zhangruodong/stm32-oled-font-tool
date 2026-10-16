# 创建 v1.1 Release 并上传两个附件
$ErrorActionPreference = 'Stop'
$repo  = 'zhangruodong/stm32-oled-font-tool'
$tag   = 'v1.1'
$title = 'v1.1'
$R     = 'G:\stm32Project\stm32-oled-font-tool'
$assets = @("$R\oledfonttool-1.1-portable.zip", "$R\installer\Output\oledfonttool-1.1-setup.exe")
$notesFile = "$R\installer\release-notes-1.1.md"

# ---- token ----
$credFile = Join-Path $env:TEMP 'gcred.txt'
"protocol=https`nhost=github.com`n" | Set-Content $credFile -Encoding ascii -NoNewline
$credOut = & cmd.exe /c "`"D:\git 2.55\Git\cmd\git.exe`" credential fill < `"$credFile`" 2>&1"
Remove-Item $credFile -Force -ErrorAction SilentlyContinue
$token = $null
foreach ($line in $credOut) { if ($line -match '^password=(.+)$') { $token = $Matches[1] } }
if (-not $token) { throw '没能取到 token' }

$headers = @{
    Authorization          = "Bearer $token"
    Accept                 = 'application/vnd.github+json'
    'X-GitHub-Api-Version' = '2022-11-28'
    'User-Agent'           = 'release-helper'
}
function Get-ApiError($err) {
    $resp = $err.Exception.Response
    if ($resp) { try { return (New-Object System.IO.StreamReader($resp.GetResponseStream())).ReadToEnd() } catch { } }
    return $err.Exception.Message
}

$me = Invoke-RestMethod -Uri 'https://api.github.com/user' -Headers $headers -Method Get
Write-Host "认证: $($me.login)"

# ---- 已存在就复用 ----
$all = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases?per_page=100" -Headers $headers -Method Get
$release = $all | Where-Object { $_.tag_name -eq $tag } | Select-Object -First 1

if ($release) {
    Write-Host "已存在 $tag，复用它"
} else {
    # 必须转成 string，否则 ConvertTo-Json 会把对象属性也序列化进去
    $notes = [string][System.IO.File]::ReadAllText($notesFile, [System.Text.Encoding]::UTF8)
    Write-Host "说明长度: $($notes.Length) 字符"
    $body = @{
        tag_name         = $tag
        target_commitish = 'main'
        name             = $title
        body             = $notes
        draft            = $false
        prerelease       = $false
    } | ConvertTo-Json -Depth 5
    try {
        $release = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases" `
                                     -Headers $headers -Method Post `
                                     -Body ([System.Text.Encoding]::UTF8.GetBytes($body)) `
                                     -ContentType 'application/json; charset=utf-8'
        Write-Host "Release 已创建"
    } catch {
        Write-Host "创建失败: $(Get-ApiError $_)"; throw
    }
}

# ---- 上传附件 ----
$existing = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/$($release.id)/assets" `
                             -Headers $headers -Method Get
foreach ($path in $assets) {
    $name = [System.IO.Path]::GetFileName($path)
    if ($existing | Where-Object { $_.name -eq $name }) {
        Write-Host "  $name 已存在，跳过"
        continue
    }
    $bytes = [System.IO.File]::ReadAllBytes($path)
    Write-Host ("  上传 {0} ({1:N1} MB) ..." -f $name, ($bytes.Length / 1MB))
    $url = "https://uploads.github.com/repos/$repo/releases/$($release.id)/assets?name=$name"
    try {
        $a = Invoke-RestMethod -Uri $url -Headers $headers -Method Post `
                               -Body $bytes -ContentType 'application/octet-stream'
        Write-Host ("    OK {0:N1} MB" -f ($a.size / 1MB))
    } catch {
        Write-Host "    失败: $(Get-ApiError $_)"; throw
    }
}

# ---- 最终状态 ----
$final = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/$($release.id)" -Headers $headers -Method Get
Write-Host ''
Write-Host "v1.1 附件:"
foreach ($a in $final.assets) { Write-Host ("  {0,-38} {1,7:N1} MB  {2}" -f $a.name, ($a.size/1MB), $a.state) }
Write-Host ''
Write-Host $final.html_url
