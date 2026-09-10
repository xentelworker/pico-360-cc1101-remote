param(
    [string]$ConfigPath = (Join-Path $PSScriptRoot 'pico360-config.txt')
)

$ErrorActionPreference = 'SilentlyContinue'

Write-Host 'Pico360 COM Port Finder'
Write-Host '========================'
Write-Host ''

$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
if (-not $ports) {
    Write-Host 'No COM ports found.'
    exit 1
}

$found = $null

foreach ($name in $ports) {
    Write-Host "Testing $name ..."
    $sp = $null
    try {
        $sp = New-Object System.IO.Ports.SerialPort $name,115200,'None',8,'One'
        $sp.ReadTimeout = 700
        $sp.WriteTimeout = 700
        $sp.DtrEnable = $true
        $sp.RtsEnable = $false
        $sp.NewLine = "`n"
        $sp.Open()
        Start-Sleep -Milliseconds 350
        $sp.DiscardInBuffer()
        $sp.WriteLine('PING')

        $deadline = [DateTime]::UtcNow.AddMilliseconds(1200)
        $response = ''
        while ([DateTime]::UtcNow -lt $deadline) {
            try {
                $line = $sp.ReadLine()
                $response += $line + "`n"
                if ($line -match 'PICO360 READY') {
                    $found = $name
                    break
                }
            } catch { }
        }
    } catch { }
    finally {
        if ($sp -and $sp.IsOpen) { $sp.Close() }
        if ($sp) { $sp.Dispose() }
    }

    if ($found) { break }
}

if (-not $found) {
    Write-Host ''
    Write-Host 'Pico360 controller was not detected.'
    Write-Host 'Close Arduino Serial Monitor and the Pico360 Windows controller, then try again.'
    exit 2
}

Write-Host ''
Write-Host "Pico360 found on $found"

$existing = @()
if (Test-Path $ConfigPath) {
    $existing = Get-Content $ConfigPath
}

$updated = @()
$replaced = $false
foreach ($line in $existing) {
    if ($line -match '^COMPORT=') {
        $updated += "COMPORT=$found"
        $replaced = $true
    } else {
        $updated += $line
    }
}

if (-not $replaced) {
    $updated = @("COMPORT=$found") + $updated
}

$updated | Set-Content -Path $ConfigPath -Encoding ASCII
Write-Host "Updated: $ConfigPath"
Write-Host ''
Write-Host 'You can now use Pico360Trigger.bat from dslrBooth.'
