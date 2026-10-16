# OLED 汉字取模工具 —— 构建脚本
#
# 双击本文件即可构建（.ps1 的文件关联是好的，不像 .bat/.cmd 在你机器上被改坏了）。
# 也可以在命令行里跑： powershell -ExecutionPolicy Bypass -File build.ps1

param(
    # 构建结束后等回车再退出；双击时会自动打开，命令行调用时默认关闭
    [switch]$Wait,

    # 构建类型：Release（默认，体积小、适合分发）或 Debug（带调试信息、便于调试）
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release'
)

# ---------------------------------------------------------------------------
# BOM 自检自愈：Windows PowerShell 5.1 读取没有 BOM 的 UTF-8 文件时，会按系统
# ANSI 代码页(这里是 GBK)解释，中文注释被解成乱码后会凑出非法语法，脚本连
# 解析都过不去，表现就是"窗口一闪而过"。编辑器保存时经常把 BOM 丢掉，
# 所以这里每次启动都检查一遍。
#
# 注意：param() 必须是脚本里第一条语句，所以这段只能放在它后面。
# ---------------------------------------------------------------------------
$__me = $PSCommandPath
if ($__me -and (Test-Path -LiteralPath $__me)) {
    $__bytes = [System.IO.File]::ReadAllBytes($__me)
    $__hasBom = ($__bytes.Length -ge 3 -and $__bytes[0] -eq 0xEF -and
                 $__bytes[1] -eq 0xBB -and $__bytes[2] -eq 0xBF)
    if (-not $__hasBom) {
        Write-Host '正在给本脚本补 UTF-8 BOM（Windows PowerShell 5.1 需要）...'
        $__bom = [byte[]](0xEF, 0xBB, 0xBF)
        $__out = New-Object byte[] ($__bom.Length + $__bytes.Length)
        [System.Array]::Copy($__bom, 0, $__out, 0, $__bom.Length)
        [System.Array]::Copy($__bytes, 0, $__out, $__bom.Length, $__bytes.Length)
        [System.IO.File]::WriteAllBytes($__me, $__out)
        Write-Host '已补上，正在重新启动脚本...'
        & (Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe') `
            -NoProfile -ExecutionPolicy Bypass -File $__me
        exit $LASTEXITCODE
    }
}

$ErrorActionPreference = 'Stop'

# 目录约定：
#   本脚本在 oledfonttool\scripts\
#   源码在   oledfonttool\src\      （含 CMakeLists.txt）
#   产物在   oledfonttool\build\
$root   = Split-Path $PSScriptRoot -Parent      # ...\oledfonttool
$src    = Join-Path $root 'src'
$build  = Join-Path $root 'build'
$log    = Join-Path $root 'build-log.txt'

# ---- 路径配置：和你机器上不一致就改这里 ----
$vcvars = 'D:\Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$qt     = 'D:\Qt\6.11.2\msvc2022_64'
$cmake  = 'D:\Qt\Tools\CMake_64\bin\cmake.exe'
$ninja  = 'D:\Qt\Tools\Ninja\ninja.exe'

function Say($msg) {
    Write-Host $msg
    Add-Content -LiteralPath $log -Value $msg -Encoding UTF8
}

# 一边显示一边写日志。不能用 Tee-Object：PowerShell 5.1 下它会按 UTF-16 追加，
# 和文件开头的 UTF-8 混在一起，日志会变成 "- - C o u l d" 这种带空格的乱码。
function RunLogged([string]$exe, [string[]]$exeArgs) {
    $writer = New-Object System.IO.StreamWriter($log, $true, (New-Object System.Text.UTF8Encoding($false)))
    try {
        $writer.AutoFlush = $true
        & $exe @exeArgs 2>&1 | ForEach-Object {
            Write-Host $_
            $writer.WriteLine($_)
        }
    } finally {
        $writer.Close()
    }
}

function Finish($code) {
    if ($Wait) {
        Write-Host ''
        Read-Host '按回车关闭窗口'
    }
    exit $code
}

function Die($msg) {
    Say ''
    Say '============================================================'
    Say " 构建失败: $msg"
    Say '============================================================'
    Say ''
    Say '--- 日志最后 40 行 ---'
    if (Test-Path $log) {
        Get-Content -LiteralPath $log -Tail 40 | ForEach-Object { Write-Host $_ }
    }
    Say ''
    Say "完整日志: $log"
    Finish 1
}

# ---- 开始 ----
'============================================================' | Set-Content -LiteralPath $log -Encoding UTF8
Say ' OLED 汉字取模工具 - 构建'
Say '============================================================'
Say " 构建类型: $Config"
Say " 源码目录: $src"
Say " 构建目录: $build"
Say " 日志文件: $log"
Say ''

# ---- [1/5] 环境自检 ----
Say '[1/6] 环境自检...'
foreach ($item in @(
    @{ n = 'vcvars64.bat'; p = $vcvars },
    @{ n = 'Qt6';          p = (Join-Path $qt 'lib\cmake\Qt6') },
    @{ n = 'cmake.exe';    p = $cmake },
    @{ n = 'ninja.exe';    p = $ninja })) {
    if (-not (Test-Path $item.p)) {
        Die "找不到 $($item.n): $($item.p)`n      请修改 build.ps1 开头的路径配置。"
    }
    Say "      OK  $($item.n)"
}

# ---- [2/5] 载入 MSVC 环境 ----
Say '[2/6] 载入 MSVC 环境...'
$vcOut = & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set" 2>&1
if ($LASTEXITCODE -ne 0) {
    Die "vcvars64.bat 执行失败 (exit=$LASTEXITCODE)"
}
foreach ($line in $vcOut) {
    if ($line -match '^([^=]+)=(.*)$') {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
    }
}
$cl = Get-Command cl.exe -ErrorAction SilentlyContinue
if (-not $cl) {
    Die '载入 MSVC 环境后仍然找不到 cl.exe'
}
Say "      OK  cl.exe -> $($cl.Source)"

# ---- [3/5] CMake 配置 ----
Say '[3/6] CMake 配置...'
Add-Content -LiteralPath $log -Value ''
Add-Content -LiteralPath $log -Value '--- CMake 配置 ---'
RunLogged $cmake @('-S', $src, '-B', $build, '-G', 'Ninja',
    "-DCMAKE_MAKE_PROGRAM=$ninja",
    "-DCMAKE_PREFIX_PATH=$qt",
    "-DCMAKE_BUILD_TYPE=$Config")
if ($LASTEXITCODE -ne 0) {
    Die "CMake 配置失败 (exit=$LASTEXITCODE)"
}

# ---- [4/5] 编译 ----
Say '[4/6] 编译...'
Add-Content -LiteralPath $log -Value ''
Add-Content -LiteralPath $log -Value '--- 编译 ---'
RunLogged $cmake @('--build', $build)
if ($LASTEXITCODE -ne 0) {
    Die "编译失败 (exit=$LASTEXITCODE)"
}

# ---- [5/6] 部署 Qt 运行库 ----
Say '[5/6] 部署 Qt 运行库...'
$exe = Join-Path $build 'oledfonttool.exe'

# windeployqt 会探测 exe 是 Debug 还是 Release，自动拷对应的 DLL
# （Debug 版是 Qt6Widgetsd.dll，带 d）。
# 这里绝对不能写死 --release：Debug 构建配 Release 的 DLL 会直接报
# "找不到 Qt6Widgetsd.dll" 起不来。
#
# 部署前先把旧的 Qt DLL 清掉，避免 Debug/Release 两套混在一起。
Get-ChildItem $build -File -Filter 'Qt6*.dll' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem $build -File -Filter 'd3dcompiler_47.dll' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue

RunLogged (Join-Path $qt 'bin\windeployqt.exe') @('--no-translations', $exe)

# 检查真正的那个 DLL 在不在——少了它程序会报"找不到 Qt6Widgetsd.dll"
$widgetsDll = Get-ChildItem $build -Filter 'Qt6Widgets*.dll' -ErrorAction SilentlyContinue
if (-not $widgetsDll) {
    Die "windeployqt 没有拷来 Qt6Widgets 运行库，$exe 运行时会报错。请把上面 windeployqt 的输出发给我。"
}
Say "      OK  已部署 $($widgetsDll.Name)"

# ---- [6/6] 打包成可分发文件夹 ----
# 目的：产出一个"拷到任何 Windows 机器都能双击运行"的干净目录，
# 里面只有 exe + 必需的 Qt DLL + 说明，没有 .obj/.pdb/.ninja 这些构建杂物。
Say '[6/6] 打包可分发文件夹...'
$pkg = Join-Path $root 'package'
if (Test-Path $pkg) {
    Remove-Item $pkg -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $pkg -Force | Out-Null

Copy-Item $exe $pkg -Force

# Qt 运行库 + 平台插件（平台插件少了程序起不来）
Get-ChildItem $build -File -Filter '*.dll' |
    Where-Object { $_.Name -notlike 'oledfonttool*' } |
    Copy-Item -Destination $pkg -Force
if (Test-Path (Join-Path $build 'platforms')) {
    Copy-Item (Join-Path $build 'platforms') $pkg -Recurse -Force
}

# 附一份使用说明，别人拿到就能用
$readme = @"
OLED 汉字取模工具  ($Config 版)
================================

双击 oledfonttool.exe 运行。不需要安装 Qt，也不需要编译器。

用法
----
1. 左边输入要显示的汉字（多行也可以，会自动去重）
2. 右边实时显示每个字的 16x16 点阵
3. 点「导出 OLED_CFont.h」生成字库
4. 把生成的文件放进 Keil 工程的 Hardware 目录，Rebuild 即可

注意
----
本工具是配合 MyFreeRTOS_C8T6 工程用的，字库格式固定为：
    阴码 / 列行式 / 逆向(低位在前)
    每行 = 3 字节 UTF-8 编码 + 32 字节点阵 = 35 字节

如果导出时提示"需要手动搬运"，说明本程序没有直接写入工程目录的权限，
按提示把文件复制进 Hardware 目录也行。
"@
Set-Content -LiteralPath (Join-Path $pkg '使用说明.txt') -Value $readme -Encoding UTF8

$pkgSize = (Get-ChildItem $pkg -Recurse -File | Measure-Object Length -Sum).Sum
$pkgFiles = (Get-ChildItem $pkg -Recurse -File | Measure-Object).Count

Say ''
Say '============================================================'
Say ' 构建成功！'
Say ''
Say " 开发用（带调试信息）: $exe"
Say " 分发用（可拷走）    : $pkg"
Say ("   共 {0} 个文件, {1:N1} MB" -f $pkgFiles, ($pkgSize / 1MB))
Say '============================================================'
Finish 0
