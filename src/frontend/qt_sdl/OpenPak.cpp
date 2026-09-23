/*
    Copyright 2026 OpenPak

    OpenPak for melonDS. See OpenPak.h.
*/
#include "OpenPak.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "Config.h"
#include "Platform.h"
#include "Net_Slirp.h"
#include "version.h"

#include <openpak/account.h>
#include <openpak/api.h>
#include <openpak/network_profile.h>
#include <openpak/platform.h>
#include <openpak/qt/account_dialog.h>
#include <openpak/qt/avatar_cache.h>
#include <openpak/qt/friend_notifier.h>
#include <openpak/qt/host.h>
#include <openpak/qt/host_kit.h>
#include <openpak/qt/prompts.h>
#include <openpak/qt/sign_in_dialog.h>
#include <openpak/qt/toast.h>

using namespace melonDS;

// main.cpp: whether any emulator instance is running a game.
bool OpenPakGameRunning();

namespace OpenPak
{
namespace
{
namespace Api = WebService::OpenPakApi;
using Kind = NextendoToast::Kind;
using Nextendo::SaveSync::LocalCopy;
using Nextendo::SaveSync::LocalState;

QString Tr(const char* text)
{
    return QCoreApplication::translate("OpenPak", text);
}

bool SignedIn()
{
    return Common::OpenPakAccount::HasBearer();
}

Config::Table Global()
{
    return Config::GetGlobalTable();
}

bool OpenPakOn()
{
    return Global().GetBool("LAN.OpenPak");
}

// A DS game code ("ADAE") as the library's 64-bit title: its four letters, big-endian, so the
// sixteen hex digits read back as the code. The cloud key is the code in lower case, as it
// always was.
u64 TitleOfCode(const std::string& code)
{
    if (code.size() != 4)
        return 0;
    u64 id = 0;
    for (char c : code)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)))
            return 0;
        id = (id << 8) | static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return id;
}

std::string CodeOfTitle(u64 id)
{
    std::string code(4, ' ');
    for (int i = 0; i < 4; i++)
        code[3 - i] = static_cast<char>((id >> (8 * i)) & 0xFF);
    return code;
}

std::string KeyOfTitle(u64 id)
{
    std::string key = CodeOfTitle(id);
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return key;
}

// ---- the ROMs this machine has loaded: game code -> .sav path and a name, kept on disk so the
// Cloud saves page knows them after a restart. Written on the emulator thread, read on the UI.
struct KnownRom
{
    std::string sav;
    std::string name;
};
std::mutex g_roms_mutex;
std::map<u64, KnownRom> g_roms;
bool g_roms_loaded = false;

std::filesystem::path RomsFile()
{
    return openpak::Platform::ConfigDir() / "openpak_ds_roms.txt";
}

void LoadRomsLocked()
{
    if (g_roms_loaded)
        return;
    g_roms_loaded = true;
    std::ifstream in(RomsFile());
    std::string line;
    while (std::getline(in, line))
    {
        std::istringstream fields(line);
        std::string code, sav, name;
        if (std::getline(fields, code, '\t') && std::getline(fields, sav, '\t'))
        {
            std::getline(fields, name);
            if (const u64 id = TitleOfCode(code))
                g_roms[id] = {sav, name};
        }
    }
}

void Remember(u64 id, const std::string& sav, const std::string& name)
{
    std::lock_guard lock(g_roms_mutex);
    LoadRomsLocked();
    auto& known = g_roms[id];
    if (known.sav == sav && known.name == name)
        return;
    known = {sav, name};
    std::error_code ec;
    std::filesystem::create_directories(RomsFile().parent_path(), ec);
    std::ofstream out(RomsFile(), std::ios::trunc);
    for (const auto& [rom, entry] : g_roms)
        out << CodeOfTitle(rom) << '\t' << entry.sav << '\t' << entry.name << '\n';
}

std::optional<KnownRom> Known(u64 id)
{
    std::lock_guard lock(g_roms_mutex);
    LoadRomsLocked();
    const auto it = g_roms.find(id);
    if (it == g_roms.end())
        return std::nullopt;
    return it->second;
}

// ---- a .sav against the cloud, as SaveSync does a save folder: a marker beside it records the
// cloud version the local copy was last in step with.
std::filesystem::path MarkerOf(const std::filesystem::path& sav)
{
    return std::filesystem::path(sav.string() + ".openpak-version");
}

std::string ReadMarker(const std::filesystem::path& sav)
{
    std::ifstream in(MarkerOf(sav));
    std::string version;
    std::getline(in, version);
    return version;
}

