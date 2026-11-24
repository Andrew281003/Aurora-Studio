#include "theme_application.h"

#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
QString defaultConfigRoot()
{
    const QString env = qEnvironmentVariable("XDG_CONFIG_HOME");
    return env.isEmpty() ? QDir::homePath() + "/.config" : env;
}

QString defaultDataRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

bool atomicWrite(const QString &path, const QByteArray &bytes, QString *error)
{
    const QFileInfo info(path);
    if (info.isSymLink()) {
        if (error) *error = "Refusing to atomically replace a symbolic link";
        return false;
    }
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) *error = "Could not create the destination directory";
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

QVariantMap result(const QString &name, bool supported, bool success, bool changed,
                   const QString &message)
{
    return {{"name", name}, {"supported", supported}, {"success", success},
            {"changed", changed}, {"message", message}};
}

bool validColor(const QString &value)
{
    static const QRegularExpression color(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    return color.match(value).hasMatch();
}

QStringList themeColors(const QVariantMap &theme, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) *error = message;
        return QStringList{};
    };
    if (theme.value("schemaVersion").toInt() != 1 || theme.value("id").toString().isEmpty()
        || theme.value("name").toString().trimmed().isEmpty())
        return fail("Preset identity or schema is invalid");

    const QVariantMap palette = theme.value("palette").toMap();
    for (const QString &key : {"background", "surface", "accent", "secondary", "tertiary",
                                "text", "muted", "border", "error", "warning", "success"}) {
        if (!validColor(palette.value(key).toString()))
            return fail("Preset contains an invalid color: " + key);
    }
    const QVariantMap terminal = theme.value("terminal").toMap();
    QStringList colors;
    for (const QString &key : {"background", "foreground", "cursor"}) {
        const QString color = terminal.value(key).toString();
        if (!validColor(color)) return fail("Terminal color is missing or invalid: " + key);
        colors << color;
    }
    const QVariantList ansi = terminal.value("ansi").toList();
    if (ansi.size() != 8) return fail("Terminal palette must contain eight ANSI colors");
    for (const QVariant &entry : ansi) {
        const QString color = entry.toString();
        if (!validColor(color)) return fail("Terminal palette contains an invalid ANSI color");
        colors << color;
    }
    return colors;
}

QByteArray alacrittyColors(const QVariantMap &theme, const QStringList &ansi)
{
    const QVariantMap terminal = theme.value("terminal").toMap();
    const QStringList names = {"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"};
    QString out;
    out += "# Managed by Aurora Studio. Changes here are replaced on the next Apply.\n";
    out += "[colors.primary]\nbackground = \"" + terminal.value("background").toString() + "\"\n";
    out += "foreground = \"" + terminal.value("foreground").toString() + "\"\n\n";
    out += "[colors.cursor]\ntext = \"" + terminal.value("background").toString() + "\"\n";
    out += "cursor = \"" + terminal.value("cursor").toString() + "\"\n\n";
    out += "[colors.selection]\ntext = \"" + terminal.value("foreground").toString() + "\"\n";
    out += "background = \"" + theme.value("palette").toMap().value("surface").toString() + "\"\n\n";
    out += "[colors.normal]\n";
    for (int i = 0; i < 8; ++i) out += names[i] + " = \"" + ansi[i + 3] + "\"\n";
    out += "\n[colors.bright]\n";
    for (int i = 0; i < 8; ++i) out += names[i] + " = \"" + ansi[i + 3] + "\"\n";
    return out.toUtf8();
}

bool removeAlacrittyColors(const QByteArray &original, QByteArray *updated, QString *error)
{
    const QList<QByteArray> lines = original.split('\n');
    QByteArray kept;
    bool inColorsTable = false;
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith('[')) {
            const int close = trimmed.indexOf(']');
            if (close < 0) {
                if (error) *error = "Alacritty configuration has a malformed table header";
                return false;
            }
            const QByteArray table = trimmed.left(close + 1);
            inColorsTable = table == "[colors]" || table.startsWith("[colors.");
        }
        if (!inColorsTable) kept += line + '\n';
    }
    while (kept.endsWith("\n\n")) kept.chop(1);
    if (!kept.isEmpty() && !kept.endsWith('\n')) kept += '\n';
    kept += '\n';
    *updated = kept;
    return true;
}

