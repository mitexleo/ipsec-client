#ifndef VPNDIALOG_H
#define VPNDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QMap>
#include <QString>

class VpnDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Add, Edit };

    explicit VpnDialog(Mode mode, QWidget *parent = nullptr,
                       const QMap<QString, QString> &connectionData = {},
                       const QString &nmcliPrefix = {});

private:
    void setupAuthVisibility();

private:
    void accept() override;

    void runNmcli(const QStringList &args);

    Mode m_mode;
    QLineEdit *m_nameEdit;
    QLineEdit *m_gatewayEdit;
    QLineEdit *m_pskEdit;
    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;
    QLineEdit *m_ikeEdit;
    QLineEdit *m_espEdit;
    QLineEdit *m_subnetEdit;
    QComboBox *m_authCombo;
    QCheckBox *m_aggressiveCheck;
    QCheckBox *m_encapCheck;
    QCheckBox *m_virtualCheck;
    QMap<QString, QString> m_connectionData;
    QString m_nmcliPrefix;
};

#endif
