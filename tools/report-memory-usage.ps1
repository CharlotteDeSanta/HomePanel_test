param(
    [string]$ElfPath = "$(Join-Path $PSScriptRoot '..\build\Debug\HomePanel_test.elf')",
    [string]$SizeToolPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-SizeTool {
    param([string]$RequestedPath)

    if ($RequestedPath -and (Test-Path $RequestedPath)) {
        return (Resolve-Path $RequestedPath).Path
    }

    $cmd = Get-Command arm-none-eabi-size.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $bundleRoot = Join-Path $env:LOCALAPPDATA 'stm32cube\bundles\gnu-tools-for-stm32'
    if (Test-Path $bundleRoot) {
        $candidate = Get-ChildItem -Path $bundleRoot -Recurse -Filter arm-none-eabi-size.exe -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($candidate) {
            return $candidate.FullName
        }
    }

    throw "Unable to locate arm-none-eabi-size.exe. Pass -SizeToolPath explicitly."
}

function Add-ToRegion {
    param(
        [hashtable]$Regions,
        [string]$Name,
        [UInt64]$Size
    )

    $Regions[$Name].Used += $Size
}

$resolvedElfPath = Resolve-Path $ElfPath
$resolvedSizeTool = Resolve-SizeTool -RequestedPath $SizeToolPath

$sizeOutput = & $resolvedSizeTool -A $resolvedElfPath.Path
if ($LASTEXITCODE -ne 0) {
    throw "arm-none-eabi-size failed with exit code $LASTEXITCODE"
}

$regions = @{
    'FLASH' = @{ Base = [UInt64]134217728; End = [UInt64]136314880; Size = 2MB; Used = [UInt64]0 }
    'EXTFLASH' = @{ Base = [UInt64]2415919104; End = [UInt64]2449473536; Size = 32MB; Used = [UInt64]0 }
    'DTCMRAM' = @{ Base = [UInt64]536870912; End = [UInt64]537001984; Size = 128KB; Used = [UInt64]0 }
    'RAM' = @{ Base = [UInt64]603979776; End = [UInt64]604504064; Size = 512KB; Used = [UInt64]0 }
    'RAM_D2' = @{ Base = [UInt64]805306368; End = [UInt64]805601280; Size = 288KB; Used = [UInt64]0 }
    'RAM_D3' = @{ Base = [UInt64]939524096; End = [UInt64]939589632; Size = 64KB; Used = [UInt64]0 }
    'ITCMRAM' = @{ Base = [UInt64]0; End = [UInt64]65536; Size = 64KB; Used = [UInt64]0 }
}

$sectionRows = @()
foreach ($line in $sizeOutput) {
    if ($line -match '^\s*section\s+size\s+addr\s*$') {
        continue
    }
    if ($line -match '^\s*(?<name>\S+)\s+(?<size>\d+)\s+(?<addr>\d+)\s*$') {
        $sectionRows += [PSCustomObject]@{
            Name = $matches['name']
            Size = [UInt64]$matches['size']
            Addr = [UInt64]$matches['addr']
        }
    }
}

foreach ($row in $sectionRows) {
    if (($row.Name -like '.debug*') -or ($row.Name -eq '.comment') -or ($row.Name -eq '.ARM.attributes')) {
        continue
    }

    foreach ($regionName in $regions.Keys) {
        $region = $regions[$regionName]
        if (($row.Addr -ge $region.Base) -and ($row.Addr -lt $region.End)) {
            Add-ToRegion -Regions $regions -Name $regionName -Size $row.Size
            break
        }
    }
}

Write-Host ""
Write-Host "Memory Usage Report" -ForegroundColor Cyan
Write-Host "ELF: $($resolvedElfPath.Path)"
Write-Host "size: $resolvedSizeTool"
Write-Host ""
Write-Host ("{0,-10} {1,12} {2,12} {3,9}" -f "Region", "Used", "Total", "Used %")
Write-Host ("{0,-10} {1,12} {2,12} {3,9}" -f "------", "----", "-----", "------")

foreach ($regionName in @('FLASH', 'EXTFLASH', 'DTCMRAM', 'RAM', 'RAM_D2', 'RAM_D3', 'ITCMRAM')) {
    $region = $regions[$regionName]
    $percent = if ($region.Size -gt 0) { [math]::Round(($region.Used * 100.0) / $region.Size, 2) } else { 0 }
    Write-Host ("{0,-10} {1,12} {2,12} {3,8}%" -f $regionName, $region.Used, $region.Size, $percent)
}

Write-Host ""
Write-Host "Top allocated sections:" -ForegroundColor Cyan
$sectionRows |
    Where-Object { $_.Addr -ne 0 } |
    Sort-Object Size -Descending |
    Select-Object -First 10 |
    ForEach-Object {
        Write-Host ("  {0,-24} {1,10} @ 0x{2}" -f $_.Name, $_.Size, $_.Addr.ToString('X8'))
    }
