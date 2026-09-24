# Next session — melonds

Updated 2026-09-24.

Upstream melonDS (DS) plus the OpenPak WFC redirect, the launch-time network profile, and a
website sign-in that names cloud saves (WFC minted no console identity). Latest tag `v0.2.2`;
tags moved from `openpak-v*` to `v*.*.*` on 09-23 and CI builds only on those.

Current status 2026-09-24: `v0.2.2`. WD-1/EP-4/EP-5, the release recipes and the UX-spec
menu, window, dialogs and settings (DS family) shipped in `openpak-v0.2.0`; since then the
signed redirect ceiling (openpak-client e180a57) and a Windows fix for redirect suffix
matching.

## Where things stand

- WFC redirect checkbox (Wi-Fi settings); melonDS's own indirect-mode DNS applies the
  profile-driven redirect list (EP-4, EP-5): fetched, validated against the compiled-in
  allowlist, else last-known-good, else built-in. Refresh button, source/version log line.
- Account + cloud saves (WD-1): per-ROM `.sav` sync under platform `ds`, cartridge game code
  as key; pull on load when local is missing or empty, push on eject/exit; conflicts resolve
  on openpak.org. Builds and links; not yet run against a game.
- Release recipes for every OS target upstream has (e762f85).

## Next steps

- First WFC match on OpenPak (E5 gate): a DS title matching against nn-wfc, alongside the
  Dolphin run of the same gate.

## Pointers

- [`../prds/`](../prds/README.md) — emulator-wide PRDs (`emulators/prds/` in the workspace):
  emulator-integration-prd.md (E5), emulator-network-profile-prd.md.
- `OPENPAK.md` — this fork's own readme.

## Scratch (research and throwaway work)

Decompiles, Ghidra projects, dumps, exefs/romfs extracts, packet captures,
strace and emulator logs, probe harnesses: put them in
`~/REPOS/Openpak/scratch/<topic>`. That folder is a local mount of the media pool,
outside every repository, so nothing in it is committed. Never use `/tmp` (a
shared 15 GB RAM disk) or elsewhere on `/home` for this. Keys and signing
material never go there. Rule: `docs/playbooks/conventions.md` in the workspace.