bool mergeAlacrittyImport(const QByteArray &original, const QString &includePath,
                          QByteArray *updated, QString *error)
{
    if (includePath.contains('\n') || includePath.contains('\r')) {
        if (error) *error = "The Alacritty include path contains unsupported control characters";
        return false;
    }
    const QByteArray expected = "import = [\"" + includePath.toUtf8().replace("\\", "\\\\").replace("\"", "\\\"") + "\"]";
    const QList<QByteArray> lines = original.split('\n');
    bool sawImport = false;
    bool alreadyManaged = false;
    for (const QByteArray &line : lines) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith('#') || !trimmed.startsWith("import")) continue;
        static const QRegularExpression importKey(QStringLiteral("^import\\s*="));
        if (importKey.match(QString::fromUtf8(trimmed)).hasMatch()) {
            if (sawImport) {
                if (error) *error = "Multiple Alacritty imports cannot be safely merged";
                return false;
            }
            sawImport = true;
            alreadyManaged = trimmed == expected;
        }
    }
    if (sawImport && !alreadyManaged) {
        if (error) *error = "An existing Alacritty import must be merged manually before Aurora Studio can manage colors";
        return false;
    }
    if (alreadyManaged) { *updated = original; return true; }
    *updated = expected + "\n" + original;
    return true;
}

