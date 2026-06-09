#ifndef VPNDIALOG_H
#define VPNDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QMap>
#include <QString>
#include <QStackedWidget>
#include <QLabel>

class VpnDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Add, Edit };
    enum Backend { BackendStrongSwan = 0, BackendVpnc };

    explicit VpnDialog(Mode mode, QWidget *parent = nullptr,
                       const QMap<QString, QString> &connectionData = {},
                       const QString &nmcliPrefix = {});

private:
    void setupBackendVisibility();
    void accept() override;
    void runNmcli(const QStringList &args);
    void populateFromConnectionData();

    // Parsing helpers
    static QMap<QString, QString> parseVpnData(const QString &vpnData);
    static QMap<QString, QString> parseVpnSecrets(const QString &vpnSecrets);

    Mode m_mode;
    QString m_nmcliPrefix;
    QMap<QString, QString> m_connectionData;

    // Backend
    QComboBox *m_backendCombo;
    QStackedWidget *m_backendStack;

    // Shared
    QLineEdit *m_nameEdit;
    QLineEdit *m_gatewayEdit;

    // ---- strongSwan fields ----
    QComboBox *m_ssAuthCombo;
    QLineEdit *m_ssPskEdit;
    QLineEdit *m_ssUserEdit;
    QLineEdit *m_ssPassEdit;
    QLineEdit *m_ssIkeEdit;
    QLineEdit *m_ssEspEdit;
    QLineEdit *m_ssSubnetEdit;
    QCheckBox *m_ssAggressiveCheck;
    QCheckBox *m_ssEncapCheck;
    QCheckBox *m_ssVirtualCheck;

    // ---- Cisco (vpnc) fields ----
    QLineEdit *m_vpncUserEdit;
    QLineEdit *m_vpncGroupNameEdit;
    QLineEdit *m_vpncGroupSecretEdit;
    QLineEdit *m_vpncPassEdit;
    QComboBox *m_vpncIkeDhgroupCombo;
    QComboBox *m_vpncPfsCombo;
    QComboBox *m_vpncPfsModeCombo;
};

#endif // VPNDIALOG_H
