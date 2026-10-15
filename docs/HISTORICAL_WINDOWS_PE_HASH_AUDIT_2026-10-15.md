# Historical Windows EXE/DLL SHA-256 audit — 2026-10-15

**Status: code committed, CI test pending.** Hash collection is not a malware or signature verdict. The original Firefox warning remains open.

The existing manual Update workflow historical security job now calculates SHA-256 hashes of every EXE and DLL in all six hash-pinned Windows Actions artifacts (10–11 October). The outer ZIP is verified before controlled extraction. No executable is launched.

Two reports are produced in the existing text-only historical-windows-malware-investigation GitHub artifact:

- historical-windows-pe-sha256.tsv: date, source run, artifact ID, relative path, size and SHA-256 of every EXE/DLL for each of six historical packages.
- historical-windows-unique-pe-sha256.tsv: deduplicated SHA-256, size, number of runs, source run IDs and all relative paths for shared DLLs and EXEs.

The report collector verifies presence of RcloneBrowser.exe, bundled rclone.exe and at least one DLL per artifact, and runs offline unit tests with synthetic bytes before downloading artifacts. The existing ClamAV scan and release lock remain unchanged. Only textual reports are uploaded by the historic audit job; no old binaries are published anew.

After a successful CI run, prioritize hash-only lookups of historical RcloneBrowser.exe files at VirusTotal, then rclone.exe and distinct Qt or MSVC DLLs. No submission, reanalysis or bypassing browser defenses is required. A report not found means UNKNOWN; even a 0-finding multi-engine report does not guarantee absence of malware.

Related: [Historical ZIP audit](HISTORICAL_WINDOWS_MALWARE_AUDIT_2026-10-14.md) and [Firefox warning triage](FIREFOX_MALWARE_HASH_TRIAGE_2026-10-14.md).
