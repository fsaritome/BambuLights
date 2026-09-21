# One-shot flash for the ESP32-WROOM-32 build.
#
# Writes bootloader + partition table + otadata + firmware + web GUI in a
# SINGLE esptool call, so the BOOT/EN dance is needed once instead of twice,
# and PlatformIO's build step is skipped entirely (it flashes whatever is
# already in .pio/build/wroom32).
#
#   .\flash.ps1               # flashes everything on COM5
#   .\flash.ps1 -Port COM7    # different port
#   .\flash.ps1 -FirmwareOnly # skip the web GUI, faster
#   .\flash.ps1 -Monitor      # open the serial monitor afterwards
#
# Run `pio run -e wroom32` yourself first if you changed any source.

param(
    [string] $Port = "COM5",
    [int]    $Baud = 460800,
    [switch] $FirmwareOnly,
    [switch] $Monitor,
    # This board's auto-reset circuit is unreliable, so you put it into
    # download mode by hand (hold BOOT, tap EN, release EN then BOOT).
    # esptool's default reset sequence would knock it straight back out of
    # that state, so we skip it. Pass -AutoReset on a board that resets
    # properly and you want to avoid the button dance.
    [switch] $AutoReset
)

$ErrorActionPreference = "Stop"

$pio       = "$env:USERPROFILE\.platformio"
$python    = "$pio\penv\Scripts\python.exe"
$esptool   = "$pio\packages\tool-esptoolpy\esptool.py"
$bootApp0  = "$pio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
$build     = "$PSScriptRoot\.pio\build\wroom32"

foreach ($f in @($python, $esptool, $bootApp0, "$build\firmware.bin")) {
    if (-not (Test-Path $f)) { throw "missing: $f`nRun 'pio run -e wroom32' first." }
}

# Offsets must match partitions_wroom32.csv. spiffs lives at 0x370000 there;
# if you edit the partition table, change it here too.
$parts = @(
    "0x1000",   "$build\bootloader.bin",
    "0x8000",   "$build\partitions.bin",
    "0xe000",   $bootApp0,
    "0x10000",  "$build\firmware.bin"
)

if (-not $FirmwareOnly) {
    if (-not (Test-Path "$build\littlefs.bin")) {
        throw "littlefs.bin missing. Run 'pio run -e wroom32 --target buildfs', or pass -FirmwareOnly."
    }
    $parts += @("0x370000", "$build\littlefs.bin")
}

$before = if ($AutoReset) { "default_reset" } else { "no_reset" }

Write-Host ""
Write-Host "Put the board in download mode FIRST: hold BOOT, tap EN," -ForegroundColor Yellow
Write-Host "release EN then BOOT. Then run this." -ForegroundColor Yellow
Write-Host "  (--before $before)" -ForegroundColor DarkGray
Write-Host ""

& $python $esptool --chip esp32 --port $Port --baud $Baud `
    --before $before --after hard_reset `
    write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect @parts

if ($LASTEXITCODE -ne 0) { throw "flash failed (exit $LASTEXITCODE)" }

Write-Host ""
Write-Host "Flashed. The board is rebooting." -ForegroundColor Green
Write-Host "After the first flash you never need USB again - use OTA at" -ForegroundColor Green
Write-Host "http://bambulights.local/update (user 'update', password 'secretsauce')." -ForegroundColor Green

if ($Monitor) {
    & "$pio\penv\Scripts\pio.exe" device monitor -p $Port -b 115200
}