void WriteMarker(const std::filesystem::path& sav, const std::string& version)
{
    if (version.empty())
        return;
    std::ofstream(MarkerOf(sav), std::ios::trunc) << version << '\n';
}

std::vector<u8> ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<u8>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

bool HasLocal(const std::filesystem::path& sav)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(sav, ec) && std::filesystem::file_size(sav, ec) > 0 &&
           !ec;
}

// The cloud copy in place of the local one, which is kept beside it.
bool ApplyCloud(const std::filesystem::path& sav, const std::vector<u8>& bytes,
                const std::string& version)
{
    std::error_code ec;
    if (HasLocal(sav))
        std::filesystem::copy_file(sav, sav.string() + ".openpak-backup",
                                   std::filesystem::copy_options::overwrite_existing, ec);
    const std::filesystem::path staged = sav.string() + ".openpak-new";
    {
        std::ofstream out(staged, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        if (!out)
            return false;
    }
    std::filesystem::rename(staged, sav, ec);
    if (ec)
        return false;
    WriteMarker(sav, version);
    return true;
}

s64 LastWritten(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto written = std::filesystem::last_write_time(path, ec);
    if (ec)
        return 0;
    const auto age = std::filesystem::file_time_type::clock::now() - written;
    return std::chrono::duration_cast<std::chrono::seconds>(
               (std::chrono::system_clock::now() -
                std::chrono::duration_cast<std::chrono::system_clock::duration>(age))
                   .time_since_epoch())
        .count();
}

LocalCopy CompareSav(const std::filesystem::path& sav, int newest_cloud)
{
    LocalCopy out;
    if (sav.empty() || !HasLocal(sav))
        return out;
    out.last_written = LastWritten(sav);
    out.version = ReadMarker(sav);
    if (out.version.empty())
    {
        out.state = newest_cloud > 0 ? LocalState::NoHistory : LocalState::ChangedHere;
    }
    else if (out.version != std::to_string(newest_cloud))
    {
        out.state = LocalState::CloudNewer;
    }
    else
    {
        std::error_code ec;
        const auto synced = std::filesystem::last_write_time(MarkerOf(sav), ec);
        out.state = !ec && std::filesystem::last_write_time(sav, ec) > synced ?
                        LocalState::ChangedHere :
                        LocalState::InStep;
    }
    return out;
}

std::string PushSav(const std::string& key, const std::filesystem::path& sav,
                    const std::string& base_version)
{
    const std::vector<u8> bytes = ReadFile(sav);
    if (bytes.empty())
        return Tr("There is no local save to upload.").toStdString();
    Api::SetSaveVersion(key, base_version);
    std::string error = Api::PushSave(key, bytes.data(), bytes.size());
    if (error.empty())
        WriteMarker(sav, Api::SaveVersion(key));
    return error;
}

// ---- the Qt library's view of melonDS: a DS host with one identity and no console account.
class MelonHost final : public openpak::qt::Host
{
public:
    using Host::Host;

    QMainWindow* main_window = nullptr;

    openpak::qt::Family GetFamily() const override { return openpak::qt::Family::DS; }
    bool IsLinked() const override { return SignedIn(); }
    void SignIn() override;
    void SignOut() override;
    void RefreshFriendCache() override {}
    void NotifyFriendRequestSent(const QString&) override {}
    QString JoinFriendSession(u64) override { return {}; }
    void EnsureChatConnected() override {}
    NextendoChatClient* GetChatClient() override { return nullptr; }

    static u64 IdOfAny(const std::string& text)
    {
        if (const u64 id = TitleOfCode(text))
            return id; // a cloud key
        u64 id = 0;
        for (char c : text)
        {
            if (!std::isxdigit(static_cast<unsigned char>(c)))
                return 0;
            id = (id << 4) | static_cast<u64>(std::isdigit(static_cast<unsigned char>(c)) ?
                                                  c - '0' :
                                                  std::tolower(static_cast<unsigned char>(c)) -
                                                      'a' + 10);
        }
        return id;
    }

    QString ResolveGameName(const std::string& app_id_hex,
                            const std::string& hint_name) const override
    {
        if (const auto known = Known(IdOfAny(app_id_hex)); known && !known->name.empty())
            return QString::fromStdString(known->name);
        return QString::fromStdString(hint_name);
    }
    QString ResolveGameIcon(const std::string&) const override { return {}; }
    std::string GetLocalAppId() const override;
    void QuickStart(u64) override {}
    void ManualSaveDownload(u64) override {}

    std::filesystem::path SaveDirectory(u64 title_id) override
    {
        const auto known = Known(title_id);
        return known ? std::filesystem::path(known->sav) : std::filesystem::path{};
    }
    std::vector<Title> InstalledTitles() const override
    {
        std::lock_guard lock(g_roms_mutex);
        LoadRomsLocked();
        std::vector<Title> titles;
        for (const auto& [id, known] : g_roms)
            titles.push_back({id, QString::fromStdString(known.name.empty() ? CodeOfTitle(id) :
                                                                              known.name)});
        return titles;
    }

    // One .sav per ROM, keyed by the lower-case game code (the Cloud saves page's hooks).
    std::string CloudSaveKey(u64 title_id) const override { return KeyOfTitle(title_id); }
    u64 CloudSaveTitle(const std::string& key) const override { return TitleOfCode(key); }
    LocalCopy CompareSave(const std::filesystem::path& sav, u64, int newest_cloud) override
    {
        return CompareSav(sav, newest_cloud);
    }
    std::string DownloadSave(const std::filesystem::path& sav, u64 title_id) override
    {
        if (sav.empty())
            return Tr("Load this game once first, so melonDS knows where its save goes.")
                .toStdString();
        const std::string key = KeyOfTitle(title_id);
        const auto bytes = Api::PullSave(key);
        if (!bytes || bytes->empty())
            return Tr("There is no cloud save for that game.").toStdString();
        if (!ApplyCloud(sav, *bytes, Api::SaveVersion(key)))
            return Tr("The cloud save could not be put in place; the local one is unchanged.")
                .toStdString();
        return {};
    }
    std::string UploadSave(const std::filesystem::path& sav, u64 title_id,
                           int newest_cloud) override
    {
        return PushSav(KeyOfTitle(title_id), sav,
                       newest_cloud > 0 ? std::to_string(newest_cloud) : std::string{});
    }

    QString AccentColor() const override
    {
        return qApp->palette().color(QPalette::Highlight).name();
    }
    bool IsDarkTheme() const override
    {
        return qApp->palette().color(QPalette::Window).lightness() < 128;
    }
    bool NotificationsEnabled() const override { return Global().GetBool("OpenPak.Notifications"); }
    void SetNotificationsEnabled(bool enabled) override
    {
        Global().SetBool("OpenPak.Notifications", enabled);
    }
    // The setting lists Bottom right, Bottom left, Top right, Top left; the toast counts
    // TopRight, TopLeft, BottomRight, BottomLeft.
    int NotificationCorner() const override
    {
        static constexpr int kToToast[] = {2, 3, 0, 1};
        return kToToast[std::clamp(Global().GetInt("OpenPak.NotificationCorner"), 0, 3)];
    }
    void SetNotificationCorner(int corner) override
    {
        static constexpr int kFromToast[] = {2, 3, 0, 1};
        Global().SetInt("OpenPak.NotificationCorner", kFromToast[std::clamp(corner, 0, 3)]);
    }
    bool RedirectEnabled() const override { return OpenPakOn(); }
    bool CloudSyncEnabled() const override { return OpenPak::CloudSyncEnabled(); }
    void SetCloudSyncEnabled(bool enabled) override { Global().SetBool("OpenPak.CloudSync", enabled); }
    std::string ServerIp() const override;
    std::string NatIp() const override { return {}; }
    void SetGuestInputSuspended(bool) override {}
    openpak::qt::Navigation* CreateNavigation(QObject*) override { return nullptr; }

    void AccountChanged(bool linked)
    {
        if (linked)
            emit AccountLinked();
        else
            emit AccountUnlinked();
    }
};

MelonHost* g_host = nullptr;
NextendoToast* g_toast = nullptr;
openpak::qt::FriendNotifier* g_friends = nullptr;
std::optional<QPixmap> g_avatar; // not a QPixmap: no QPixmap before QApplication
std::string g_profile_server; // the network profile's address, when it carries one

// The ROM whose save is loaded: its cloud key and the .sav it maps to. Empty when no ROM is
// loaded or the save was pushed. Emulator thread; the push hands them to a worker.
std::mutex g_run_mutex;
u64 g_run_title = 0;
std::string g_sav_path;
std::set<u64> g_paused; // titles whose automatic sync waits for a conflict to be resolved

std::string MelonHost::GetLocalAppId() const
{
    if (!OpenPakGameRunning())
        return {};
    std::lock_guard lock(g_run_mutex);
    if (g_run_title == 0)
        return {};
    char hex[17];
    std::snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(g_run_title));
    return hex;
}

