# Deploy: melonds

## Building from source

Build instructions are upstream's, in `BUILD.md` in this repository (Linux, Windows and macOS).

OpenPak-specific notes:

- Clone with submodules (`git clone --recursive`). The fork adds a submodule, `Externals/openpak-client`, which is currently a private repository. Without access to it the OpenPak code does not build, so outside contributors cannot yet build this fork as it stands.
- Release builds run from `v*.*.*` tags through `.github/workflows/openpak_release.yml`.

## Connecting to OpenPak

Create an account at https://openpak.org. In the emulator, use the OpenPak menu (left of Help) to sign in, then turn on the WFC connection in the Wi-Fi settings or in Config, OpenPak settings. A first interactive launch asks once whether to connect.

You must supply your own ROMs and, for firmware boot, a firmware and BIOS dump from a DS you own. OpenPak provides none and never will.

## Downloads

No public build yet. Release tags exist and the repository has a release workflow, but no release has been published and none is advertised while this fork is transitional. Check https://github.com/openpak/melonds/releases for any change.
