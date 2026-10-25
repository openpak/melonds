# melonDS Emulator

OpenPak's melonDS Emulator.

| | |
| --- | --- |
| Console | DS |
| Kind | Emulator |
| Based on | [melonDS](https://github.com/melonDS-emu/melonDS) (GPL-3.0-or-later as upstream) |
| Licence | As upstream, see [LICENSE](LICENSE) |

## What it does

- A "Connect Nintendo WFC to OpenPak" option in the Wi-Fi settings. With it on, the redirect list resolves to the OpenPak server.
- An OpenPak network profile fetched at launch, validated against a compiled-in allowlist.
- Cloud saves for each ROM's `.sav`, keyed by the cartridge game code.
- The OpenPak menu and window that every OpenPak emulator shares.
- Everything else is upstream melonDS.

## Build

Build instructions are upstream's, in [BUILD.md](BUILD.md) (Linux, Windows and macOS). Clone with submodules:

```sh
git clone --recursive https://github.com/openpak/melonds
```

OpenPak-specific notes are in [deploy](docs/deploy.md).

## Docs

- [Protocol](docs/protocol.md)
- [Evidence](docs/evidence.md)
- [Deploy](docs/deploy.md)
- [Notes](docs/notes.md)
- [Upstream README](docs/upstream.md)

## Licence

See [LICENSE](LICENSE).
