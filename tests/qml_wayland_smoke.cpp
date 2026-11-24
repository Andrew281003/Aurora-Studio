#include "../src/theme.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTemporaryDir>
#include <QTest>

class QmlWaylandSmoke : public QObject
{
    Q_OBJECT
private slots:
    void desktopFlows()
    {
        QVERIFY2(QGuiApplication::platformName().contains("wayland"),
                 qPrintable(QGuiApplication::platformName()));
        QTemporaryDir data;
        QVERIFY(data.isValid());
        qputenv("XDG_DATA_HOME", data.path().toUtf8());

        qmlRegisterType<ThemeRepository>("Aurora", 1, 0, "ThemeRepository");
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("launchScreen", QGuiApplication::primaryScreen());
        QStringList qmlWarnings;
        connect(&engine, &QQmlApplicationEngine::warnings, &engine,
                [&qmlWarnings](const QList<QQmlError> &errors) {
                    for (const auto &error : errors) qmlWarnings << error.toString();
                });
        QQmlComponent component(&engine, QUrl("qrc:/qml/Main.qml"));
        QVERIFY2(!component.isError(), qPrintable(component.errorString()));
        QScopedPointer<QObject> root(component.create());
        QVERIFY2(root, qPrintable(component.errorString()));
        auto *window = qobject_cast<QQuickWindow *>(root.data());
        QVERIFY(window);
        QTRY_VERIFY(window->isVisible());

        auto *repository = root->findChild<QObject *>("themeRepository");
        QVERIFY(repository);
        QCOMPARE(repository->property("selected").toInt(), 0);
        QVERIFY(QMetaObject::invokeMethod(root.data(), "moveSelection", Q_ARG(QVariant, 1)));
        QCOMPARE(repository->property("selected").toInt(), 1);
        QVERIFY(QMetaObject::invokeMethod(root.data(), "moveSelection", Q_ARG(QVariant, -1)));
        QCOMPARE(repository->property("selected").toInt(), 0);

        const QString outputDir = QDir::current().filePath("screenshots");
        QVERIFY(QDir().mkpath(outputDir));
        QTest::qWait(450);
        QVERIFY2(window->grabWindow().save(outputDir + "/aurora-studio-gallery.png"), "gallery screenshot failed");

        auto openAndClose = [&root](const char *objectName, const char *buttonName, const QString &screenshot) {
            QObject *dialog = root->findChild<QObject *>(QString::fromLatin1(objectName));
            if (!dialog) return false;
            if (!QMetaObject::invokeMethod(dialog, "open")) return false;
            for (int i = 0; i < 20 && !dialog->property("visible").toBool(); ++i) QTest::qWait(25);
            if (!dialog->property("visible").toBool()) return false;
            auto *window = qobject_cast<QQuickWindow *>(root.data());
            if (!window->grabWindow().save(QDir::current().filePath("screenshots/") + screenshot)) return false;
            QObject *button = root->findChild<QObject *>(QString::fromLatin1(buttonName));
            return button && QMetaObject::invokeMethod(button, "click");
        };
        QVERIFY(openAndClose("previewDialog", "previewCloseButton", "aurora-studio-preview.png"));
        QVERIFY(openAndClose("applyDialog", "applyCancelButton", "aurora-studio-apply-confirmation.png"));

        QVERIFY(QMetaObject::invokeMethod(root.data(), "beginEdit"));
        QCOMPARE(root->property("page").toString(), QString("editor"));
        QTest::qWait(250);
        QVERIFY(window->grabWindow().save(outputDir + "/aurora-studio-editor.png"));
        QVariantMap edit = root->property("edit").toMap();
        edit["name"] = "Unsaved smoke test";
        root->setProperty("edit", edit);
        QVERIFY(QMetaObject::invokeMethod(root.data(), "cancelEdit"));
        QObject *discard = root->findChild<QObject *>("discardDialog");
        QVERIFY(discard && discard->property("visible").toBool());
        QVERIFY(window->grabWindow().save(outputDir + "/aurora-studio-discard-confirmation.png"));
        QObject *keepEditing = root->findChild<QObject *>("discardKeepButton");
        QVERIFY(keepEditing && QMetaObject::invokeMethod(keepEditing, "click"));
        QCOMPARE(root->property("page").toString(), QString("editor"));
        QVERIFY(QMetaObject::invokeMethod(root.data(), "cancelEdit"));
        QObject *discardButton = root->findChild<QObject *>("discardButton");
        QVERIFY(discardButton && QMetaObject::invokeMethod(discardButton, "click"));
        QCOMPARE(root->property("page").toString(), QString("gallery"));

        root->setProperty("page", "presets");
        QCOMPARE(root->property("page").toString(), QString("presets"));
        QTest::qWait(150);
        QVERIFY(window->grabWindow().save(outputDir + "/aurora-studio-presets.png"));
        root->setProperty("page", "gallery");
        QVERIFY(QMetaObject::invokeMethod(root.data(), "requestClose"));
        QTRY_VERIFY(!window->isVisible());
        QVERIFY2(qmlWarnings.filter("Binding loop detected").isEmpty(),
                 qPrintable(qmlWarnings.join('\n')));
    }
};

QTEST_MAIN(QmlWaylandSmoke)
#include "qml_wayland_smoke.moc"
