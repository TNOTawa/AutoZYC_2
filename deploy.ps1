# AutoZYC v2.0 打包脚本
# 将脚本文件和 .mod2 模块打包为 .au2pkg.zip
# 用法: 在项目根目录运行 .\deploy.ps1

$ErrorActionPreference = "Stop"
$version = "2.0.0"
$root = $PSScriptRoot
if (!$root) { $root = Split-Path -Parent $MyInvocation.MyCommand.Path }
if (!$root) { $root = Get-Location }

$outDir = "$root\dist"
$tempDir = "$outDir\_pack_temp"

if (Test-Path $tempDir) { Remove-Item $tempDir -Recurse -Force }
New-Item -ItemType Directory -Path "$tempDir\Script\AutoZYC" -Force | Out-Null

# 脚本文件
Copy-Item "$root\script\@AutoZYC.obj2" "$tempDir\Script\AutoZYC\"
Copy-Item "$root\script\@AutoZYC.tra2" "$tempDir\Script\AutoZYC\"
Copy-Item "$root\script\@AutoZYC.anm2" "$tempDir\Script\AutoZYC\"

# .mod2 模块（脚本的必需依赖）
$mod2 = "$root\native\AutoZYCParse\build\AutoZYCParse.mod2"
if (!(Test-Path $mod2)) { Write-Error ".mod2 未找到，请先编译" }
Copy-Item $mod2 "$tempDir\Script\AutoZYC\"

# 包信息
Copy-Item "$outDir\package.ini" $tempDir
Copy-Item "$outDir\package.txt" $tempDir

# 生成 zip
$zip = "$outDir\AutoZYC-v$version.au2pkg.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$tempDir\*" -DestinationPath $zip

Remove-Item $tempDir -Recurse -Force
Write-Host "打包完成: $zip"
