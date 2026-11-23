#include "theme.h"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
int main(int argc,char**argv){
    QGuiApplication app(argc,argv);
    app.setApplicationName("aurora-studio");
    app.setApplicationDisplayName("Aurora Studio");
    app.setDesktopFileName("aurora-studio");
    qmlRegisterType<ThemeRepository>("Aurora",1,0,"ThemeRepository");
    QQmlApplicationEngine e;
    QScreen *launchScreen=QGuiApplication::screenAt(QCursor::pos());
    if(!launchScreen)launchScreen=QGuiApplication::primaryScreen();
    e.rootContext()->setContextProperty("launchScreen",launchScreen);
    QObject::connect(&e,&QQmlApplicationEngine::warnings,[](const QList<QQmlError>&errors){for(const auto&x:errors)qWarning().noquote()<<x.toString();});
    QQmlComponent component(&e,QUrl("qrc:/qml/Main.qml"));
    if(component.isError()){
        for(const auto &error:component.errors())qWarning().noquote()<<error.toString();
        return 1;
    }
    QObject *root=component.create();
    if(!root){
        for(const auto &error:component.errors())qWarning().noquote()<<error.toString();
        return 1;
    }
    return app.exec();
}