std::string MelonHost::ServerIp() const
{
    return g_profile_server.empty() ? Global().GetString("LAN.OpenPakServer") : g_profile_server;
}

// Toasts belong to the UI thread; this may be called from any.
void Toast(const QString& text, Kind kind)
{
    QMetaObject::invokeMethod(qApp, [text, kind] {
        if (g_toast)
            g_toast->Show(text, {}, {}, kind);
    }, Qt::QueuedConnection);
}

QString NameOf(u64 id)
{
    const auto known = Known(id);
    return QString::fromStdString(known && !known->name.empty() ? known->name : CodeOfTitle(id));
}

void LoadAvatar()
{
    g_avatar.reset();
    if (!SignedIn())
        return;
    std::thread([] {
        auto profile = std::make_shared<Api::Profile>(Api::GetProfile());
        QMetaObject::invokeMethod(qApp, [profile] {
            if (!profile->ok || profile->image_base64.empty())
                return;
            const QPixmap source = Nextendo::AvatarCache::Get("self", profile->image_base64, 64);
            if (source.isNull())
                return;
            QPixmap round(20, 20);
            round.fill(Qt::transparent);
            QPainter painter(&round);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath clip;
            clip.addEllipse(0, 0, 20, 20);
            painter.setClipPath(clip);
            painter.drawPixmap(0, 0, 20, 20, source);
            g_avatar = round;
        }, Qt::QueuedConnection);
    }).detach();
}

