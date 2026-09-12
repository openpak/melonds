/*
    Copyright 2026 OpenPak

    OpenPak account and cloud saves for melonDS. See OpenPak.h.
*/
#include "OpenPak.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <fstream>
#include <span>
#include <thread>
#include <vector>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "Config.h"
#include "Platform.h"

#include <openpak/account.h>
#include <openpak/api.h>
#include <openpak/platform.h>

using namespace melonDS;

namespace OpenPak
{
namespace
{
// The ROM whose save is loaded: its cloud key (lowercase game code) and the
// .sav it maps to. Empty when no ROM is loaded or the save was pushed.
std::string g_title_key;
std::string g_sav_path;

std::filesystem::path LocalSavePath() { return std::filesystem::path(g_sav_path); }

bool LocalSaveHasContent()
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(LocalSavePath(), ec))
        return false;
    return std::filesystem::file_size(LocalSavePath(), ec) > 0 && !ec;
}

void PushNow(bool wait)
{
    if (g_title_key.empty() || g_sav_path.empty())
        return;
    if (!Common::OpenPakAccount::HasBearer() || !CloudSyncEnabled())
        return;
    if (!LocalSaveHasContent())
        return;

    auto run = [key = g_title_key, path = g_sav_path] {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return;
        std::vector<char> bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
        if (bytes.empty())
            return;
        const std::string error = WebService::OpenPakApi::PushSave(key,
            reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
        if (!error.empty())
            Platform::Log(Platform::LogLevel::Warn, "OpenPak: save push for %s failed: %s\n",
                          key.c_str(), error.c_str());
        else
            Platform::Log(Platform::LogLevel::Info, "OpenPak: save pushed for %s (%zu bytes)\n",
                          key.c_str(), bytes.size());
    };

    if (wait)
        run();
    else
        std::thread(run).detach();

    g_title_key.clear();
    g_sav_path.clear();
}
}  // namespace

bool CloudSyncEnabled()
{
    return Config::GetGlobalTable().GetBool("OpenPak.CloudSync");
}

void Init()
{
    std::filesystem::path config_dir =
        std::filesystem::path(Platform::GetLocalFilePath("melonDS.toml")).parent_path();
    openpak::Platform::SetDirectories(config_dir, config_dir / "openpak_cache");
    WebService::OpenPakApi::SetSavesPlatform("ds");
}

void OnRomLoading(const unsigned char* filedata, unsigned int filelen,
                  const std::string& savname)
{
    // Whatever was loaded before is closed by the loader; its save is final.
    PushNow(false);

    g_sav_path = savname;
    g_title_key.clear();

    // The DS cartridge header carries a four-letter game code at 0x0C; it is
    // what a player's own .sav is named after, so it is what the cloud copy is
    // keyed by too. Lowercase, and only for codes that are actually letters.
    if (filelen < 0x10)
        return;
    char code[5] = {0};
    for (int i = 0; i < 4; i++)
    {
        const unsigned char c = filedata[0x0C + i];
        if (c < 'A' || c > 'z' || (c > 'Z' && c < 'a'))
            return;
        code[i] = static_cast<char>(c - ('A' - 'a'));
    }
    g_title_key = code;

    if (!CloudSyncEnabled() || !Common::OpenPakAccount::HasBearer())
        return;
    if (LocalSaveHasContent())
        return;  // never overwrite a local save from the automatic path

    auto bytes = WebService::OpenPakApi::PullSave(g_title_key);
    if (!bytes || bytes->empty())
        return;
    std::ofstream f(g_sav_path, std::ios::binary | std::ios::trunc);
    if (!f)
        return;
    f.write(reinterpret_cast<const char*>(bytes->data()),
            static_cast<std::streamsize>(bytes->size()));
    Platform::Log(Platform::LogLevel::Info,
                  "OpenPak: cloud save applied to %s (%zu bytes)\n", g_sav_path.c_str(),
                  bytes->size());
}

void OnRomClosed()
{
    PushNow(false);
}

void PushPending(bool wait)
{
    PushNow(wait);
}

