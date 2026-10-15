# Windows package Authenticode and publisher metadata inventory.
# This script INSPECTS packaged executables; it does not run them, sign them,
# contact signature providers, restore quarantined files, or bypass security.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [Parameter(Mandatory = $true)][string]$ReportDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$outDir = [System.IO.Path]::GetFullPath($ReportDir)
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$csvPath = Join-Path $outDir "windows-authenticode-files.csv"
$summaryPath = Join-Path $outDir "windows-authenticode-summary.txt"
$summary = [System.Collections.Generic.List[string]]::new()
$summary.Add("Windows executable Authenticode provenance audit")
$summary.Add("result=not_completed")
$summary.Add("source_commit=$env:GITHUB_SHA")
$summary.Add("github_run_id=$env:GITHUB_RUN_ID")
$summary.Add("github_run_attempt=$env:GITHUB_RUN_ATTEMPT")
$summary.Add("audit_timestamp_utc=$([DateTime]::UtcNow.ToString('o'))")
$summary.Add("signature_valid_is_not_malware_safety=true")
$summary.Add("version_metadata_is_not_authenticated_without_valid_signature=true")

try {
    if (-not $IsWindows) {
        throw "Windows Authenticode verification must run on a Windows host."
    }
    $root = (Resolve-Path -LiteralPath $PackageDir -ErrorAction Stop).Path
    $appExe = Join-Path $root "RcloneBrowser.exe"
    $rcloneExe = Join-Path $root "rclone\rclone.exe"
    if (-not (Test-Path -LiteralPath $appExe -PathType Leaf) -or
        -not (Test-Path -LiteralPath $rcloneExe -PathType Leaf)) {
        throw "Required main executable or bundled rclone.exe is missing."
    }

    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction Stop |
        Where-Object { $_.Extension -in @(".exe", ".dll") } |
        Sort-Object FullName)
    if (@($files | Where-Object { $_.Extension -ieq ".dll" }).Count -lt 1) {
        throw "No package DLLs found; cannot audit dependencies."
    }

    $rows = [System.Collections.Generic.List[object]]::new()
    foreach ($item in $files) {
        $signature = Get-AuthenticodeSignature -LiteralPath $item.FullName -ErrorAction Stop
        $cert = $signature.SignerCertificate
        $stamp = $signature.TimeStamperCertificate
        $info = $item.VersionInfo
        $relative = [System.IO.Path]::GetRelativePath($root, $item.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()

        $rows.Add([pscustomobject][ordered]@{
            relative_path = $relative
            file_size_bytes = $item.Length
            sha256 = $hash
            signature_status = [string]$signature.Status
            signature_type = [string]$signature.SignatureType
            signature_status_message = [string]$signature.StatusMessage
            signer_subject = if ($null -ne $cert) { $cert.Subject } else { "" }
            signer_issuer = if ($null -ne $cert) { $cert.Issuer } else { "" }
            signer_thumbprint = if ($null -ne $cert) { $cert.Thumbprint } else { "" }
            signer_valid_from_utc = if ($null -ne $cert) { $cert.NotBefore.ToUniversalTime().ToString("o") } else { "" }
            signer_valid_until_utc = if ($null -ne $cert) { $cert.NotAfter.ToUniversalTime().ToString("o") } else { "" }
            timestamp_signer_subject = if ($null -ne $stamp) { $stamp.Subject } else { "" }
            company_name_metadata = [string]$info.CompanyName
            product_name_metadata = [string]$info.ProductName
            file_version_metadata = [string]$info.FileVersion
            original_filename_metadata = [string]$info.OriginalFilename
        })
    }

    # Keep all findings in a dedicated report OUTSIDE the distributable ZIP.
    $rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding utf8

    $valid = @($rows | Where-Object { $_.signature_status -eq "Valid" })
    $unsigned = @($rows | Where-Object { $_.signature_status -eq "NotSigned" })
    $unexpected = @($rows | Where-Object {
        $_.signature_status -notin @("Valid", "NotSigned")
    })
    $summary.Add("checked_exe_dll_files=$($rows.Count)")
    $summary.Add("authenticode_valid=$($valid.Count)")
    $summary.Add("authenticode_unsigned=$($unsigned.Count)")
    $summary.Add("authenticode_invalid_or_untrusted_or_unknown=$($unexpected.Count)")

    foreach ($group in ($rows | Group-Object signature_status | Sort-Object Name)) {
        $summary.Add("status_count_$($group.Name)=$($group.Count)")
    }
    foreach ($row in $rows) {
        $summary.Add("file=$($row.relative_path) status=$($row.signature_status) sha256=$($row.sha256)")
    }

    $summary.Add("result=audited_signatures_only")
    if ($unexpected.Count -gt 0) {
        $summary.Add("result=blocked_unexpected_signature_status")
        throw "$($unexpected.Count) file(s) have unexpected/invalid Windows signature status. Manual review required."
    }
    Write-Host "Windows Authenticode audit: $($rows.Count) EXE/DLL files; $($valid.Count) valid, $($unsigned.Count) unsigned, $($unexpected.Count) abnormal."
    Write-Host "No malware-free verdict or release approval is implied."
}
catch {
    $summary.Add("error=$($_.Exception.Message -replace '[\r\n]+', ' ')")
    Write-Host "::error::Windows Authenticode inventory failed: $($_.Exception.Message)"
    throw
}
finally {
    $summary | Set-Content -LiteralPath $summaryPath -Encoding utf8
}