// Sign in to OpenPak (UX spec §3.3): off the UI thread, errors inline. True when it signed in.
bool ShowSignIn(QWidget* parent)
{
    if (OpenPakGameRunning())
        return false;
    OpenPakSignInDialog dialog(parent);
    const std::string device = Global().GetString("OpenPak.DeviceName");
    dialog.SetDeviceName(device.empty() ? openpak::qt::DefaultDeviceName(QStringLiteral("melonDS")) :
                                          QString::fromStdString(device));
    dialog.SetSubmitter(&openpak::qt::SignInAccountOnly);
    if (dialog.exec() != QDialog::Accepted || !SignedIn())
        return false;
    Global().SetString("OpenPak.DeviceName", dialog.DeviceName().toStdString());
    Config::Save();
    Toast(Tr("Signed in as %1.").arg(QString::fromStdString(Common::OpenPakAccount::GetUsername())),
          Kind::Account);
    LoadAvatar();
    if (g_friends)
        g_friends->Reset();
    g_host->AccountChanged(true);
    return true;
}

void MelonHost::SignIn()
{
    ShowSignIn(main_window);
}

void MelonHost::SignOut()
{
    openpak::qt::SignOutAndRevoke();
    g_avatar.reset();
    Toast(Tr("Signed out of OpenPak."), Kind::Account);
    if (g_friends)
        g_friends->Reset();
    AccountChanged(false);
}

void OpenWindow(int page)
{
    OpenPakAccountDialog dialog(g_host, g_host->main_window, page);
    dialog.exec();
}

void CheckStoredSignIn()
{
    if (!SignedIn())
        return;
    std::thread([] {
        auto profile = std::make_shared<Api::Profile>(Api::GetProfile());
        QMetaObject::invokeMethod(qApp, [profile] {
            if (!profile->ok && !SignedIn())
            {
                Toast(Tr("The OpenPak sign-in for %1 has expired. Sign in again from the OpenPak "
                         "menu.")
                          .arg(QStringLiteral("melonDS")),
                      Kind::Account);
                g_host->AccountChanged(false);
                return;
            }
            if (profile->ok && !profile->name.empty() &&
                profile->name != Common::OpenPakAccount::GetUsername())
            {
                Common::OpenPakAccount::SaveBearerOnly(profile->name,
                                                       Common::OpenPakAccount::GetBearer());
            }
        }, Qt::QueuedConnection);
    }).detach();
    LoadAvatar();
}

void ApplyProfile(const openpak::NetworkProfile::Result& applied)
{
    if (applied.source == openpak::NetworkProfile::Source::BuiltIn)
        return;
    auto suffixes = applied.profile.suffixes;
    suffixes.insert(suffixes.end(), applied.profile.exact.begin(), applied.profile.exact.end());
    Net_Slirp::SetOpenPakSuffixes(suffixes);
    g_profile_server = applied.profile.server_address;
    ApplyServer();
}

QString SourceWord(openpak::NetworkProfile::Source source)
{
    switch (source)
    {
    case openpak::NetworkProfile::Source::Fetched: return Tr("fetched");
    case openpak::NetworkProfile::Source::Cached: return Tr("cached");
    case openpak::NetworkProfile::Source::BuiltIn: return Tr("built in");
    }
    return {};
}

