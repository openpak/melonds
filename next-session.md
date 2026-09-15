# Next session — melonds

Updated 2026-09-15.

Upstream melonDS (DS) plus the OpenPak WFC redirect, the launch-time network profile, and a
website sign-in that names cloud saves (WFC minted no console identity). Released through
`openpak-v0.1.2`; the cloud-save/profile/sign-in work and the full-OS release recipes are
committed but untagged.

## Where things stand

- WFC redirect checkbox (Wi-Fi settings); melonDS's own indirect-mode DNS applies the
  profile-driven redirect list (EP-4, EP-5): fetched, validated against the compiled-in
  allowlist, else last-known-good, else built-in. Refresh button, source/version log line.
- Account + cloud saves (WD-1): per-ROM `.sav` sync under platform `ds`, cartridge game code
  as key; pull on load when local is missing or empty, push on eject/exit; conflicts resolve
  on openpak.org. Builds and links; not yet run against a game.
- Release recipes for every OS target upstream has (e762f85). Untagged.

## Next steps

- Local build, cut the next `openpak-v*` tag with WD-1/EP-4/EP-5.
- First WFC match on OpenPak (E5 gate): a DS title matching against nn-wfc, alongside the
  Dolphin run of the same gate.

## Pointers

- [`../prds/`](../prds/README.md) — emulator-wide PRDs (`emulators/prds/` in the workspace):
  emulator-integration-prd.md (E5), emulator-network-profile-prd.md.
- `OPENPAK.md` — this fork's own readme.
