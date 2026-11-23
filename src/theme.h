#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class ThemeRepository : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList themes READ themes NOTIFY themesChanged)
    Q_PROPERTY(int selected READ selected WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY draftChanged)
    Q_PROPERTY(bool editing READ editing NOTIFY draftChanged)
public:
    explicit ThemeRepository(QObject *parent = nullptr);
    QVariantList themes() const;
    int selected() const { return m_selected; }
    void setSelected(int value);
    Q_INVOKABLE void applySelected();
    Q_INVOKABLE void exportSelected();
    Q_INVOKABLE void createTheme();
    QVariantMap draft() const { return m_draft; }
    bool editing() const { return m_editing; }
    Q_INVOKABLE void editSelected();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void saveDraft(const QVariantMap &value);
    Q_INVOKABLE void cancelDraft();
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void importTheme();
    Q_INVOKABLE void exportPreset();
    Q_INVOKABLE bool importFile(const QString &path);
    Q_INVOKABLE bool exportFile(const QString &path);
signals:
    void themesChanged();
    void selectedChanged();
    void notice(QString message);
    void draftChanged();
private:
    QVariantList m_themes;
    int m_selected = 0;
    QVariantMap m_draft;
    QString m_editingId;
    bool m_editing = false;
    bool validate(const QVariantMap &theme, QString *error) const;
    void reload();
    QString userDir() const;
};
