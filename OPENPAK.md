# melonDS for OpenPak

Fork of upstream melonDS with two additions: a way to point the emulated console at OpenPak,
and cloud saves driven by the emulator. Everything else is upstream, merged as it moves.
Builds: `openpak-v*` tags publish a GitHub Release (`.github/workflows/openpak_release.yml`).
The save-sync and account work builds and links; not yet run against a game.

## WFC redirect (Config → Wi-Fi settings)

A **Connect Nintendo WFC to OpenPak** checkbox in the Wi-Fi settings. melonDS answers DNS
itself in indirect mode; with the option on, every name on the applied redirect list resolves
to the OpenPak server (`LAN.OpenPakServer` in melonDS.toml). The DS talks HTTP/HTTPS to NAS;
the server side handles the login certificate.

The redirect list and the address are **not compiled in any more** (EP-4,
`prds/emulator-network-profile-prd.md`). At launch melonDS fetches the network profile
(`openpak-client`, one conditional request, two seconds); what applies is:

1. the fetched profile (`src/frontend/qt_sdl/OpenPak.cpp` applies it to `Net_Slirp`),
   validated against the compiled-in allowlist — the ceiling —
2. else the last-known-good stored profile (`config/openpak_network_profile.json`),
3. else the compiled-in list and `LAN.OpenPakServer`, which are the fallback, not the truth.

**Refresh network settings** in the OpenPak section of the Wi-Fi settings dialog re-runs that
without a restart. Every launch logs which source was used — fetched, cached or built-in —
and the profile version.

## OpenPak account and cloud saves

WFC has no accounts, so melonDS's OpenPak account is a website sign-in and nothing more: it
exists to put a name on cloud saves and to carry the bearer token they upload with
(`SignInAccountOnly`; no console identity is minted, because WFC minted none). Sign in from
the OpenPak section of the Wi-Fi settings dialog.

Cloud sync is per ROM: the save is the ROM's `.sav` file, uploaded under platform `ds` and the
game code from the cartridge header (offset 0x0C) as key — the same file name a player's own
save uses. When a ROM loads, the cloud copy is applied first if the local `.sav` is missing or
empty (the load path pulls before melonDS reads the save, so a fresh machine boots with its
cloud save); when a ROM is ejected or the emulator exits, the `.sav` is pushed. A local save
is never overwritten by the automatic path — conflicts resolve on openpak.org, which keeps
every version. Server side: the same `/api/v1/me/saves/{platform}/{title}` the website and
phone app use (`prds/cloud-saves-prd.md` CS-06, platform ids S-1).

The **Sync .sav files** config key is `OpenPak.CloudSync` in melonDS.toml.

Server side: the OpenPak network (`account`, `nn-account`, `nn-friends`, `nn-nncs`, `nn-boss`,
`nn-juxtaposition`, `nn-soap` for Wii U and 3DS; `nn-wfc` for Wii and DS) answers both the
Nintendo names and the `openpak.org` names behind one TLS front.
