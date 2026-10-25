# Notes: melonds

## Scope

melonDS for OpenPak is OpenPak's Nintendo DS emulator fork, built on [melonDS](https://github.com/melonDS-emu/melonDS). It is an independent project. Upstream melonDS remains our base, and we merge upstream changes as they land. This is not an official melonDS release.

The fork adds a Nintendo WFC redirect to OpenPak, cloud saves driven by the emulator, and the OpenPak sign-in, menu and settings that every OpenPak emulator shares. Everything else is upstream melonDS.

## Status

**Status:** Transitional (kept for now, successor planned, no builds advertised).

## Not affiliated

This is an independent project and is not endorsed by the melonDS project. It is not affiliated with Nintendo, and all trademarks belong to their owners.

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

The original upstream README is preserved as [README.upstream.md](../README.upstream.md).
