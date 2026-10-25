# Protocol: melonds

## Account

WFC has no console accounts, so the OpenPak sign-in is a website sign-in that names the player and authorises save uploads.

- The shared OpenPak menu and window: website sign-in and sign-out, account page, read-only friends list, cloud saves and status.

## Redirect and network profile

- A "Connect Nintendo WFC to OpenPak" checkbox in the Wi-Fi settings. melonDS answers DNS itself in indirect mode, and with the option on, names on the redirect list resolve to the OpenPak server.
- A launch-time fetch of the OpenPak network profile for the redirect list and address, validated against a compiled-in allowlist. If the fetch fails it uses the last stored profile, then a built-in list. A "Refresh network settings" button re-runs it, and each launch logs which source was used.

## Cloud saves

- Cloud saves for each ROM's `.sav`, keyed by the cartridge game code. The newest cloud copy comes down when a ROM loads (the replaced local file is kept as a backup), the save goes up on eject or exit, and a conflict flow lets you choose.
