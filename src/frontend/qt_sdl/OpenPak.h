/*
    Copyright 2026 OpenPak

    This file is part of the OpenPak additions to melonDS. melonDS itself is
    copyright the melonDS team and distributed under its own licence; this
    file is new code and carries this header alone.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this file, to deal in the file without restriction, including
    without limitation the rights to use, copy, modify, merge, publish,
    distribute, sublicense, and/or sell copies of the file, subject to the
    following conditions: this notice stays in every copy of the file.

    THE FILE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.

    OpenPak for melonDS: the OpenPak menu, the shared account window (openpak-client's Qt
    library, DS family), the sign-in / connect / sign-out dialogs, Config -> OpenPak settings...
    and cloud saves, as emulators/prds/openpak-ux-spec.md has them for every emulator.

    WFC minted no accounts, so the emulator's OpenPak account is a website sign-in: it names the
    player and carries the bearer token cloud saves upload with. A DS save is one .sav file per
    ROM; it maps onto the saves service as one versioned blob keyed by the ROM's game code.
*/
#pragma once

#include <string>

class QMainWindow;
class QMenuBar;
class QWidget;

namespace OpenPak
{
// One-time set-up after Config::Load(): where the client library keeps its files, which
// emulator this is, the website, the saves platform ("ds"), and the network profile, fetched
// off the UI thread and applied on it.
void Init();

// Every main window: the OpenPak menu, immediately left of Help. The first window also hosts the
// toasts and the account window.
void AddMenu(QMainWindow* window, QMenuBar* menu_bar);

// After the first window shows, on a plain interactive launch (no ROM given): the connect
// prompt, once per install.
void MaybeAskToConnect();

// Config -> OpenPak settings... (UX spec §3.13).
void ShowSettings(QWidget* parent);

// The OpenPak server the slirp resolver answers with: none when the connection is off, else the
// network profile's address, else LAN.OpenPakServer.
void ApplyServer();

// Called from EmuInstance::loadROM while the ROM is being loaded, before the .sav is read (on
// the emulator thread): remembers which save belongs to the loaded game, and brings down the
// newest cloud copy when the local one is not already in step -- at most five seconds, with a
// Skip, never blocking the load for longer. filedata is the ROM image; the game code at 0x0C is
// the save's cloud key.
void OnRomLoading(const unsigned char* filedata, unsigned int filelen,
                  const std::string& savname);

// Called from EmuInstance::ejectCart after the save manager flushed.
void OnRomClosed();

// Pushes a still-pending save, if any. Called at exit, after the emu instances are gone and the
// .sav files are final. wait: run inline rather than on a detached thread, because the process
// is on its way out.
void PushPending(bool wait);

bool CloudSyncEnabled();
}  // namespace OpenPak
