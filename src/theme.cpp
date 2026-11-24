#include "theme.h"
#include "theme_application.h"
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

static QVariantMap readTheme(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    auto doc=QJsonDocument::fromJson(f.readAll()); if (!doc.isObject()) return {};
    auto m=doc.object().toVariantMap(); m["path"]=path; return m;
}
static QString previewUrlFor(const QVariantMap &theme, const QString &themePath) {
    const QString preview = theme.value("preview").toString();
    QString fileName = QFileInfo(preview).fileName();

    const QString adjacent = QFileInfo(themePath).dir().filePath(preview);
    if (!themePath.startsWith(":/") && !preview.isEmpty() && QFileInfo::exists(adjacent))
        return QUrl::fromLocalFile(QFileInfo(adjacent).absoluteFilePath()).toString();
    const QString resource = QStringLiteral(":/assets/") + fileName;
    if (QFileInfo::exists(resource))
        return QStringLiteral("qrc") + resource;
    if (fileName.endsWith(".jpg") || fileName.endsWith(".jpeg")) {
        const QString svgResource = QStringLiteral(":/assets/") + QFileInfo(fileName).completeBaseName() + ".svg";
        if (QFileInfo::exists(svgResource))
            return QStringLiteral("qrc") + svgResource;
    }
    const QString themeResource = QStringLiteral(":/assets/") + theme.value("id").toString() + ".svg";
    if (QFileInfo::exists(themeResource))
        return QStringLiteral("qrc") + themeResource;
    return {};
}
ThemeRepository::ThemeRepository(QObject *p):QObject(p),m_application(new ThemeApplication) { reload(); }
ThemeRepository::~ThemeRepository(){delete m_application;}
QString ThemeRepository::userDir() const { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/themes"; }
void ThemeRepository::reload() {
    m_themes.clear(); QDir bundled(QStringLiteral(":/themes"));
    for (const auto &f:bundled.entryList({"*.json"},QDir::Files,QDir::Name)) { auto t=readTheme(bundled.filePath(f)); QString error; if(validate(t,&error)){auto map=t;map["builtin"]=true;map["previewUrl"]=previewUrlFor(map,bundled.filePath(f));m_themes<<map;} }
    QDir users(userDir()); for(const auto &f:users.entryList({"*.json"},QDir::Files,QDir::Name)){auto t=readTheme(users.filePath(f));QString error;if(validate(t,&error)){auto map=t;map["builtin"]=false;map["previewUrl"]=previewUrlFor(map,users.filePath(f));m_themes<<map;}}
    if(m_selected>=m_themes.size())m_selected=qMax(0,m_themes.size()-1); emit themesChanged();emit selectedChanged();
}
QVariantList ThemeRepository::themes() const{return m_themes;}
void ThemeRepository::setSelected(int i){if(i<0||i>=m_themes.size()||i==m_selected)return;m_selected=i;emit selectedChanged();}
bool ThemeRepository::validate(const QVariantMap&t,QString*error)const{
    auto fail=[&](QString s){if(error)*error=s;return false;};
    if(t.value("schemaVersion").toInt()!=1)return fail("Unsupported schema version");
    if(t.value("id").toString().isEmpty()||t.value("name").toString().trimmed().isEmpty())return fail("Theme needs an id and name");
    auto p=t.value("palette").toMap(); const QStringList keys={"background","surface","accent","secondary","tertiary","text","muted","border","error","warning","success"};
    QRegularExpression color("^#[0-9A-Fa-f]{6}$"); for(const auto&k:keys)if(!color.match(p.value(k).toString()).hasMatch())return fail("Invalid or missing color: "+k);
    auto terminal=t.value("terminal").toMap();if(terminal.value("ansi").toList().size()!=8)return fail("Terminal palette must contain eight ANSI colors");
    return true;
}
void ThemeRepository::createTheme(){
    if(m_themes.isEmpty())return; auto t=m_themes[m_selected].toMap();t["id"]="custom-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);t["name"]="My Aurora Theme";t["description"]="A personal color story";t.remove("path");t["builtin"]=false;m_draft=t;m_editingId.clear();m_editing=true;emit draftChanged();
}
void ThemeRepository::editSelected(){if(m_themes.isEmpty())return;auto t=m_themes[m_selected].toMap();if(t.value("builtin").toBool()){createTheme();return;}m_draft=t;m_draft.remove("path");m_editingId=t.value("id").toString();m_editing=true;emit draftChanged();}
void ThemeRepository::duplicateSelected(){createTheme();if(m_editing){m_draft["name"]=m_themes[m_selected].toMap().value("name").toString()+" Copy";emit draftChanged();}}
void ThemeRepository::cancelDraft(){m_editing=false;m_draft.clear();emit draftChanged();}
void ThemeRepository::saveDraft(const QVariantMap&value){
    QVariantMap t=value;t["schemaVersion"]=1;if(t.value("id").toString().isEmpty())t["id"]="custom-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);t.remove("path");t.remove("builtin");t.remove("previewUrl");
    QString error;if(!validate(t,&error)){emit notice("Cannot save preset: "+error);return;}
    const QString id=t.value("id").toString();bool existingBuiltin=false;for(const auto&it:m_themes){auto x=it.toMap();if(x.value("id").toString()==id&&x.value("builtin").toBool())existingBuiltin=true;}
    if(existingBuiltin){t["id"]="custom-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);}
    QDir().mkpath(userDir());QSaveFile f(userDir()+"/"+t["id"].toString()+".json");if(!f.open(QIODevice::WriteOnly)||f.write(QJsonDocument::fromVariant(t).toJson(QJsonDocument::Indented))<0||!f.commit()){emit notice("Could not save preset");return;}
    reload();for(int i=0;i<m_themes.size();++i)if(m_themes[i].toMap().value("id")==t["id"]){setSelected(i);break;}cancelDraft();emit notice("Preset saved");
}
void ThemeRepository::deleteSelected(){if(m_themes.isEmpty())return;auto t=m_themes[m_selected].toMap();if(t.value("builtin").toBool()){emit notice("Bundled presets are immutable");return;}if(!QFile::remove(userDir()+"/"+t.value("id").toString()+".json")){emit notice("Could not delete preset");return;}reload();emit notice("Custom preset deleted");}
void ThemeRepository::importTheme(){QString p=QFileDialog::getOpenFileName(nullptr,"Import Aurora preset",{},"Aurora theme (*.json)");if(!p.isEmpty())importFile(p);}
bool ThemeRepository::importFile(const QString&p){auto t=readTheme(p);QString error;if(!validate(t,&error)){emit notice("Import rejected: "+error);return false;}t.remove("path");t["id"]="custom-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);saveDraft(t);return !m_editing;}
void ThemeRepository::exportPreset(){if(m_themes.isEmpty())return;QString p=QFileDialog::getSaveFileName(nullptr,"Export Aurora preset",m_themes[m_selected].toMap().value("id").toString()+".json","Aurora theme (*.json)");if(!p.isEmpty())exportFile(p);}
bool ThemeRepository::exportFile(const QString&p){if(m_themes.isEmpty())return false;auto t=m_themes[m_selected].toMap();t.remove("path");t.remove("builtin");t.remove("previewUrl");QSaveFile f(p);if(!f.open(QIODevice::WriteOnly)||f.write(QJsonDocument::fromVariant(t).toJson(QJsonDocument::Indented))<0||!f.commit()){emit notice("Could not export preset");return false;}emit notice("Preset exported");return true;}
void ThemeRepository::exportSelected(){exportPreset();}
void ThemeRepository::applySelected(){applySelectedTheme();}
QVariantList ThemeRepository::applySelectedTheme(){
    if(m_themes.isEmpty())return {};
    auto outcomes=m_application->apply(m_themes[m_selected].toMap());
    for(const auto &item:outcomes){auto result=item.toMap();emit notice(result.value("message").toString());}
    emit applicationStateChanged();
    return outcomes;
}
bool ThemeRepository::restorePreviousTheme(){
    QString message;bool ok=m_application->restore(&message);emit notice(message);emit applicationStateChanged();return ok;
}
QVariantList ThemeRepository::integrationStatus()const{return m_application->status();}
bool ThemeRepository::hasRestorePoint()const{return m_application->hasRestorePoint();}
