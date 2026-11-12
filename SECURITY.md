# Security policy

## Version scope

Grain Field is experimental software. Security reports are accepted for the [latest published release](https://github.com/wallstrom/grain-field/releases/latest) (currently v0.8.0), the earlier v0.7.1 release and v0.7.0 pre-release, and the current `main` branch. Older releases have no promised security backports; fixes may require upgrading to a newer build.

Reports and fixes are handled on a best-effort basis. There is no guaranteed response time, remediation deadline, or long-term support commitment.

## Report a vulnerability privately

Use GitHub's [Report a vulnerability](https://github.com/wallstrom/grain-field/security/advisories/new) form, also available from this repository's **Security > Advisories** page. A GitHub account is required. Reports submitted through this channel are private to the reporter and authorized participants until an advisory is published.

Do not post vulnerabilities, exploit details, credentials, or private recordings in public issues, discussions, or pull requests. If the form is unavailable, open an issue containing only a request to arrange a private reporting channel, with no vulnerability details.

Include:

- The affected release or commit, macOS version, CPU architecture, and plugin format (AU, VST3, or standalone).
- The DAW and its version, if applicable.
- A description of the issue, its potential impact, and the conditions needed to trigger it.
- Minimal reproduction steps and, where needed, a small synthetic or otherwise shareable test file.
- Relevant logs with credentials, personal paths, and other private information removed.
- Any suggested mitigation or fix.

Test only on systems and files you own or have permission to use. Do not access others' data or disrupt shared services. Coordinate disclosure privately so a fix or mitigation can be prepared before technical details are made public.

Ordinary bugs and feature requests can be filed as [GitHub issues](https://github.com/wallstrom/grain-field/issues), provided they contain no sensitive information.

## Current distribution limitations

- Published binaries are not verified Developer ID-signed or notarized. A passing build or matching checksum is not a security audit or a guarantee that a binary is safe.
- Real DAW integration and other manual checks remain unverified; back up projects and existing plugin copies before evaluation.
- Download only from this repository's releases or Actions runs. Compare release ZIP checksums with the published release notes before installation.
- Do not disable Gatekeeper, lower startup security, or remove quarantine attributes as a routine workaround. Follow the [installation and macOS security guidance](docs/USER_GUIDE.md#10-build-in-the-cloud-instead-optional).

## Dependencies

Reports involving the build scripts, release workflow, bundled assets, or dependency integration are in scope. For a vulnerability originating in JUCE or another dependency, identify the affected upstream version and any existing advisory. Avoid publishing undisclosed upstream vulnerability details in a public issue here.

The project license is in [LICENSE](LICENSE). Dependencies, fonts, and sounds retain their separate licensing terms as described in the [README](README.md#licensing).
