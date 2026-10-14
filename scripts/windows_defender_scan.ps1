# Mandatory, fail-closed Windows Defender scan for GitHub runner packages.
# Never disable protection, add exclusions, or restore quarantined files.
param(
  [Parameter(Mandatory=$true)][string]$PackageDir,
  [Parameter(Mandatory=$true)][string]$ArchivePath,
  [Parameter(Mandatory=$true)][string]$ReportPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$report = [System.Collections.Generic.List[string]]::new()
$report.Add("scanner=Microsoft Defender Antivirus")
$report.Add("result=not_completed")
$report.Add("commit=$env:GITHUB_SHA")
$report.Add("run_id=$env:GITHUB_RUN_ID")
$report.Add("scan_started_utc=$([DateTime]::UtcNow.ToString('o'))")
$file = [IO.Path]::GetFullPath($ReportPath)
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file) | Out-Null
try {
  $folder = (Resolve-Path -LiteralPath $PackageDir).Path
  $archive = (Resolve-Path -LiteralPath $ArchivePath).Path
  $files = @(Get-ChildItem -LiteralPath $folder -Recurse -File)
  if (-not (Test-Path -LiteralPath (Join-Path $folder "RcloneBrowser.exe") -PathType Leaf) -or
      -not (Test-Path -LiteralPath (Join-Path $folder "rclone\rclone.exe") -PathType Leaf) -or
      @($files | Where-Object { $_.Extension -ieq ".dll" }).Count -lt 1) {
    throw "Required Windows executables or bundled DLLs are missing."
  }
  $hashes = @{}
  foreach ($item in $files) {
    $hashes[$item.FullName] = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
  }
  $zipHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
  $report.Add("archive_name=$([IO.Path]::GetFileName($archive))")
  $report.Add("archive_sha256=$($zipHash.ToLowerInvariant())")
  $report.Add("file_count=$($files.Count)")
  foreach ($cmd in @("Get-MpComputerStatus", "Update-MpSignature", "Start-MpScan", "Get-MpThreatDetection")) {
    if (-not (Get-Command -Name $cmd -ErrorAction SilentlyContinue)) {
      throw "Required Defender command unavailable: $cmd"
    }
  }
  try {
    Update-MpSignature -ErrorAction Stop | Out-Null
    $report.Add("signature_update=success")
  } catch {
    $report.Add("signature_update=failed_older_signatures_must_still_be_fresh")
    Write-Warning "Signature update failed; checking available definition age."
  }
  $status = Get-MpComputerStatus -ErrorAction Stop
  $report.Add("antivirus_enabled=$($status.AntivirusEnabled)")
  $report.Add("running_mode=$($status.AMRunningMode)")
  $report.Add("signature_version=$($status.AntivirusSignatureVersion)")
  $report.Add("signature_updated=$($status.AntivirusSignatureLastUpdated)")
  if (-not $status.AntivirusEnabled -or $status.AMRunningMode -ne "Normal") {
    throw "Defender is not running in active antivirus mode."
  }
  if ([string]::IsNullOrWhiteSpace([string]$status.AntivirusSignatureVersion) -or
      $null -eq $status.AntivirusSignatureLastUpdated -or
      ((Get-Date) - [datetime]$status.AntivirusSignatureLastUpdated).TotalDays -gt 3) {
    throw "Microsoft Defender signature data is stale or unavailable."
  }
  Write-Host "Scanning packaged directory..."
  Start-MpScan -ScanType CustomScan -ScanPath $folder -ErrorAction Stop
  Write-Host "Scanning distributable ZIP..."
  Start-MpScan -ScanType CustomScan -ScanPath $archive -ErrorAction Stop
  $detections = @(Get-MpThreatDetection -ErrorAction Stop)
  $report.Add("threat_detection_count=$($detections.Count)")
  if ($detections.Count -gt 0) {
    foreach ($d in $detections) { $report.Add("detected_threat_id=$($d.ThreatID)") }
    throw "Malware detection found. Artifact upload is blocked."
  }
  $afterFiles = @(Get-ChildItem -LiteralPath $folder -Recurse -File)
  if ($afterFiles.Count -ne $files.Count) { throw "Scanned file count changed." }
  foreach ($item in $afterFiles) {
    if (-not $hashes.ContainsKey($item.FullName) -or
        $hashes[$item.FullName] -ne (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash) {
      throw "A packaged file changed during the scan."
    }
  }
  if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $zipHash) {
    throw "The distribution ZIP changed during the scan."
  }
  $final = Get-MpComputerStatus -ErrorAction Stop
  if (-not $final.AntivirusEnabled -or $final.AMRunningMode -ne "Normal") {
    throw "Defender stopped during the scan."
  }
  $report[1] = "result=no_detections_reported"
  Write-Host "Defender completed both scans with no reported detections."
}
catch {
  $report[1] = "result=failed_or_unavailable"
  $report.Add("reason=$($_.Exception.Message -replace '[\r\n]+', ' ')")
  Write-Host "::error::Defender scan gate failed: $($_.Exception.Message)"
  throw
}
finally {
  $report | Set-Content -LiteralPath $file -Encoding utf8
}
