# melonDS for OpenPak

Fork of upstream melonDS with one addition: a way to point the emulated console at OpenPak.
Everything else is upstream, merged as it moves. Builds: `openpak-v*` tags publish a GitHub
Release (`.github/workflows/openpak_release.yml`). Not yet run against a game.

A **Connect Nintendo WFC to OpenPak** checkbox in the Wi-Fi settings. melonDS answers DNS
itself in indirect mode; with the option on, every `*.nintendowifi.net`, `*.gamespy.com` and
`*.openpak.org` name resolves to the OpenPak server (`LAN.OpenPakServer` in melonDS.toml,
default the production box). The DS talks HTTP/HTTPS to NAS; the server side handles the
login certificate.

Server side: the OpenPak network (`account`, `nn-account`, `nn-friends`, `nn-nncs`, `nn-boss`,
`nn-juxtaposition`, `nn-soap` for Wii U and 3DS; `nn-wfc` for Wii and DS) answers both the
Nintendo names and the `openpak.org` names behind one TLS front.
