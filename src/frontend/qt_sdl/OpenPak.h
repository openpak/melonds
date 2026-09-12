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

    OpenPak account and cloud saves for melonDS (WD-1, prds/platform-wii-ds-prd.md).
    WFC minted no accounts, so the emulator's OpenPak account is a website sign-in
    that exists to carry the bearer token cloud saves upload with. A DS save is one
    .sav file per ROM; it maps onto the saves service as one versioned blob keyed
    by the ROM's game code.
*/
#pragma once

#include <string>

#include <QWidget>

namespace OpenPak
{
// One-time set-up: point the client library at melonDS's config dir and name
// this host's saves platform ("ds"). Call once after Config::Load().
void Init();

// The account row for the Wi-Fi settings dialog: who is signed in, sign in /
// sign out. Parented to the dialog; owned by Qt.
QWidget* CreateWifiDialogSection(QWidget* parent);

// Called from EmuInstance::loadROM while the ROM is being loaded, before the
// .sav is read: remembers which save belongs to the loaded game, and pulls the
// cloud save into the (missing or empty) local .sav so the game boots with it.
// filedata is the ROM image; the game code at 0x0C is the save's cloud key.
void OnRomLoading(const unsigned char* filedata, unsigned int filelen,
                  const std::string& savname);

// Called from EmuInstance::ejectCart after the save manager flushed.
void OnRomClosed();

// Pushes a still-pending save, if any. Called at exit, after the emu instances
// are gone and the .sav files are final. wait: run inline rather than on a
// detached thread, because the process is on its way out.
void PushPending(bool wait);

bool CloudSyncEnabled();
}  // namespace OpenPak

class OpenPakSignInDialog : public QWidget
{
  Q_OBJECT
public:
  explicit OpenPakSignInDialog(QWidget* parent = nullptr);

private:
  void CreateMainLayout();
  void OnSignInButtonClicked();
  void OnSignOutButtonClicked();
  void RefreshAccountState();

  class QLabel* m_account_status;
  class QLineEdit* m_email_edit;
  class QLineEdit* m_password_edit;
  class QLabel* m_status_label;
  class QPushButton* m_sign_in_button;
  class QPushButton* m_sign_out_button;
};
