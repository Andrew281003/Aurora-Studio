#include "../src/theme.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class RepositoryTests:public QObject{Q_OBJECT
private slots:
 void initTestCase(){m_data=std::make_unique<QTemporaryDir>();qputenv("XDG_DATA_HOME",m_data->path().toUtf8());}
 void init(){m_data=std::make_unique<QTemporaryDir>();qputenv("XDG_DATA_HOME",m_data->path().toUtf8());}
 void galleryLoadsEight(){ThemeRepository r;QCOMPARE(r.themes().size(),8);}
 void saveAndPersist(){ThemeRepository r;QCOMPARE(r.themes().size(),8);r.setSelected(0);r.createTheme();auto d=r.draft();d["name"]="Persisted Test";r.saveDraft(d);QVERIFY(!r.editing());QCOMPARE(r.themes().size(),9);ThemeRepository second;QCOMPARE(second.themes().size(),9);QCOMPARE(second.themes().last().toMap().value("name").toString(),QString("Persisted Test"));}
 void importExportRoundTrip(){ThemeRepository r;QTemporaryDir tmp;QString path=tmp.filePath("theme.json");QVERIFY(r.exportFile(path));int old=r.themes().size();QVERIFY(r.importFile(path));QCOMPARE(r.themes().size(),old+1);QCOMPARE(r.themes().last().toMap().value("name"),r.themes().first().toMap().value("name"));}
 void invalidImportRejected(){ThemeRepository r;QTemporaryDir tmp;QString path=tmp.filePath("invalid.json");QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("{\"schemaVersion\":99}");f.close();int old=r.themes().size();QVERIFY(!r.importFile(path));QCOMPARE(r.themes().size(),old);}
 void bundledCannotDelete(){ThemeRepository r;QCOMPARE(r.themes().size(),8);r.setSelected(0);r.deleteSelected();QCOMPARE(r.themes().size(),8);}
private:std::unique_ptr<QTemporaryDir>m_data;
};
QTEST_MAIN(RepositoryTests)
#include "theme_repository_test.moc"
