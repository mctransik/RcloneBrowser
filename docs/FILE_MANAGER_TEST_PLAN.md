# Two-pane file manager — development smoke-test checklist

> **Not a test report.** These steps are still to be performed on a verified local build. This draft does not certify the software or demonstrate that the recent feature-branch code compiled successfully.

## Scope

Applies to `feature/cloud-control-ui-mvp`. Do not run these tests against the historical `v4.0.2-rc.1` release: it does **not** contain the independent two-pane interface.

Use disposable files and test remotes, **not** production backups. Do not bypass Firefox, SmartScreen or antivirus alerts for unsigned experimental executables. A clean CI job is not equivalent to a malware scan.

## Local browser navigation

1. Open **File manager** and select **This PC / local storage** in the left pane.
2. Visit existing test folders A, B and C by double-click or through the location field. After A → B → C, press **Back** twice and **Forward** once; confirm the displayed folder matches B.
3. From B, open a different folder D. **Forward** should become disabled; no stale path from the abandoned C branch should remain.
4. Verify **Alt+Left**, **Alt+Right** and **Alt+Up** navigate only the local pane containing keyboard focus. If both panes show local storage, focus the right one and confirm that pressing a shortcut does not change the left.
5. Check that Back is disabled when there is no earlier entry, and Forward when there is no later one.
6. Try an unavailable folder or a disconnected test NAS share. A warning should appear; the current folder and navigation history should not unexpectedly advance. Restore the share and retry.
7. Navigate more than 50 folders if practical; earlier history should be discarded without crashes or invalid button states.

## Existing two-pane functionality — regression checks

1. Set left pane to local storage and right pane to a disposable rclone remote. Switch each independently between local and cloud; neither should affect the other.
2. Drag the middle separator; verify it moves smoothly, the 50:50 button resets it, and pane widths remain after restarting.
3. In Details view, resize the Name/Size column boundary and another column. Verify widths remain for that side after view refresh and reopening the app.
4. Select a test file and use **Copy selected →**. Confirm that the source and destination in the confirmation dialog are correct.
5. Cancel the confirmation: no transfer should start. Confirm the copy on the second attempt: the transfer should appear in **Transfers**.
6. Repeat with a small disposable folder and then drag a test file between panes; ensure drag-and-drop **copies**, never silently moves/deletes.
7. Test copying a disposable file when an equally named destination already exists. Confirm existing destination content remains unchanged (rclone `--ignore-existing` behavior).
8. Attempt copying a folder into itself or a descendant in a local-to-local setup. The operation must be rejected before starting a transfer.
9. Verify your existing rclone crypt remote appears without exporting its secrets to the app UI, logs or public bug reports.

## Project identity checks

1. Click **Project on GitHub** in the sidebar. It must open `https://github.com/mctransik/RcloneBrowser`, not a page claiming this is Jackpison's official fork.
2. Open **Update & About** → **About Rclone Browser**. It must distinguish this independent development fork from upstream, link to our repository, and credit Jackpison, kapitainsky, Martins Mozeiko and DinCahill.
3. The About window must **not** claim that GitHub Issues are enabled or that our product has a finalized name.

## Release gate

- Capture the Windows and Linux CI status for the exact source commit.
- Record manual GUI test outcomes and the build/environment details separately.
- Investigate the previously observed Firefox malware warning **before** suggesting that testers run a flagged executable.
- Do not merge to `main`, retag RC1 or publish a new release without review and explicit approval.
