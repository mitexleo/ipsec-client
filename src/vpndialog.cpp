#include "vpndialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QDialogButtonBox>

VpnDialog::VpnDialog(Mode mode, QWidget *parent,
                     const QMap<QString, QString> &connectionData)
    : QDialog(parent)
    , m_mode(mode)
    , m_connectionData(connectionData)
{
    setWindowTitle(mode == Add ? "Add Connection" : "Edit Connection");

    auto *layout = new QFormLayout(this);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Connection name");
    layout->addRow("Name:", m_nameEdit);

    m_gatewayEdit = new QLineEdit(this);
    m_gatewayEdit->setPlaceholderText("VPN gateway address");
    layout->addRow("Gateway:", m_gatewayEdit);

    m_authCombo = new QComboBox(this);
    m_authCombo->addItems({"PSK", "EAP"});
    layout->addRow("Auth method:", m_authCombo);

    m_pskEdit = new QLineEdit(this);
    m_pskEdit->setEchoMode(QLineEdit::Password);
    m_pskEdit->setPlaceholderText("Pre-shared key");
    layout->addRow("PSK secret:", m_pskEdit);

    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText("Username (xauth)");
    layout->addRow("Username:", m_userEdit);

    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_passEdit->setPlaceholderText("Password");
    layout->addRow("Password:", m_passEdit);

    m_ikeEdit = new QLineEdit("aes256-sha512-modp2048", this);
    layout->addRow("IKE cipher:", m_ikeEdit);

    m_espEdit = new QLineEdit("aes256-sha512-modp2048", this);
    layout->addRow("ESP cipher:", m_espEdit);

    m_aggressiveCheck = new QCheckBox("Aggressive mode", this);
    m_aggressiveCheck->setChecked(true);
    layout->addRow(m_aggressiveCheck);

    m_encapCheck = new QCheckBox("Enable encapsulation", this);
    m_encapCheck->setChecked(true);
    layout->addRow(m_encapCheck);

    m_virtualCheck = new QCheckBox("Virtual adapter", this);
    m_virtualCheck->setChecked(true);
    layout->addRow(m_virtualCheck);

    m_subnetEdit = new QLineEdit(this);
    m_subnetEdit->setPlaceholderText("0.0.0.0/0");
    layout->addRow("Right subnet:", m_subnetEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_authCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &VpnDialog::onAuthChanged);
    connect(m_authCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { m_pskEdit->setVisible(m_authCombo->currentText() == "PSK"); });

    if (mode == Edit) {
        m_nameEdit->setText(connectionData.value("name"));
    }

    onAuthChanged();
}

void VpnDialog::onAuthChanged()
{
    bool isPsk = m_authCombo->currentText() == "PSK";
    m_pskEdit->setVisible(isPsk);
    layout()->invalidate();
}

void VpnDialog::accept()
{
    QString name = m_nameEdit->text().trimmed();
    QString gateway = m_gatewayEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Connection name is required.");
        return;
    }
    if (gateway.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Gateway address is required.");
        return;
    }

    QString method = m_authCombo->currentText().toLower();
    QString psk = m_pskEdit->text();
    QString user = m_userEdit->text();
    QString pass = m_passEdit->text();
    QString ike = m_ikeEdit->text();
    QString esp = m_espEdit->text();
    QString aggressive = m_aggressiveCheck->isChecked() ? "yes" : "no";
    QString encap = m_encapCheck->isChecked() ? "yes" : "no";
    QString virtual_ = m_virtualCheck->isChecked() ? "yes" : "no";
    QString subnet = m_subnetEdit->text().isEmpty() ? "0.0.0.0/0" : m_subnetEdit->text();

    QString vpnData = QString("address=%1, method=%2, psk=%3, leftxauthusername=%4, "
                              "aggressive=%5, keyexchange=ikev1, ike=%6, esp=%7, "
                              "encap=%8, virtual=%9, rightsubnet=%10")
                          .arg(gateway, method, psk, user,
                               aggressive, ike, esp,
                               encap, virtual_, subnet);

    QString vpnSecrets = QString("xauthpassword=%1").arg(pass);

    if (m_mode == Add) {
        QStringList args = {
            "connection", "add", "type", "vpn", "vpn-type", "strongswan",
            "connection.id", name,
            "vpn.data", vpnData,
            "vpn.secrets", vpnSecrets
        };

        QProcess proc;
        proc.start("nmcli", args);
        proc.waitForFinished(10000);

        if (proc.exitCode() != 0) {
            QMessageBox::critical(this, "Error",
                QString("Failed to add connection:\n%1")
                    .arg(QString::fromUtf8(proc.readAllStandardError())));
            return;
        }
    } else {
        QString uuid = m_connectionData.value("uuid");
        QStringList args = {
            "connection", "modify", uuid,
            "vpn.data", vpnData,
            "vpn.secrets", vpnSecrets
        };

        QProcess proc;
        proc.start("nmcli", args);
        proc.waitForFinished(10000);

        if (proc.exitCode() != 0) {
            QMessageBox::critical(this, "Error",
                QString("Failed to modify connection:\n%1")
                    .arg(QString::fromUtf8(proc.readAllStandardError())));
            return;
        }
    }

    QDialog::accept();
}