QString NetworkStatusLine(long long version, openpak::NetworkProfile::Source source)
{
    const QString what = source == openpak::NetworkProfile::Source::BuiltIn ?
                             SourceWord(source) :
                             Tr("version %1, %2").arg(version).arg(SourceWord(source));
    return Tr("Network settings: %1.").arg(what);
}

void EnsureHost(QMainWindow* window)
{
    if (g_host)
        return;
    g_host = new MelonHost(qApp);
    g_host->main_window = window;
    openpak::qt::Host::SetCurrent(g_host);
    g_toast = new NextendoToast(window);
    QObject::connect(g_toast, &NextendoToast::clicked, g_host, [](NextendoToast::Kind kind) {
        switch (kind)
        {
        case Kind::Online:
        case Kind::Offline:
        case Kind::Request:
            if (SignedIn())
                OpenWindow(OpenPakAccountDialog::kFriendsPage);
            break;
        case Kind::Saves:
            if (SignedIn())
                OpenWindow(OpenPakAccountDialog::kCloudSavesPage);
            break;
        case Kind::Account:
            if (SignedIn())
                OpenWindow(OpenPakAccountDialog::kAccountPage);
            else
                ShowSignIn(g_host->main_window);
            break;
        default:
            break;
        }
    });
    g_friends = new openpak::qt::FriendNotifier(g_toast, g_host);
    CheckStoredSignIn();
}

// The push when a ROM closes: local I/O here, the network on a worker.
void PushNow(bool wait)
{
    u64 id = 0;
    std::string sav;
    {
        std::lock_guard lock(g_run_mutex);
        id = std::exchange(g_run_title, 0);
        sav = std::exchange(g_sav_path, {});
    }
    if (id == 0 || sav.empty() || !SignedIn() || !CloudSyncEnabled() || g_paused.count(id) != 0)
        return;
    if (!HasLocal(sav))
        return;

    auto run = [id, sav] {
        const std::string key = KeyOfTitle(id);
        const std::string error = PushSav(key, sav, ReadMarker(sav));
        if (error.empty())
        {
            Platform::Log(Platform::LogLevel::Info, "OpenPak: save pushed for %s\n", key.c_str());
            Toast(Tr("Save for %1 uploaded to OpenPak.").arg(NameOf(id)), Kind::Saves);
        }
        else
        {
            Platform::Log(Platform::LogLevel::Warn, "OpenPak: save push for %s failed: %s\n",
                          key.c_str(), error.c_str());
            Toast(Tr("The save for %1 did not upload: %2")
                      .arg(NameOf(id), QString::fromStdString(error)),
                  Kind::Saves);
        }
    };
    if (wait)
        run();
    else
        std::thread(run).detach();
}

enum class Pull { Nothing, Pulled, BothExist };

// The newest cloud copy before the ROM reads its save, as SaveSync::PullBeforeLaunch does it for
// a folder. still_wanted is asked once more right before anything is written.
Pull PullBeforeLaunch(u64 id, const std::filesystem::path& sav,
                      const std::function<bool()>& still_wanted)
{
    const std::string key = KeyOfTitle(id);
    const auto bytes = Api::PullSave(key);
    if (!bytes || bytes->empty())
        return Pull::Nothing;
    const std::string cloud = Api::SaveVersion(key);
    const std::string known = ReadMarker(sav);
    const bool local = HasLocal(sav);
    if (local && cloud == known)
        return Pull::Nothing;
    if (local && known.empty())
    {
        // Both sides have a save and they never met -- unless they are the same bytes.
        if (ReadFile(sav) == *bytes)
        {
            WriteMarker(sav, cloud);
            return Pull::Nothing;
        }
        return Pull::BothExist;
    }
    if (still_wanted && !still_wanted())
        return Pull::Nothing;
    return ApplyCloud(sav, *bytes, cloud) ? Pull::Pulled : Pull::Nothing;
}
}  // namespace

bool CloudSyncEnabled()
{
    return Global().GetBool("OpenPak.CloudSync");
}

void ApplyServer()
{
    auto cfg = Global();
    if (!cfg.GetBool("LAN.OpenPak"))
        Net_Slirp::SetOpenPakServer("");
    else
        Net_Slirp::SetOpenPakServer(g_profile_server.empty() ? cfg.GetString("LAN.OpenPakServer") :
                                                               g_profile_server);
}