QJsonObject readJournal(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QString detectedWallpaperManager()
{
#ifdef Q_OS_LINUX
    const QDir proc("/proc");
    const QStringList candidates = {"dms", "hyprpaper", "swww-daemon", "awww-daemon", "wpaperd"};
    for (const QString &pid : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        pid.toLongLong(&numeric);
        if (!numeric) continue;
        QFile comm(proc.filePath(pid + "/comm"));
        if (!comm.open(QIODevice::ReadOnly)) continue;
        const QString name = QString::fromUtf8(comm.readAll()).trimmed();
        if (candidates.contains(name)) return name;
    }
#endif
    return {};
}

bool writeJournal(const QString &path, const QJsonObject &object, QString *error)
{
    return atomicWrite(path, QJsonDocument(object).toJson(QJsonDocument::Compact), error);
}

QString sha256(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
} // namespace

ThemeApplication::ThemeApplication(QString configRoot, QString dataRoot)
    : m_configRoot(configRoot.isEmpty() ? defaultConfigRoot() : std::move(configRoot)),
      m_dataRoot(dataRoot.isEmpty() ? defaultDataRoot() : std::move(dataRoot))
{
    if (!recoverInterrupted())
        m_recoveryMessage = "An interrupted application could not be recovered; Apply is disabled until Restore succeeds.";
}

QString ThemeApplication::journalPath() const { return m_dataRoot + "/theme-application.json"; }
QString ThemeApplication::backupPath() const { return m_dataRoot + "/previous-alacritty.toml"; }
QString ThemeApplication::transactionBackupPath() const { return m_dataRoot + "/transaction-before-alacritty.toml"; }
static QString managedColorsPath(const QString &configRoot) { return configRoot + "/alacritty/aurora-studio-colors.toml"; }

bool ThemeApplication::recoverInterrupted()
{
    if (!QFileInfo::exists(journalPath())) return true;
    const QJsonObject journal = readJournal(journalPath());
    const QString state = journal.value("state").toString();
    if (state == "applied" || state == "restored") return true;
    if (state != "pending") return false;
    const QString rollback = journal.value("rollbackPath").toString();
    const QString expectedConfig = m_configRoot + "/alacritty/alacritty.toml";
    const QString expectedInclude = managedColorsPath(m_configRoot);
    if (journal.value("configPath").toString() != expectedConfig
        || journal.value("includePath").toString() != expectedInclude
        || rollback != transactionBackupPath() || !journal.value("existed").toBool()
        || !QFileInfo::exists(rollback) || QFileInfo(expectedConfig).isSymLink()
        || QFileInfo(expectedInclude).isSymLink() || QFileInfo(rollback).isSymLink()) return false;
    QFile backup(rollback);
    if (!backup.open(QIODevice::ReadOnly)) return false;
    const QByteArray configBytes = backup.readAll();
    QByteArray includeBytes;
    if (journal.value("includeExisted").toBool()) {
        QFile includeBackup(m_dataRoot + "/transaction-before-alacritty-colors.toml");
        if (!includeBackup.open(QIODevice::ReadOnly)) return false;
        includeBytes = includeBackup.readAll();
    }
    QString error;
    if (!atomicWrite(journal.value("configPath").toString(), configBytes, &error)) return false;
    const QString includePath = journal.value("includePath").toString();
    if (journal.value("includeExisted").toBool()) {
        if (!atomicWrite(includePath, includeBytes, &error)) return false;
    } else if (QFileInfo::exists(includePath) && !QFile::remove(includePath)) {
        return false;
    }
    QFile verifyConfig(expectedConfig);
    if (!verifyConfig.open(QIODevice::ReadOnly) || verifyConfig.readAll() != configBytes) return false;
    if (journal.value("includeExisted").toBool()) {
        QFile verifyInclude(includePath);
        if (!verifyInclude.open(QIODevice::ReadOnly) || verifyInclude.readAll() != includeBytes) return false;
    } else if (QFileInfo::exists(includePath)) {
        return false;
    }
    QJsonObject recovered;
    if (journal.value("previousJournal").isObject()
        && journal.value("previousJournal").toObject().value("state").toString() == "applied") {
        recovered = journal.value("previousJournal").toObject();
    } else {
        recovered = journal;
        recovered["state"] = "restored";
        recovered["recovery"] = "interrupted application rolled back on startup";
    }
    return writeJournal(journalPath(), recovered, &error);
}

QVariantList ThemeApplication::status() const
{
    const QString config = m_configRoot + "/alacritty/alacritty.toml";
    const bool active = QFileInfo::exists(config);
    const bool disabled = !m_recoveryMessage.isEmpty();
    const QString wallpaperManager = detectedWallpaperManager();
    const QString wallpaperMessage = wallpaperManager.isEmpty()
        ? "Unsupported: no known wallpaper manager was detected; no wallpaper changes are available."
        : "Unsupported: detected " + wallpaperManager + ", but this build has no verified external set-and-rollback interface.";
    return {
        result("Alacritty", true, active && !disabled, false,
               disabled ? m_recoveryMessage : (active ? "Supported: managed colors in the active TOML configuration." : "Unsupported: active alacritty.toml was not found.")),
        result("Hyprland", false, false, false, "Unsupported: the active session uses a DMS-managed Lua config; no safe external palette include is available."),
        result("Wallpaper", false, false, false, wallpaperMessage),
        result("DMS", false, false, false, "Unsupported: DMS exposes seed-based generation, not an exact external Aurora palette API."),
        result("GTK", false, false, false, "Unsupported: no ownership-safe appearance interface was verified."),
        result("Qt", false, false, false, "Unsupported: no ownership-safe desktop-wide appearance interface was verified.")
    };
}

bool ThemeApplication::hasRestorePoint() const
{
    const QJsonObject journal = readJournal(journalPath());
    return journal.value("state").toString() == "applied" && QFileInfo::exists(backupPath());
}

QVariantList ThemeApplication::apply(const QVariantMap &theme)
{
    if (!m_recoveryMessage.isEmpty())
        return {result("Alacritty", true, false, false, m_recoveryMessage)};
    QString error;
    const QStringList colors = themeColors(theme, &error);
    if (colors.isEmpty()) return {result("Alacritty", true, false, false, "Preset rejected: " + error)};

    const QString config = m_configRoot + "/alacritty/alacritty.toml";
    const QString includePath = managedColorsPath(m_configRoot);
    QFile current(config);
    if (QFileInfo(config).isSymLink())
        return {result("Alacritty", true, false, false, "The active Alacritty configuration is a symbolic link; refusing to replace it atomically")};
    if (!current.open(QIODevice::ReadOnly))
        return {result("Alacritty", true, false, false, "Active Alacritty configuration is unavailable: " + current.errorString())};
    const QByteArray before = current.readAll();
    QByteArray after;
    if (!removeAlacrittyColors(before, &after, &error))
        return {result("Alacritty", true, false, false, "Configuration rejected: " + error)};
    QByteArray afterImport;
    if (!mergeAlacrittyImport(after, includePath, &afterImport, &error))
        return {result("Alacritty", true, false, false, "Configuration rejected: " + error)};
    QFile existingInclude(includePath);
    const bool includeExisted = existingInclude.exists();
    if (QFileInfo(includePath).isSymLink())
        return {result("Alacritty", true, false, false, "The Aurora Studio include path is a symbolic link; refusing to replace it")};
    QByteArray priorInclude;
    if (includeExisted) {
        if (!existingInclude.open(QIODevice::ReadOnly)) return {result("Alacritty", true, false, false, "Could not inspect the managed color include")};
        priorInclude = existingInclude.readAll();
        if (!priorInclude.contains("Managed by Aurora Studio"))
            return {result("Alacritty", true, false, false, "The Aurora Studio color include path is already occupied by another file")};
    }
    const QByteArray includeAfter = alacrittyColors(theme, colors);

    QJsonObject previous = readJournal(journalPath());
    const QString state = previous.value("state").toString();
    bool baselineIncludeExisted = previous.value("baselineIncludeExisted").toBool();
    if (state != "applied") {
        if (!atomicWrite(backupPath(), before, &error))
            return {result("Alacritty", true, false, false, "Could not create a restore backup: " + error)};
        baselineIncludeExisted = includeExisted;
        if (includeExisted && !atomicWrite(m_dataRoot + "/previous-alacritty-colors.toml", priorInclude, &error))
            return {result("Alacritty", true, false, false, "Could not back up the previous color include: " + error)};
    }
    if (!atomicWrite(transactionBackupPath(), before, &error))
        return {result("Alacritty", true, false, false, "Could not create a transaction rollback point: " + error)};
    if (includeExisted && !atomicWrite(m_dataRoot + "/transaction-before-alacritty-colors.toml", priorInclude, &error))
        return {result("Alacritty", true, false, false, "Could not back up the existing managed include: " + error)};
    QJsonObject journal{{"state", "pending"}, {"configPath", config}, {"existed", true},
                        {"rollbackPath", transactionBackupPath()}, {"includePath", includePath},
                        {"includeExisted", includeExisted}, {"baselineIncludeExisted", baselineIncludeExisted},
                        {"presetId", theme.value("id").toString()},
                        {"previousJournal", state == "applied" ? QJsonValue(previous) : QJsonValue(QJsonObject{})}};
    if (!writeJournal(journalPath(), journal, &error))
        return {result("Alacritty", true, false, false, "Could not start a recoverable transaction: " + error)};
    const auto rollbackTransaction = [&]() {
        QFile backup(transactionBackupPath());
        if (!backup.open(QIODevice::ReadOnly)) return false;
        const QByteArray configBytes = backup.readAll();
        if (!atomicWrite(config, configBytes, nullptr)) return false;
        if (includeExisted) {
            if (!atomicWrite(includePath, priorInclude, nullptr)) return false;
        } else if (QFileInfo::exists(includePath) && !QFile::remove(includePath)) {
            return false;
        }
        QFile verifyConfig(config);
        if (!verifyConfig.open(QIODevice::ReadOnly) || verifyConfig.readAll() != configBytes) return false;
        if (includeExisted) {
            QFile verifyPriorInclude(includePath);
            if (!verifyPriorInclude.open(QIODevice::ReadOnly) || verifyPriorInclude.readAll() != priorInclude) return false;
        }
        if (state == "applied") return writeJournal(journalPath(), previous, nullptr);
        journal["state"] = "restored";
        return writeJournal(journalPath(), journal, nullptr);
    };
    if (!atomicWrite(includePath, includeAfter, &error) || !atomicWrite(config, afterImport, &error)) {
        const bool rolledBack = rollbackTransaction();
        return {result("Alacritty", true, false, false, rolledBack
            ? "Apply failed and the previous configuration was restored: " + error
            : "Apply failed and rollback is pending recovery on the next Aurora Studio launch: " + error)};
    }

    QFile verify(config);
    QFile verifyInclude(includePath);
    if (!verify.open(QIODevice::ReadOnly) || verify.readAll() != afterImport
        || !verifyInclude.open(QIODevice::ReadOnly) || verifyInclude.readAll() != includeAfter) {
        const bool rolledBack = rollbackTransaction();
        return {result("Alacritty", true, false, false, rolledBack
            ? "Verification failed; previous configuration was restored."
            : "Verification failed and rollback is pending recovery on the next Aurora Studio launch.")};
    }
    journal["state"] = "applied";
    journal["configSha256"] = sha256(afterImport);
    journal["includeSha256"] = sha256(includeAfter);
    if (!writeJournal(journalPath(), journal, &error)) {
        // Leave the pending marker intact if possible; startup recovery will restore the before-image.
        return {result("Alacritty", true, false, true, "Colors were written, but the commit record failed; restart Aurora Studio to recover safely.")};
    }
    m_recoveryMessage.clear();
    return {result("Alacritty", true, true, true, "Alacritty managed color include and import were written and verified. They take effect in newly opened terminal windows.")};
}

bool ThemeApplication::restore(QString *message)
{
    QJsonObject journal = readJournal(journalPath());
    if (journal.value("state").toString() != "applied") {
        if (message) *message = "There is no applied Aurora theme to restore.";
        return false;
    }
    if (journal.value("configPath").toString() != m_configRoot + "/alacritty/alacritty.toml"
        || journal.value("includePath").toString() != managedColorsPath(m_configRoot)) {
        if (message) *message = "The saved restore record does not match the detected Alacritty configuration.";
        return false;
    }
    QFile activeConfig(journal.value("configPath").toString());
    QFile activeInclude(journal.value("includePath").toString());
    if (QFileInfo(activeConfig.fileName()).isSymLink() || QFileInfo(activeInclude.fileName()).isSymLink()) {
        if (message) *message = "An Alacritty file became a symbolic link after Apply; restore was stopped to preserve it.";
        return false;
    }
    if (!activeConfig.open(QIODevice::ReadOnly) || !activeInclude.open(QIODevice::ReadOnly)
        || sha256(activeConfig.readAll()) != journal.value("configSha256").toString()
        || sha256(activeInclude.readAll()) != journal.value("includeSha256").toString()) {
        if (message) *message = "Alacritty files changed after Apply; restore was stopped to preserve those newer edits.";
        return false;
    }
    if (!QFileInfo::exists(backupPath())) {
        if (message) *message = "The previous configuration backup is missing.";
        return false;
    }
    if (QFileInfo(backupPath()).isSymLink()) {
        if (message) *message = "The previous configuration backup is a symbolic link; restore was stopped.";
        return false;
    }
    QFile backup(backupPath());
    if (!backup.open(QIODevice::ReadOnly)) {
        if (message) *message = "Could not read the previous configuration backup.";
        return false;
    }
    const QByteArray expected = backup.readAll();
    QByteArray previousInclude;
    if (journal.value("baselineIncludeExisted").toBool()) {
        const QString includeBackupPath = m_dataRoot + "/previous-alacritty-colors.toml";
        if (QFileInfo(includeBackupPath).isSymLink()) {
            if (message) *message = "The previous color include backup is a symbolic link; restore was stopped.";
            return false;
        }
        QFile includeBackup(includeBackupPath);
        if (!includeBackup.open(QIODevice::ReadOnly)) {
            if (message) *message = "The previous color include backup is missing.";
            return false;
        }
        previousInclude = includeBackup.readAll();
    }
    QString error;
    if (!atomicWrite(journal.value("configPath").toString(), expected, &error)) {
        if (message) *message = "Restore failed: " + error;
        return false;
    }
    QFile verify(journal.value("configPath").toString());
    if (!verify.open(QIODevice::ReadOnly) || verify.readAll() != expected) {
        if (message) *message = "Restore wrote the backup but verification failed.";
        return false;
    }
    const QString includePath = journal.value("includePath").toString();
    if (journal.value("baselineIncludeExisted").toBool()) {
        if (!atomicWrite(includePath, previousInclude, &error)) {
            if (message) *message = "Could not restore the previous color include: " + error;
            return false;
        }
        QFile verifyInclude(includePath);
        if (!verifyInclude.open(QIODevice::ReadOnly) || verifyInclude.readAll() != previousInclude) {
            if (message) *message = "The previous color include was restored but verification failed.";
            return false;
        }
    } else if (QFileInfo::exists(includePath) && !QFile::remove(includePath)) {
        if (message) *message = "Could not remove Aurora Studio's managed color include.";
        return false;
    }
    journal["state"] = "restored";
    if (!writeJournal(journalPath(), journal, &error)) {
        if (message) *message = "Configuration was restored, but the state record could not be updated: " + error;
        return false;
    }
    m_recoveryMessage.clear();
    if (message) *message = "Previous Alacritty configuration was restored and verified.";
    return true;
}
