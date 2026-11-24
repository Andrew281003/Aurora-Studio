#include "../src/theme.h"
#include "../src/theme_application.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

class RepositoryTests:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){m_data=std::make_unique<QTemporaryDir>();qputenv("XDG_DATA_HOME",m_data->path().toUtf8());}
 void init(){m_data=std::make_unique<QTemporaryDir>();qputenv("XDG_DATA_HOME",m_data->path().toUtf8());}
 void galleryLoadsEight(){ThemeRepository r;QCOMPARE(r.themes().size(),8);}
 void bundledArtworkIsEmbedded(){ThemeRepository r;QCOMPARE(r.themes().size(),8);for(const auto&entry:r.themes()){const auto theme=entry.toMap();const QString url=theme.value("previewUrl").toString();QVERIFY2(url.startsWith("qrc:/assets/"),qPrintable(url));QFile asset(":/assets/"+QFileInfo(QUrl(url).path()).fileName());QVERIFY2(asset.open(QIODevice::ReadOnly),qPrintable(url));QVERIFY(asset.readAll().startsWith(QByteArray::fromHex("ffd8ff")));QFile svg(":/assets/"+theme.value("id").toString()+".svg");QVERIFY(svg.open(QIODevice::ReadOnly));QVERIFY(svg.readAll().startsWith("<svg"));}}
 void persistedLegacyArtworkResolves(){ThemeRepository r;auto theme=r.themes().first().toMap();theme["id"]="custom-legacy-preview";theme["name"]="Legacy preview";theme["preview"]="../assets/emerald-forest-portrait.jpg";r.saveDraft(theme);ThemeRepository restored;QCOMPARE(restored.themes().size(),9);QCOMPARE(restored.themes().last().toMap().value("previewUrl").toString(),QString("qrc:/assets/emerald-forest-portrait.jpg"));}
 void saveAndPersist(){ThemeRepository r;QCOMPARE(r.themes().size(),8);r.setSelected(0);r.createTheme();auto d=r.draft();d["name"]="Persisted Test";r.saveDraft(d);QVERIFY(!r.editing());QCOMPARE(r.themes().size(),9);ThemeRepository second;QCOMPARE(second.themes().size(),9);QCOMPARE(second.themes().last().toMap().value("name").toString(),QString("Persisted Test"));}
 void importExportRoundTrip(){ThemeRepository r;QTemporaryDir tmp;QString path=tmp.filePath("theme.json");QVERIFY(r.exportFile(path));int old=r.themes().size();QVERIFY(r.importFile(path));QCOMPARE(r.themes().size(),old+1);QCOMPARE(r.themes().last().toMap().value("name"),r.themes().first().toMap().value("name"));}
 void invalidImportRejected(){ThemeRepository r;QTemporaryDir tmp;QString path=tmp.filePath("invalid.json");QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("{\"schemaVersion\":99}");f.close();int old=r.themes().size();QVERIFY(!r.importFile(path));QCOMPARE(r.themes().size(),old);}
 void bundledCannotDelete(){ThemeRepository r;QCOMPARE(r.themes().size(),8);r.setSelected(0);r.deleteSelected();QCOMPARE(r.themes().size(),8);}
 void applicationPreservesAndRestores(){
  QTemporaryDir root;QVERIFY(root.isValid());const QString configDir=root.path()+"/config/alacritty";QVERIFY(QDir().mkpath(configDir));
  const QString initial="[window]\nopacity = 0.92\n\n[colors.primary]\nforeground = \"#ffffff\"\nbackground = \"#000000\"\n\n[colors.normal]\nblack = \"#000000\"\n";
  QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));config.write(initial.toUtf8());config.close();
  ThemeApplication app(root.path()+"/config",root.path()+"/data");QVariantMap theme=ThemeRepository().themes().first().toMap();
  const auto outcomes=app.apply(theme);QVERIFY(outcomes.first().toMap().value("success").toBool());QVERIFY(app.hasRestorePoint());
  QVERIFY(config.open(QIODevice::ReadOnly));const QByteArray applied=config.readAll();config.close();QVERIFY(applied.contains("opacity = 0.92"));QVERIFY(applied.contains("import ="));QVERIFY(!applied.contains("[colors.primary]"));QCOMPARE(applied.count("import ="),1);
  QFile managed(configDir+"/aurora-studio-colors.toml");QVERIFY(managed.open(QIODevice::ReadOnly));QVERIFY(managed.readAll().contains("[colors.bright]"));
  QString message;QVERIFY(app.restore(&message));QVERIFY(message.contains("verified"));QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),initial.toUtf8());QVERIFY(!managed.exists());
 }
 void repeatedApplyKeepsOriginalRestorePoint(){
  QTemporaryDir root;const QString configDir=root.path()+"/config/alacritty";QVERIFY(QDir().mkpath(configDir));QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));const QByteArray initial="[window]\nopacity = 0.8\n[colors.primary]\nbackground=\"#000000\"\n";config.write(initial);config.close();
  ThemeApplication app(root.path()+"/config",root.path()+"/data");ThemeRepository repo;QVERIFY(app.apply(repo.themes().first().toMap()).first().toMap().value("success").toBool());QVERIFY(app.apply(repo.themes().last().toMap()).first().toMap().value("success").toBool());
  QString error;QVERIFY(app.restore(&error));QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),initial);
 }
 void invalidThemeRejectedWithoutWrite(){
  QTemporaryDir root;const QString configDir=root.path()+"/config/alacritty";QVERIFY(QDir().mkpath(configDir));QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));const QByteArray initial="[window]\nopacity = 0.8\n";config.write(initial);config.close();
  ThemeRepository repo;QVariantMap theme=repo.themes().first().toMap();auto terminal=theme.value("terminal").toMap();terminal["cursor"]="not-a-color";theme["terminal"]=terminal;ThemeApplication app(root.path()+"/config",root.path()+"/data");const auto outcome=app.apply(theme);
  QVERIFY(!outcome.first().toMap().value("success").toBool());QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),initial);QVERIFY(!app.hasRestorePoint());
 }
 void missingConfigIsReported(){QTemporaryDir root;ThemeApplication app(root.path()+"/config",root.path()+"/data");QVariantMap status=app.status().first().toMap();QVERIFY(status.value("supported").toBool());QVERIFY(!status.value("success").toBool());QVERIFY(status.value("message").toString().contains("not found"));QVERIFY(!app.apply(ThemeRepository().themes().first().toMap()).first().toMap().value("success").toBool());}
 void existingImportIsPreservedAndRejected(){QTemporaryDir root;const QString configDir=root.path()+"/config/alacritty";QVERIFY(QDir().mkpath(configDir));QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));const QByteArray initial="import = [\"another-theme.toml\"]\n[window]\nopacity = 0.9\n";config.write(initial);config.close();ThemeApplication app(root.path()+"/config",root.path()+"/data");auto result=app.apply(ThemeRepository().themes().first().toMap()).first().toMap();QVERIFY(!result.value("success").toBool());QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),initial);QVERIFY(!QFileInfo::exists(configDir+"/aurora-studio-colors.toml"));}
 void restoreProtectsEditsMadeAfterApply(){QTemporaryDir root;const QString configDir=root.path()+"/config/alacritty";QVERIFY(QDir().mkpath(configDir));QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));config.write("[window]\nopacity=0.8\n");config.close();ThemeApplication app(root.path()+"/config",root.path()+"/data");QVERIFY(app.apply(ThemeRepository().themes().first().toMap()).first().toMap().value("success").toBool());QVERIFY(config.open(QIODevice::Append));config.write("# changed by user later\n");config.close();QString error;QVERIFY(!app.restore(&error));QVERIFY(error.contains("changed after Apply"));}
 void interruptedTransactionRecovers(){
  QTemporaryDir root;const QString configDir=root.path()+"/config/alacritty";const QString dataDir=root.path()+"/data";QVERIFY(QDir().mkpath(configDir));QVERIFY(QDir().mkpath(dataDir));
  QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));config.write("partially applied");config.close();const QByteArray original="[window]\nopacity = 0.7\n";
  QFile backup(dataDir+"/transaction-before-alacritty.toml");QVERIFY(backup.open(QIODevice::WriteOnly));backup.write(original);backup.close();
  QFile restore(dataDir+"/previous-alacritty.toml");QVERIFY(restore.open(QIODevice::WriteOnly));restore.write("first known-good");restore.close();
  QJsonObject previous{{"state","applied"},{"configPath",config.fileName()},{"includePath",configDir+"/aurora-studio-colors.toml"},{"baselineIncludeExisted",false}};
  QJsonObject journal{{"state","pending"},{"configPath",config.fileName()},{"includePath",configDir+"/aurora-studio-colors.toml"},{"includeExisted",false},{"rollbackPath",backup.fileName()},{"existed",true},{"previousJournal",previous}};QFile state(dataDir+"/theme-application.json");QVERIFY(state.open(QIODevice::WriteOnly));state.write(QJsonDocument(journal).toJson(QJsonDocument::Compact));state.close();
  ThemeApplication app(root.path()+"/config",dataDir);QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),original);QVERIFY(app.hasRestorePoint());QVERIFY(app.apply(ThemeRepository().themes().first().toMap()).first().toMap().value("success").toBool());
 }
 void unsupportedAndMissingAssetsAreExplicit(){
  QTemporaryDir root;const QString configDir=root.path()+"/config/alacritty";QVERIFY(QDir().mkpath(configDir));QFile config(configDir+"/alacritty.toml");QVERIFY(config.open(QIODevice::WriteOnly));config.write("[window]\nopacity = 0.9\n");config.close();
  ThemeRepository repo;QVariantMap theme=repo.themes().first().toMap();theme["wallpaper"]="../../missing-wallpaper.svg";theme["preview"]="../../missing-preview.jpg";ThemeApplication app(root.path()+"/config",root.path()+"/data");
  QVERIFY(app.apply(theme).first().toMap().value("success").toBool());const auto statuses=app.status();QVERIFY(!statuses[1].toMap().value("supported").toBool());QVERIFY(!statuses[2].toMap().value("supported").toBool());QVERIFY(statuses[2].toMap().value("message").toString().contains("Unsupported"));
 }
private:std::unique_ptr<QTemporaryDir>m_data;
};
QTEST_MAIN(RepositoryTests)
#include "theme_repository_test.moc"
