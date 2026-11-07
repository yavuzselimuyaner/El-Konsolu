# Writes the latest Retro-Go image and the games to the board.
param([string]$Port = 'COM6')
$ErrorActionPreference = 'Stop'

$vfsOffset = 0x380000
$vfsSize = 13107200

Set-Location $PSScriptRoot
. .\idf53.ps1
$python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
$image = Get-ChildItem retro-go\*.img | Sort-Object LastWriteTime | Select-Object -Last 1
New-Item -ItemType Directory -Force flash-cikti | Out-Null

$bytes = [IO.File]::ReadAllBytes($image.FullName)
[IO.File]::WriteAllBytes("$PSScriptRoot\flash-cikti\base.bin", $bytes[0..($vfsOffset - 1)])

& $python "$env:IDF_PATH\components\fatfs\wl_fatfsgen.py" roms-indirilen\sd --output_file flash-cikti\vfs.bin `
    --partition_size $vfsSize --sector_size 4096 --long_name_support

& $python -m esptool --chip esp32s3 --port $Port --baud 460800 write_flash --flash_size 16MB `
    0x0 flash-cikti\base.bin ('0x{0:X}' -f $vfsOffset) flash-cikti\vfs.bin