void Init()
{
    std::filesystem::path config_dir =
        std::filesystem::path(Platform::GetLocalFilePath("melonDS.toml")).parent_path();
    openpak::Platform::SetDirectories(config_dir, config_dir / "openpak_cache");
    openpak::Platform::SetClient("melonds", MELONDS_VERSION);
    // The website is read once, before the first request (the library keeps it for the run).
    if (const std::string website = Global().GetString("OpenPak.Website"); !website.empty())
        qputenv("OPENPAK_API", QByteArray::fromStdString(website));
    Api::SetSavesPlatform("ds");
    if (const std::string device = Global().GetString("OpenPak.DeviceName"); !device.empty())
        Api::SetSaveDevice(device);

    // One conditional request at launch, off the UI thread: what is OpenPak, and what should
    // this emulator send it? Offline it keeps the last-known-good profile or the compiled-in
    // list. Applied on the UI thread, where the resolver's settings are changed.
    std::thread([] {
        auto applied = std::make_shared<openpak::NetworkProfile::Result>(
            openpak::NetworkProfile::Fetch("ds"));
        QMetaObject::invokeMethod(qApp, [applied] { ApplyProfile(*applied); },
                                  Qt::QueuedConnection);
    }).detach();
}

void AddMenu(QMainWindow* window, QMenuBar* menu_bar)
{
    EnsureHost(window);
    openpak::qt::MenuHooks hooks;
    hooks.game_running = &OpenPakGameRunning;
    hooks.openpak_on = &OpenPakOn;
    hooks.signed_in_as = [] {
        return SignedIn() ? QString::fromStdString(Common::OpenPakAccount::GetUsername()) :
                            QString{};
    };
    hooks.avatar = [] { return g_avatar.value_or(QPixmap{}); };
    hooks.sign_in = [window] { ShowSignIn(window); };
    hooks.sign_out = [] { g_host->SignOut(); };
    hooks.open_window = &OpenWindow;
    hooks.open_settings = [window] { ShowSettings(window); };
    openpak::qt::AddOpenPakMenu(menu_bar, std::move(hooks));
}

void MaybeAskToConnect()
{
    auto cfg = Global();
    if (!g_host || cfg.GetBool("OpenPak.ConnectAsked"))
        return;
    cfg.SetBool("OpenPak.ConnectAsked", true);
    Config::Save();
    if (SignedIn())
        return;
    if (!openpak::qt::AskToConnect(g_host->main_window, openpak::qt::Family::DS))
        return;
    if (ShowSignIn(g_host->main_window))
    {
        cfg.SetBool("LAN.OpenPak", true);
        cfg.SetBool("OpenPak.CloudSync", true);
        Config::Save();
        ApplyServer();
    }
}

