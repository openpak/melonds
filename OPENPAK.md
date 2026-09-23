# melonDS for OpenPak

Fork of upstream melonDS with three additions: a way to point the emulated console at OpenPak,
cloud saves driven by the emulator, and the OpenPak menu and window every OpenPak emulator
shares (`emulators/prds/openpak-ux-spec.md`, DS family). Everything else is upstream, merged as it moves.
Builds: `vX.Y.Z` tags (`v*.*.*`) publish a GitHub Release (`.github/workflows/openpak_release.yml`).
The save-sync and account work builds and links; not yet run against a game.

## WFC redirect (Config → Wi-Fi settings)

A **Connect Nintendo WFC to OpenPak** checkbox in the Wi-Fi settings, mirrored in Config →
OpenPak settings... (same key, `LAN.OpenPak`). melonDS answers DNS
itself in indirect mode; with the option on, every name on the applied redirect list resolves
to the OpenPak server (`LAN.OpenPakServer` in melonDS.toml). The DS talks HTTP/HTTPS to NAS;
the server side handles the login certificate.

The redirect list and the address are **not compiled in any more** (EP-4,
`emulators/prds/emulator-network-profile-prd.md`). At launch melonDS fetches the network profile
(`openpak-client`, one conditional request, two seconds); what applies is:

1. the fetched profile (`src/frontend/qt_sdl/OpenPak.cpp` applies it to `Net_Slirp`),
   validated against the compiled-in allowlist — the ceiling —
2. else the last-known-good stored profile (`config/openpak_network_profile.json`),
3. else the compiled-in list and `LAN.OpenPakServer`, which are the fallback, not the truth.

**Refresh network settings** (Config → OpenPak settings... → Advanced) re-runs that off the UI
thread, without a restart. The launch fetch runs off the UI thread too. Every launch logs which source was used — fetched, cached or built-in —
and the profile version.

## The OpenPak menu, window and settings (UX spec)

- **OpenPak menu** in every main window, immediately left of Help (`openpak::qt::AddOpenPakMenu`):
  *Sign in to OpenPak...* / *Signed in as {name}*, Friends, Invitations, Cloud saves, Mods, News,
  Status, *OpenPak settings...*, *OpenPak website*, *Sign out...*. Sign in and sign out wait for
  every instance to stop its game; with the WFC connection off the header opens the settings.
- **The OpenPak window** is the library's (`OpenPakAccountDialog`, `Family::DS`): Account (no
  console identity), Friends read-only with a link to manage them on openpak.org, Invitations,
  Mods and News show their not-here panels, Cloud saves (`ds` saves only; Download, Upload,
  Delete, *Resolve...* for a conflict), Status.
- **Sign-in** is the library dialog with a Device name (`OpenPak.DeviceName`, it names the
  machine on uploaded save versions), off the UI thread, errors inline. **Sign out** is confirmed
  and revokes the token. A first interactive launch without a ROM asks *Connect to OpenPak?* once
  (`OpenPak.ConnectAsked`); signing in from it turns on the WFC connection and cloud sync.
- **Config → OpenPak settings...**: the WFC switch, the account row, *Open OpenPak...*, *Sync cloud
  saves automatically...* (`OpenPak.CloudSync`, default on), *Show notifications* and
  *Notification corner* (`OpenPak.Notifications`, `OpenPak.NotificationCorner`), and a collapsed
  Advanced with the Website (`OpenPak.Website`, read at launch) and *Refresh network settings*.
- **Toasts** (library toast, 6 s): signed in / out, an expired stored sign-in, cloud save pulled,
  pushed, push failed and conflict, and friends coming online or asking.

## Cloud saves

WFC has no accounts, so melonDS's OpenPak account is a website sign-in and nothing more
(`SignInAccountOnly`): it names the player and carries the bearer token saves upload with.

The save is the ROM's `.sav`, uploaded under platform `ds` with the lower-case game code from
the cartridge header (offset 0x0C) as key; for the library's pages the code is packed into a
64-bit title id (`Host::CloudSaveKey`/`CloudSaveTitle`). A `<sav>.openpak-version` marker beside
it records the cloud version the file was last in step with, as SaveSync does for a folder.
When a ROM loads (before melonDS reads the save), the newest cloud copy comes down if the local
file is not already in step (the old one is kept as `.sav.openpak-backup`) -- at most five
seconds, with a "Checking cloud save..." dialog and Skip; when a ROM is ejected or the emulator
exits, the `.sav` goes up. A local and a cloud save that never met are a conflict: the game
starts on the local file, a toast says so, automatic sync for it pauses, and the Cloud saves
page's *Resolve...* chooses. ROMs loaded once are remembered (`openpak_ds_roms.txt` in the
config dir) so the Cloud saves page knows their `.sav`.

Server side: the OpenPak network (`account`, `nn-account`, `nn-friends`, `nn-nncs`, `nn-boss`,
`nn-juxtaposition`, `nn-soap` for Wii U and 3DS; `nn-wfc` for Wii and DS) answers both the
Nintendo names and the `openpak.org` names behind one TLS front.
