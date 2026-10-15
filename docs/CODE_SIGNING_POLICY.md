# Code signing policy — draft (not active)

> **Status: preparation only.** This project is **not currently enrolled** with SignPath Foundation, has **no SignPath-approved certificate**, and does **not offer signed project releases**. Do not interpret a successful GitHub Actions build, a SHA-256 digest, or a GitHub artifact attestation as an Authenticode signature.

Project repository: https://github.com/mctransik/RcloneBrowser

This policy is being prepared against [SignPath Foundation's OSS conditions](https://signpath.org/terms.html) and [SignPath's origin-verification documentation](https://docs.signpath.io/origin-verification/). It describes planned release-signing controls, **not existing enforced settings**.

## Scope and status

- Current deliverables: unsigned portable Windows application package and Linux test artifacts. The development branch is not an authorized signing source.
- A finished project name, icon, publisher metadata and release version policy remain to be chosen. Existing files still carry inherited “Rclone Browser” naming.
- Signatures must be applied **only after** project acceptance, artifact-configuration review and approval of a specific release. A signed binary confirms origin/integrity within its certification chain, **not** that it is malware-free.
- Bundled third-party binaries (such as upstream rclone, Qt libraries and the Microsoft VC++ runtime) must not be represented as independently authored by this project or signed as our own executables. Follow the foundation's upstream/fork rules and all redistribution terms.

## Signing responsibilities (proposed)

Roles must be verified and the responsible people or GitHub permission groups published **before applying**:

| Role | Responsibility | Assigned |
| --- | --- | --- |
| Authors / committers | Write and maintain source and build scripts | To be documented |
| Reviewers | Review proposed changes, dependencies and build scripts before release | To be documented |
| Signing approvers | Independently decide whether a specific release may be signed | To be documented |

- Maintainers, reviewers and approvers must use multi-factor authentication for GitHub and SignPath.
- Non-committer contributions must be reviewed by an authorized reviewer before inclusion.
- A signing request must **always receive manual approval** by a designated approver; automated build success alone is insufficient.
- Signing access is never granted to arbitrary external pull requests or untrusted forks.

## Proposed release-signing procedure (not yet implemented)

1. Review source changes, CMake files, GitHub Actions workflows, build inputs, included third-party binaries and licenses. Resolve findings before a release.
2. Select a reviewed, protected release ref. Development-branch and pull-request artifacts are **not signable**.
3. Build through a trusted CI system using the reviewed ref; confirm commit identity and build provenance. Restrict dependencies and network downloads sufficiently to support origin verification. The current CI pins the rclone version and validates its PGP-signed checksums plus a committed expected archive SHA-256. Qt is set to a fixed version but retrieved using a cached installer, and other supply-chain inputs are not yet fully reproducible; independent dependency provenance still needs assessment.
4. Inspect the resulting release candidate for suspicious behavior; investigate reports and antivirus detections instead of suppressing warnings. Verify package content, hashes, version resources and publisher metadata.
5. Submit the exact built artifact to an approved SignPath signing policy using verified build-origin data. Require manual sign-off for **each** signing request.
6. Verify the returned Authenticode signature, expected publisher identity, timestamp and artifact digest before publishing.
7. Publish only the approved artifact, its hashes, provenance, third-party notices, installation/removal instructions and a release entry linking back to this policy. Have a documented revocation/incident response process.

## Required disclosures before an application

- Document the project's actual public release and download page. The older `v4.0.2-rc.1` is **not** a release of the planned new cloud-management UI.
- Review eligibility for signing a derivative of [Jackpison/RcloneExplorer](https://github.com/Jackpison/RcloneExplorer). The foundation has additional conditions on forks and upstream signing; compliance is **not established**.
- Confirm the repository's contributor/reviewer/approver roles, MFA and protected release permissions.
- Complete the [privacy and network behavior draft](PRIVACY.md) and [third-party license review](THIRD_PARTY_REVIEW.md).
- Verify complete Windows package metadata (product name and consistent version); current branding is transitional.
- Check whether the foundation accepts the project after independent reputation and security review. Acceptance is discretionary.

If the project is eventually accepted, its public release and homepage policy must include the **exact acknowledgement** required by the foundation:

> Free code signing provided by SignPath.io, certificate by SignPath Foundation

**That sentence is an acknowledgement template, not a claim that this project currently receives the service.**

## Current evidence and limitations

- CI definition: [`.github/workflows/update.yml`](../.github/workflows/update.yml).
- The **release-tag job is disabled** while security review continues; its planned checksum/attestation/publishing steps do not run. A separate, manual, **development-branch-only** `windows_provenance` job has been implemented after successful ClamAV review of the Windows ZIP and awaits its first CI validation; it likewise does not create an Authenticode-signed executable.
- The automated pipeline builds and tests Windows/Linux; a clean build is not a malware verdict.
- The project currently distributes a portable directory ZIP, not a signed Windows installer.
- No SignPath signing key, organization identifier or token should be added to this repository.

Security or signing status must remain clearly marked **unverified** until the necessary independent checks are actually completed.

## Development-only provenance gate (CI validated; not a release approval)

See [scanned Windows build provenance plan](WINDOWS_BUILD_PROVENANCE_2026-10-15.md). The gate verifies the SHA-256 of the promoted ZIP against the scanner-produced checksum, then requests an OIDC-backed GitHub repository attestation and enforces exact repository, workflow, source commit and branch with the GitHub CLI. The attestation and its separate CLI verification succeeded on [GitHub run #38746642831](https://github.com/mctransik/RcloneBrowser/actions/runs/38746642831); the exact scanned Windows ZIP SHA-256 is `307c0a783c1ca133173bce180e3b5d5394e8f19d0efcb5e7a6794c2bafca2176`. **This is not a code-signing certificate or permission to release.**