void ShowSettings(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(Tr("OpenPak settings"));
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout(&dialog);
    auto cfg = Global();

    // Account
    auto* account = new QGroupBox(Tr("Account"));
    auto* account_layout = new QVBoxLayout(account);
    auto* enable = new QCheckBox(Tr("Connect Nintendo WFC to OpenPak"));
    enable->setToolTip(Tr("When this is off the emulator behaves exactly as upstream does: "
                          "offline, and nothing is sent anywhere."));
    enable->setChecked(cfg.GetBool("LAN.OpenPak"));
    account_layout->addWidget(enable);
    auto* account_row = new QHBoxLayout;
    auto* account_text = new QLabel;
    account_text->setWordWrap(true);
    auto* account_button = new QPushButton;
    account_row->addWidget(account_text, 1);
    account_row->addWidget(account_button);
    account_layout->addLayout(account_row);
    auto* open = new QPushButton(Tr("Open OpenPak..."));
    auto* open_row = new QHBoxLayout;
    open_row->addWidget(open);
    open_row->addStretch(1);
    account_layout->addLayout(open_row);
    auto* cloud = new QCheckBox(Tr("Sync cloud saves automatically when a game starts and stops"));
    cloud->setChecked(cfg.GetBool("OpenPak.CloudSync"));
    account_layout->addWidget(cloud);
    layout->addWidget(account);

    // Notifications
    auto* notifications = new QGroupBox(Tr("Notifications"));
    auto* notifications_layout = new QFormLayout(notifications);
    auto* show = new QCheckBox(Tr("Show notifications"));
    show->setChecked(cfg.GetBool("OpenPak.Notifications"));
    notifications_layout->addRow(show);
    auto* corner = new QComboBox;
    corner->addItems({Tr("Bottom right"), Tr("Bottom left"), Tr("Top right"), Tr("Top left")});
    corner->setCurrentIndex(std::clamp(cfg.GetInt("OpenPak.NotificationCorner"), 0, 3));
    notifications_layout->addRow(Tr("Notification corner"), corner);
    layout->addWidget(notifications);

    // Advanced, collapsed
    auto* advanced_toggle = new QToolButton;
    advanced_toggle->setText(Tr("Advanced"));
    advanced_toggle->setCheckable(true);
    advanced_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    advanced_toggle->setArrowType(Qt::RightArrow);
    advanced_toggle->setAutoRaise(true);
    layout->addWidget(advanced_toggle);
    auto* advanced = new QWidget;
    auto* advanced_layout = new QFormLayout(advanced);
    auto* website = new QLineEdit(QString::fromStdString(cfg.GetString("OpenPak.Website")));
    website->setPlaceholderText(QStringLiteral("https://openpak.org"));
    website->setToolTip(Tr("Where the account lives and where you sign in. Leave this at "
                           "openpak.org unless you run your own deployment."));
    advanced_layout->addRow(Tr("Website"), website);
    auto* refresh = new QPushButton(Tr("Refresh network settings"));
    auto* network_status = new QLabel;
    network_status->setWordWrap(true);
    if (const auto stored = openpak::NetworkProfile::LoadStored("ds"))
        network_status->setText(
            NetworkStatusLine(stored->version, openpak::NetworkProfile::Source::Cached));
    else
        network_status->setText(NetworkStatusLine(0, openpak::NetworkProfile::Source::BuiltIn));
    auto* refresh_row = new QHBoxLayout;
    refresh_row->addWidget(refresh);
    refresh_row->addWidget(network_status, 1);
    advanced_layout->addRow(refresh_row);
    advanced->hide();
    layout->addWidget(advanced);
    QObject::connect(advanced_toggle, &QToolButton::toggled, advanced, [advanced, advanced_toggle](bool on) {
        advanced->setVisible(on);
        advanced_toggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    // Every change applies at once, as the upstream Wi-Fi checkbox mirrors the same key.
    QObject::connect(enable, &QCheckBox::toggled, &dialog, [](bool on) {
        Global().SetBool("LAN.OpenPak", on);
        ApplyServer();
    });
    QObject::connect(cloud, &QCheckBox::toggled, &dialog,
                     [](bool on) { Global().SetBool("OpenPak.CloudSync", on); });
    QObject::connect(show, &QCheckBox::toggled, &dialog,
                     [](bool on) { Global().SetBool("OpenPak.Notifications", on); });
    QObject::connect(corner, &QComboBox::currentIndexChanged, &dialog,
                     [](int index) { Global().SetInt("OpenPak.NotificationCorner", index); });
    QObject::connect(website, &QLineEdit::editingFinished, &dialog, [website] {
        Global().SetString("OpenPak.Website", website->text().trimmed().toStdString());
    });

    // No network on the UI thread: the refresh runs beside it and reports back.
    QObject::connect(refresh, &QPushButton::clicked, &dialog, [refresh, network_status] {
        refresh->setEnabled(false);
        network_status->setText(Tr("Checking..."));
        QPointer<QLabel> status{network_status};
        QPointer<QPushButton> button{refresh};
        std::thread([status, button] {
            auto applied = std::make_shared<openpak::NetworkProfile::Result>(
                openpak::NetworkProfile::Refresh("ds"));
            QMetaObject::invokeMethod(qApp, [status, button, applied] {
                ApplyProfile(*applied);
                if (status)
                    status->setText(NetworkStatusLine(applied->profile.version, applied->source));
                if (button)
                    button->setEnabled(true);
            }, Qt::QueuedConnection);
        }).detach();
    });

    const auto update = [enable, account_text, account_button] {
        const bool running = OpenPakGameRunning();
        enable->setEnabled(!running);
        if (SignedIn())
        {
            account_text->setText(Tr("Signed in as %1")
                                      .arg(QString::fromStdString(
                                          Common::OpenPakAccount::GetUsername())));
            account_button->setText(Tr("Sign out..."));
        }
        else
        {
            account_text->setText(Tr("Not signed in"));
            account_button->setText(Tr("Sign in..."));
        }
        account_button->setEnabled(!running);
        account_button->setToolTip(running ? Tr("Stop the running game first.") : QString{});
    };
    update();
    QObject::connect(account_button, &QPushButton::clicked, &dialog, [&dialog, update] {
        if (SignedIn())
        {
            if (openpak::qt::ConfirmSignOut(&dialog))
                g_host->SignOut();
        }
        else
        {
            ShowSignIn(&dialog);
        }
        update();
    });
    QObject::connect(open, &QPushButton::clicked, &dialog,
                     [] { OpenWindow(OpenPakAccountDialog::kAccountPage); });

    dialog.exec();
    Global().SetString("OpenPak.Website", website->text().trimmed().toStdString());
    Config::Save();
}

void OnRomLoading(const unsigned char* filedata, unsigned int filelen,
                  const std::string& savname)
{
    // Whatever was loaded before is closed by the loader; its save is final.
    PushNow(false);

    // The DS cartridge header carries a four-letter game code at 0x0C; it is what a player's
    // own .sav is named after, so it is what the cloud copy is keyed by too.
    if (filelen < 0x10)
        return;
    std::string code(reinterpret_cast<const char*>(filedata) + 0x0C, 4);
    const u64 id = TitleOfCode(code);
    if (id == 0)
        return;
    {
        std::lock_guard lock(g_run_mutex);
        g_run_title = id;
        g_sav_path = savname;
    }
    Remember(id, savname, std::filesystem::path(savname).stem().string());

    if (!CloudSyncEnabled() || !SignedIn())
        return;

    // At most five seconds, with a Skip (UX spec §5.1): the pull runs on a worker while this
    // (emulator) thread waits; the UI thread shows "Checking cloud save..." if it takes a while.
    struct Wait
    {
        std::mutex mutex;
        std::condition_variable done_cv;
        bool done = false;
        std::atomic<bool> wanted{true};
        Pull result = Pull::Nothing;
    };
    auto wait = std::make_shared<Wait>();
    std::thread([wait, id, sav = std::filesystem::path(savname)] {
        const Pull result = PullBeforeLaunch(id, sav, [wait] { return wait->wanted.load(); });
        std::lock_guard lock(wait->mutex);
        wait->result = result;
        wait->done = true;
        wait->done_cv.notify_all();
    }).detach();

    auto dialog = std::make_shared<QPointer<QDialog>>();
    QMetaObject::invokeMethod(qApp, [wait, dialog] {
    QTimer::singleShot(300, qApp, [wait, dialog] {
        {
            std::lock_guard lock(wait->mutex);
            if (wait->done || !wait->wanted)
                return;
        }
        auto* box = new QDialog(g_host ? g_host->main_window : nullptr);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->setWindowTitle(Tr("OpenPak"));
        auto* layout = new QVBoxLayout(box);
        layout->addWidget(new QLabel(Tr("Checking cloud save...")));
        auto* bar = new QProgressBar;
        bar->setRange(0, 0);
        bar->setTextVisible(false);
        layout->addWidget(bar);
        auto* buttons = new QDialogButtonBox;
        buttons->addButton(Tr("Skip"), QDialogButtonBox::RejectRole);
        QObject::connect(buttons, &QDialogButtonBox::rejected, box, [wait, box] {
            {
                std::lock_guard lock(wait->mutex);
                wait->wanted = false;
                wait->done_cv.notify_all();
            }
            box->close();
        });
        layout->addWidget(buttons);
        *dialog = box;
        box->show();
    });
    }, Qt::QueuedConnection);

    Pull result = Pull::Nothing;
    {
        std::unique_lock lock(wait->mutex);
        wait->done_cv.wait_for(lock, std::chrono::seconds(5),
                               [&] { return wait->done || !wait->wanted; });
        wait->wanted = wait->done;
        result = wait->done ? wait->result : Pull::Nothing;
    }
    QMetaObject::invokeMethod(qApp, [dialog] {
        if (*dialog)
            (*dialog)->close();
    }, Qt::QueuedConnection);

    if (result == Pull::Pulled)
    {
        g_paused.erase(id);
        Platform::Log(Platform::LogLevel::Info, "OpenPak: cloud save applied to %s\n",
                      savname.c_str());
        Toast(Tr("Cloud save for %1 downloaded; the previous local copy was kept beside it.")
                  .arg(NameOf(id)),
              Kind::Saves);
    }
    else if (result == Pull::BothExist)
    {
        // Starts on the local save; automatic sync for this title waits for the choice.
        g_paused.insert(id);
        Toast(Tr("%1 has a save here and a different one in the cloud. Choose one on the Cloud "
                 "saves page.")
                  .arg(NameOf(id)),
              Kind::Saves);
    }
    else
    {
        g_paused.erase(id);
    }
}

void OnRomClosed()
{
    PushNow(false);
}

void PushPending(bool wait)
{
    PushNow(wait);
}
}  // namespace OpenPak
