# melonDS for OpenPak

melonDS for OpenPak is OpenPak's Nintendo DS emulator fork, built on [melonDS](https://github.com/melonDS-emu/melonDS). It is an independent project. Upstream melonDS remains our base, and we merge upstream changes as they land. This is not an official melonDS release.

The fork adds a Nintendo WFC redirect to OpenPak, cloud saves driven by the emulator, and the OpenPak sign-in, menu and settings that every OpenPak emulator shares. Everything else is upstream melonDS.

**Status:** Transitional (kept for now, successor planned, no builds advertised).

## Not affiliated

This is an independent project and is not endorsed by the melonDS project. It is not affiliated with Nintendo, and all trademarks belong to their owners.

## What works today

Sourced from the fork's own notes. The save-sync and account work builds and links. None of it has been run against a game yet.

- A "Connect Nintendo WFC to OpenPak" checkbox in the Wi-Fi settings. melonDS answers DNS itself in indirect mode, and with the option on, names on the redirect list resolve to the OpenPak server.
- A launch-time fetch of the OpenPak network profile for the redirect list and address, validated against a compiled-in allowlist. If the fetch fails it uses the last stored profile, then a built-in list. A "Refresh network settings" button re-runs it, and each launch logs which source was used.
- The shared OpenPak menu and window: website sign-in and sign-out, account page, read-only friends list, cloud saves and status.
- Cloud saves for each ROM's `.sav`, keyed by the cartridge game code. The newest cloud copy comes down when a ROM loads (the replaced local file is kept as a backup), the save goes up on eject or exit, and a conflict flow lets you choose.

Not verified: a WFC match on OpenPak with any DS title, any save sync against a real cartridge image, and any hardware behaviour.

## Connecting to OpenPak

Create an account at https://openpak.org. In the emulator, use the OpenPak menu (left of Help) to sign in, then turn on the WFC connection in the Wi-Fi settings or in Config, OpenPak settings. A first interactive launch asks once whether to connect.

WFC has no console accounts, so the OpenPak sign-in is a website sign-in that names the player and authorises save uploads.

You must supply your own ROMs and, for firmware boot, a firmware and BIOS dump from a DS you own. OpenPak provides none and never will.

## Downloads

No public build yet. Release tags exist and the repository has a release workflow, but no release has been published and none is advertised while this fork is transitional. Check https://github.com/openpak/melonds/releases for any change.

## Building from source

Build instructions are upstream's, in `BUILD.md` in this repository (Linux, Windows and macOS).

OpenPak-specific notes:

- Clone with submodules (`git clone --recursive`). The fork adds a submodule, `Externals/openpak-client`, which is currently a private repository. Without access to it the OpenPak code does not build, so outside contributors cannot yet build this fork as it stands.
- Release builds run from `v*.*.*` tags through `.github/workflows/openpak_release.yml`.

## Known limitations

- Not yet run against a game. The first WFC match on OpenPak is the next gate.
- Friends are read-only in the OpenPak window. Invitations, Mods and News show placeholder panels.
- Cloud saves cover DS `.sav` files only.
- The OpenPak window learns of a ROM's save only after that ROM has been loaded once.

## Contributing and support

Issues: https://github.com/openpak/melonds/issues. For anything that is not specific to OpenPak, upstream melonDS is the better place. See `CONTRIBUTING.md` for the upstream guidelines.

Security problems: report them privately to the OpenPak maintainers through the contact on https://openpak.org. Please do not open a public issue.

The most useful help right now is a first WFC test with a DS title against OpenPak, with the log attached.

## License and acknowledgements

melonDS is licensed under GPL-3.0-or-later; the full text is in `LICENSE`. This fork keeps that licence. We credit the melonDS team and the contributors credited in upstream's README, including Martin (GBAtek), Cydrak (3D GPU research) and limittox (icon).

The original upstream README is preserved as [README.upstream.md](README.upstream.md).
