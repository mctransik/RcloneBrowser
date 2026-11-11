# RcloneBrowser Cloud UI — first hands-on Windows acceptance (2026-11-11)

> **MANUAL TEST PLAN, NOT A PASS REPORT.** PR #5 is a draft UI integration
> test branch, based on the Cloud UI prototype from PR #3. Do not merge or publish on the strength of these checks.
> No real clouds, credentials, backups, NAS shares or personal files are needed.
> No approval for product distribution or an executable security certification.

## Gate A — validate the exact build before running it

- [ ] Record the precise branch commit SHA and GitHub Actions run ID from
      `integration/rc1-cloud-ui-2026-10-19`. Record the exact integration
      branch commit tested. Never substitute an artifact from another branch
      or call an older CI run current.
- [ ] Verify the current Windows and Linux CI jobs, Windows ClamAV scan, and
      security/report jobs relevant to the selected build. The artifact must
      be promoted from a **successful scanned run**; NEVER use
      `UNSCANNED-windows-build-inputs`.
- [ ] Confirm the selected artifact source SHA and published scan-report ZIP
      digest match. Scan results and attestations do not prove malware-free.
- [ ] Prefer an isolated Windows Sandbox/VM with an empty test profile and
      no production rclone configuration. **Do not disable SmartScreen,
      Defender, Bitdefender, Firefox warnings or another security control**
      to run an unsigned or flagged test binary. Record a warning as a
      blocking failure, not a hurdle to bypass.
- [ ] If there is no verified current build, STOP before executing anything.

## Gate B — prepare harmless local fixtures

Create two disposable empty folders in a sandbox/VM:
- `C:\RB-UI-TEST\Left`
- `C:\RB-UI-TEST\Right`

Using Windows Explorer, create a small text file `sample.txt` in
`Left` containing exactly `RcloneBrowser UI test — no private data`.
Also place a **different** `sample.txt` in `Right` containing
`EXISTING DESTINATION — DO NOT OVERWRITE`. Use ordinary Windows UI
controls; no shell or cloud account required. If an existing
`C:\RB-UI-TEST` folder is present, stop and choose a distinct empty
location; never delete an existing user's folder automatically.

This is a UI interaction exercise on the **integration prototype**, **not**
permission to touch real files. The current PR #3 feature-branch documentation
may be ahead in *documentation*, but no newer UI feature is implied here.

## Gate C — first 15-minute human acceptance

| ID | Action in the application | Required observation | Result |
| --- | --- | --- | --- |
| UI-01 | Launch the exact verified build in the isolated environment | Window opens without crash; correct development/preview identity | NOT TESTED |
| UI-02 | Open the Dashboard and return to the File Manager | Navigation works and clearly distinguishes unavailable account/capacity information | NOT TESTED |
| UI-03 | Set both panels to local storage and navigate to the two new empty test folders | Each panel independently displays the expected location and files | NOT TESTED |
| UI-04 | Use Back, Forward and Up in the active local panel | Navigation affects only the focused panel, no wrong-history jumps | NOT TESTED |
| UI-05 | Try selecting Left/sample.txt and copying toward Right | An explicit confirmation names source and destination; cancel starts nothing | NOT TESTED |
| UI-06 | Confirm an attempted copy with an already existing Right/sample.txt | Existing file stays byte-for-byte unchanged; no silent overwrite | NOT TESTED |
| UI-07 | Delete only the disposable destination test file **manually in Windows Explorer**, then repeat copy | Confirmed copy can complete and destination receives exact public fixture bytes | NOT TESTED |
| UI-08 | Open the Storage screen with no configured remotes | Unsupported/unavailable capacities are displayed honestly, without invented numbers | NOT TESTED |
| UI-09 | Open Security Center / About and inspect upstream and project links | No misleading claim that the app itself is signed or that a historical build has verified provenance | NOT TESTED |
| UI-10 | Exit and relaunch; revisit the two panes | Saved pane positions and widths behave as intended; no surprise transfers | NOT TESTED |

A failing check is a **bug report**, not a reason to loosen safety
controls. In particular, a copy failure does not authorize testing with
real documents or a live cloud.

## Record results before modifying product code

- Exact executable SHA-256:
- Branch SHA and Actions run URL:
- Operating system and isolation method:
- Which checks passed/failed (UI-01 … UI-10):
- Any unexpected prompts/antivirus warnings:
- A screenshot containing **only** the synthetic UI fixtures, if helpful:

Suggested next action: turn a reproducible failed UI-xx step into one
small, focused code change on a **small, isolated UI development branch**, with a test.
Keep the integration branch for validation rather than layering
unrelated fixes directly into it.
Do not extend the unrelated stacked cryptography research series.

Related plans:
- `docs/RC1_CLOUD_UI_INTEGRATION_2026-10-19.md`
- `docs/FILE_MANAGER_TEST_PLAN.md`
- `docs/PROJECT_IDENTITY_AUDIT.md`
- `docs/WINDOWS_LICENSE_OBLIGATIONS_2026-10-19.md`

**Status: checklist only; all results remain NOT TESTED until a person
executes and records them. No merge/release authorization.**