QWidget* CreateWifiDialogSection(QWidget* parent)
{
    auto* box = new QGroupBox(QObject::tr("OpenPak account"), parent);
    auto* layout = new QHBoxLayout(box);

    auto* status = new QLabel(box);
    auto* button = new QPushButton(box);
    std::function<void()> refresh = [status, button] {
        if (Common::OpenPakAccount::HasBearer())
        {
            status->setText(QObject::tr("Signed in as <b>%1</b>. Saves sync to your account.")
                                .arg(QString::fromStdString(
                                    Common::OpenPakAccount::GetUsername())));
            button->setText(QObject::tr("Sign out"));
        }
        else
        {
            status->setText(QObject::tr("Not signed in. Sign in to sync .sav files."));
            button->setText(QObject::tr("Sign in"));
        }
    };
    refresh();

    // The dialog deletes itself on close; the row refreshes with it.
    QObject::connect(button, &QPushButton::clicked, box, [box, parent, refresh] {
        auto* dialog = new OpenPakSignInDialog(parent);
        QObject::connect(dialog, &QObject::destroyed, box, refresh);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });

    layout->addWidget(status, 1);
    layout->addWidget(button, 0);
    return box;
}
}  // namespace OpenPak

OpenPakSignInDialog::OpenPakSignInDialog(QWidget* parent) : QWidget(parent, Qt::Dialog)
{
    setWindowTitle(QObject::tr("Sign in to OpenPak"));
    CreateMainLayout();
    RefreshAccountState();
}

void OpenPakSignInDialog::CreateMainLayout()
{
    auto* layout = new QGridLayout(this);
    m_account_status = new QLabel(this);
    m_email_edit = new QLineEdit(this);
    m_email_edit->setPlaceholderText(QObject::tr("you@example.com"));
    m_password_edit = new QLineEdit(this);
    m_password_edit->setPlaceholderText(QObject::tr("Password"));
    m_password_edit->setEchoMode(QLineEdit::Password);
    m_status_label = new QLabel(this);
    m_sign_in_button = new QPushButton(QObject::tr("Sign in"), this);
    m_sign_out_button = new QPushButton(QObject::tr("Sign out"), this);
    m_sign_in_button->setDefault(true);

    layout->addWidget(m_account_status, 0, 0, 1, 2);
    layout->addWidget(new QLabel(QObject::tr("Email"), this), 1, 0);
    layout->addWidget(m_email_edit, 1, 1);
    layout->addWidget(new QLabel(QObject::tr("Password"), this), 2, 0);
    layout->addWidget(m_password_edit, 2, 1);
    layout->addWidget(m_status_label, 3, 0, 1, 2);
    layout->addWidget(m_sign_in_button, 4, 0);
    layout->addWidget(m_sign_out_button, 4, 1);

    connect(m_sign_in_button, &QPushButton::clicked, this,
            &OpenPakSignInDialog::OnSignInButtonClicked);
    connect(m_sign_out_button, &QPushButton::clicked, this,
            &OpenPakSignInDialog::OnSignOutButtonClicked);
}

void OpenPakSignInDialog::OnSignInButtonClicked()
{
    m_status_label->setText(QObject::tr("Signing in…"));
    auto result = WebService::OpenPakApi::SignInAccountOnly(m_email_edit->text().toStdString(),
                                                           m_password_edit->text().toStdString());
    if (!result.ok)
    {
        m_status_label->setText(QString::fromStdString(result.error));
        return;
    }
    Common::OpenPakAccount::SaveBearerOnly(m_email_edit->text().toStdString(), result.bearer);
    m_password_edit->clear();
    RefreshAccountState();
}

void OpenPakSignInDialog::OnSignOutButtonClicked()
{
    Common::OpenPakAccount::Clear();
    RefreshAccountState();
}

void OpenPakSignInDialog::RefreshAccountState()
{
    const bool has_bearer = Common::OpenPakAccount::HasBearer();
    m_email_edit->setEnabled(!has_bearer);
    m_password_edit->setEnabled(!has_bearer);
    m_sign_in_button->setEnabled(!has_bearer);
    m_sign_out_button->setEnabled(has_bearer);
    if (has_bearer)
    {
        m_account_status->setText(QObject::tr("Signed in as %1.").arg(
            QString::fromStdString(Common::OpenPakAccount::GetUsername())));
        m_status_label->setText(
            QObject::tr("With “Connect Nintendo WFC to OpenPak” on and cloud sync enabled, "
                        "each game's .sav syncs under this account."));
    }
    else
    {
        m_account_status->setText(QObject::tr("Not signed in."));
        m_status_label->clear();
    }
}
