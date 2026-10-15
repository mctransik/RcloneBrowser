# Historical Windows EXE/DLL SHA-256 audit — 2026-10-15

**Status: CI-validated on 2026-10-15; findings and unresolved limitations below.** Hash collection is not a malware or signature verdict. The original Firefox warning remains open.

The existing manual Update workflow historical security job now calculates SHA-256 hashes of every EXE and DLL in all six hash-pinned Windows Actions artifacts (10–11 October). The outer ZIP is verified before controlled extraction. No executable is launched.

Two reports are produced in the existing text-only historical-windows-malware-investigation GitHub artifact:

- historical-windows-pe-sha256.tsv: date, source run, artifact ID, relative path, size and SHA-256 of every EXE/DLL for each of six historical packages.
- historical-windows-unique-pe-sha256.tsv: deduplicated SHA-256, size, number of runs, source run IDs and all relative paths for shared DLLs and EXEs.

The report collector verifies presence of RcloneBrowser.exe, bundled rclone.exe and at least one DLL per artifact, and runs offline unit tests with synthetic bytes before downloading artifacts. The existing ClamAV scan and release lock remain unchanged. Only textual reports are uploaded by the historic audit job; no old binaries are published anew.

After a successful CI run, prioritize hash-only lookups of historical RcloneBrowser.exe files at VirusTotal, then rclone.exe and distinct Qt or MSVC DLLs. No submission, reanalysis or bypassing browser defenses is required. A report not found means UNKNOWN; even a 0-finding multi-engine report does not guarantee absence of malware.

Related: [Historical ZIP audit](HISTORICAL_WINDOWS_MALWARE_AUDIT_2026-10-14.md) and [Firefox warning triage](FIREFOX_MALWARE_HASH_TRIAGE_2026-10-14.md).

## Verified results — 2026-10-15

[GitHub Actions run #38708969697](https://github.com/mctransik/RcloneBrowser/actions/runs/38708969697) on commit `00c83e8bb3d6c10f959eda0e8808bf130c5c6940` completed successfully. The **downloaded text-only report artifact** `historical-windows-malware-investigation` (artifact ID `11960117099`) was independently opened and parsed for the following values:

- All six outer Actions ZIPs matched the pinned hashes, and each contained 67 ZIP members.
- Across the six packages, there are 156 EXE/DLL instances = 26 per build (2 EXEs and 24 DLLs).
- The deduplicated manifest contains **31 unique EXE/DLL hashes**: six different `RcloneBrowser.exe` binaries and 25 hashes shared among all six builds (one bundled `rclone.exe` and 24 DLLs).
- The bundled `rclone/rclone.exe` has identical SHA-256 in all six packages: `41eab3e118d264920b62c3327966c5ece0dc4dd715ee617d9e30450d8ead95a4`.
- Historical ClamAV 1.5.4 scan: 408 files, 404.18 MiB, **0 detected infected files**; malware database updated to daily v28154. The two synthetic-byte unit tests passed. The newer Windows package scan also reported 70 files and 0 detected infections.

### Exact inner `RcloneBrowser.exe` hashes

| Date | Historical run | EXE SHA-256 | VirusTotal hash-only existing-report lookup |
|---|---|---|---|
| 2026-10-10 | [#38064040344](https://github.com/mctransik/RcloneBrowser/actions/runs/38064040344) | `dfdedcd0ed8ec1b5769c21e1ed4d650132207736c1ef9ce31919364d5347490e` | [Existing report](https://www.virustotal.com/gui/file/dfdedcd0ed8ec1b5769c21e1ed4d650132207736c1ef9ce31919364d5347490e/detection) |
| 2026-10-10 | [#38067162852](https://github.com/mctransik/RcloneBrowser/actions/runs/38067162852) | `fd5d46c8b364c3c30004ed33047c4bb9bc77a0e2bbc764d3f905d79780e594d3` | [Existing report](https://www.virustotal.com/gui/file/fd5d46c8b364c3c30004ed33047c4bb9bc77a0e2bbc764d3f905d79780e594d3/detection) |
| 2026-10-11 | [#38122324539](https://github.com/mctransik/RcloneBrowser/actions/runs/38122324539) | `231e96f48a7f7f68f45eadf504cad9861535a97aa9eb363ad5bc11a553492b7f` | [Existing report](https://www.virustotal.com/gui/file/231e96f48a7f7f68f45eadf504cad9861535a97aa9eb363ad5bc11a553492b7f/detection) |
| 2026-10-11 | [#38123394766](https://github.com/mctransik/RcloneBrowser/actions/runs/38123394766) | `1d36378b6cc29dbbd5f5dd323890147635030edf0c0df8d328092443d2254419` | [Existing report](https://www.virustotal.com/gui/file/1d36378b6cc29dbbd5f5dd323890147635030edf0c0df8d328092443d2254419/detection) |
| 2026-10-11 | [#38126413180](https://github.com/mctransik/RcloneBrowser/actions/runs/38126413180) | `3ec701a923d2758799ade3b678f0f2473367656542fa36ae4e7402b24b08a002` | [Existing report](https://www.virustotal.com/gui/file/3ec701a923d2758799ade3b678f0f2473367656542fa36ae4e7402b24b08a002/detection) |
| 2026-10-11 | [#38129020027](https://github.com/mctransik/RcloneBrowser/actions/runs/38129020027) | `a676befda5be579f419d31d83193034f9f3ebf750c98b79564d6ea163b567968` | [Existing report](https://www.virustotal.com/gui/file/a676befda5be579f419d31d83193034f9f3ebf750c98b79564d6ea163b567968/detection) |

**Important:** The VirusTotal links above are simply hash queries. No external VirusTotal report content has been retrieved or evaluated as part of this investigation, and a missing result is not clearance. The historical Firefox-blocked download remains unidentified by precise sample hash. No DLL or EXE was run during the historical audit, and automatic release remains disabled.
