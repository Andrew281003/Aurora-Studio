#pragma once

#include <QVariantList>
#include <QVariantMap>

class ThemeApplication {
public:
    explicit ThemeApplication(QString configRoot = {}, QString dataRoot = {});
    QVariantList apply(const QVariantMap &theme);
    bool restore(QString *message = nullptr);
    QVariantList status() const;
    bool hasRestorePoint() const;
    QString recoveryMessage() const { return m_recoveryMessage; }

private:
    QString m_configRoot;
    QString m_dataRoot;
    QString m_recoveryMessage;
    QString journalPath() const;
    QString backupPath() const;
    QString transactionBackupPath() const;
    bool recoverInterrupted();
};
