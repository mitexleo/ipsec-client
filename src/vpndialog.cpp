#include "vpndialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QDialogButtonBox>

// ---------------------------------------------------------------------------
// Parsing helpers for connection data (used when editing)
// ---------------------------------------------------------------------------

QMap<QString, QString> VpnDialog::parseVpnData(const QString &vpnData)
{
    // vpn.data is comma-separated key=value pairs:
    //   address=X, method=psk, psk=Y, leftxauthusername=Z, ...
    // or for vpnc:
    //   gateway=X, user=Y, group-name=Z, ike-dhgroup=dh14, ...
    QMap<QString, QString> result;
    const QStringList pairs = vpnData.split(',', Qt::SkipEmptyParts);
    for (const QString &pair : pairs) {
        int eq = pair.indexOf('=');
        if (eq < 0)
            continue;
        QString key = pair.left(eq).trimmed();
        QString val = pair.mid(eq + 1).trimmed();
        result[key] = val;
    }
    return result;
}

QMap<QString, QString> VpnDialog::parseVpnSecrets(const QString &vpnSecrets)
{
    // Same comma-separated key=value format:
    //   xauthpassword=SECRET
    // or for vpnc:
    //   password=SECRET, group-secret=SECRET2
    return parseVpnData(vpnSecrets);
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

VpnDialog::VpnDialog(Mode mode, QWidget *parent,
                     const QMap<QString, QString> &connectionData,
                     const QString &nmcliPrefix)
    : QDialog(parent)
    , m_mode(mode)
    , m_nmcliPrefix(nmcliPrefix)
    , m_connectionData(connectionData)
{
    setWindowTitle(mode == Add ? "Add Connection" : "Edit Connection");
    setMinimumWidth(420);

    auto *mainLayout = new QVBoxLayout(this);

    // -- Backend selector row --
    auto *backendRow = new QHBoxLayout;
    backendRow->addWidget(new QLabel("Backend:", this));
    m_backendCombo = new QComboBox(this);
    m_backendCombo->addItems({"strongSwan", "Cisco (vpnc)"});
    backendRow->addWidget(m_backendCombo, 1);
    mainLayout->addLayout(backendRow);

    // -- Shared fields (Name, Gateway) --
    auto *sharedForm = new QFormLayout;
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Connection name");
    sharedForm->addRow("Name:", m_nameEdit);

    m_gatewayEdit = new QLineEdit(this);
    m_gatewayEdit->setPlaceholderText("VPN gateway address");
    sharedForm->addRow("Gateway:", m_gatewayEdit);
    mainLayout->addLayout(sharedForm);

    // -- Backend-specific stacked pages --
    m_backendStack = new QStackedWidget(this);

    // -------- strongSwan page --------
    auto *ssPage = new QWidget;
    auto *ssForm = new QFormLayout(ssPage);

    m_ssAuthCombo = new QComboBox(ssPage);
    m_ssAuthCombo->addItems({"PSK", "EAP"});
    ssForm->addRow("Auth method:", m_ssAuthCombo);

    m_ssPskEdit = new QLineEdit(ssPage);
    m_ssPskEdit->setEchoMode(QLineEdit::Password);
    m_ssPskEdit->setPlaceholderText("Pre-shared key");
    ssForm->addRow("PSK secret:", m_ssPskEdit);

    m_ssUserEdit = new QLineEdit(ssPage);
    m_ssUserEdit->setPlaceholderText("Username (xauth)");
    ssForm->addRow("Username:", m_ssUserEdit);

    m_ssPassEdit = new QLineEdit(ssPage);
    m_ssPassEdit->setEchoMode(QLineEdit::Password);
    m_ssPassEdit->setPlaceholderText("Password");
    ssForm->addRow("Password:", m_ssPassEdit);

    m_ssIkeEdit = new QLineEdit("aes256-sha512-modp2048", ssPage);
    ssForm->addRow("IKE cipher:", m_ssIkeEdit);

    m_ssEspEdit = new QLineEdit("aes256-sha512-modp2048", ssPage);
    ssForm->addRow("ESP cipher:", m_ssEspEdit);

    m_ssAggressiveCheck = new QCheckBox("Aggressive mode", ssPage);
    m_ssAggressiveCheck->setChecked(false);
    ssForm->addRow(m_ssAggressiveCheck);

    m_ssEncapCheck = new QCheckBox("Enable encapsulation", ssPage);
    m_ssEncapCheck->setChecked(true);
    ssForm->addRow(m_ssEncapCheck);

    m_ssVirtualCheck = new QCheckBox("Virtual adapter", ssPage);
    m_ssVirtualCheck->setChecked(true);
    ssForm->addRow(m_ssVirtualCheck);

    m_ssSubnetEdit = new QLineEdit(ssPage);
    m_ssSubnetEdit->setPlaceholderText("0.0.0.0/0");
    ssForm->addRow("Right subnet:", m_ssSubnetEdit);

    m_backendStack->addWidget(ssPage);

    // -------- Cisco (vpnc) page --------
    auto *vpncPage = new QWidget;
    auto *vpncForm = new QFormLayout(vpncPage);

    m_vpncUserEdit = new QLineEdit(vpncPage);
    m_vpncUserEdit->setPlaceholderText("Xauth username (also used as group name)");
    vpncForm->addRow("Username:", m_vpncUserEdit);

    m_vpncGroupNameEdit = new QLineEdit(vpncPage);
    m_vpncGroupNameEdit->setPlaceholderText("Leave blank to use username");
    vpncForm->addRow("Group name:", m_vpncGroupNameEdit);

    m_vpncGroupSecretEdit = new QLineEdit(vpncPage);
    m_vpncGroupSecretEdit->setEchoMode(QLineEdit::Password);
    m_vpncGroupSecretEdit->setPlaceholderText("Group PSK / pre-shared key");
    vpncForm->addRow("Group secret:", m_vpncGroupSecretEdit);

    m_vpncPassEdit = new QLineEdit(vpncPage);
    m_vpncPassEdit->setEchoMode(QLineEdit::Password);
    m_vpncPassEdit->setPlaceholderText("User XAUTH password");
    vpncForm->addRow("Password:", m_vpncPassEdit);

    m_vpncIkeDhgroupCombo = new QComboBox(vpncPage);
    m_vpncIkeDhgroupCombo->addItems({"dh1", "dh2", "dh5", "dh14", "dh15", "dh16", "dh17", "dh18", "dh19", "dh20", "dh21", "dh22", "dh23", "dh24"});
    m_vpncIkeDhgroupCombo->setCurrentText("dh14");
    vpncForm->addRow("IKE DH group:", m_vpncIkeDhgroupCombo);

    m_vpncPfsCombo = new QComboBox(vpncPage);
    m_vpncPfsCombo->addItems({"dh1", "dh2", "dh5", "dh14", "dh15", "dh16", "dh17", "dh18", "dh19", "dh20", "dh21", "dh22", "dh23", "dh24", "server"});
    m_vpncPfsCombo->setCurrentText("dh14");
    vpncForm->addRow("PFS DH group:", m_vpncPfsCombo);

    m_vpncPfsModeCombo = new QComboBox(vpncPage);
    m_vpncPfsModeCombo->addItems({"server", "required", "nopfs"});
    m_vpncPfsModeCombo->setCurrentText("server");
    vpncForm->addRow("PFS mode:", m_vpncPfsModeCombo);

    m_backendStack->addWidget(vpncPage);

    mainLayout->addWidget(m_backendStack, 1);

    // -- Button box --
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { setupBackendVisibility(); });
    connect(m_ssAuthCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { m_ssPskEdit->setVisible(m_ssAuthCombo->currentText() == "PSK"); });

    // Populate fields from connection data (for Add via import or Edit)
    populateFromConnectionData();
    setupBackendVisibility();
}

// ---------------------------------------------------------------------------
// Show/hide backend-specific fields
// ---------------------------------------------------------------------------

void VpnDialog::setupBackendVisibility()
{
    m_backendStack->setCurrentIndex(m_backendCombo->currentIndex());
}

// ---------------------------------------------------------------------------
// Populate fields from m_connectionData (used by import and edit)
// ---------------------------------------------------------------------------

void VpnDialog::populateFromConnectionData()
{
    QString serviceType = m_connectionData.value("service-type");

    // Detect backend from service type (only relevant when editing)
    if (serviceType.contains("vpnc"))
        m_backendCombo->setCurrentIndex(BackendVpnc);
    else if (serviceType.contains("strongswan"))
        m_backendCombo->setCurrentIndex(BackendStrongSwan);

    // Shared fields
    if (m_connectionData.contains("name"))
        m_nameEdit->setText(m_connectionData.value("name"));
    if (m_connectionData.contains("gateway") || m_connectionData.contains("address")) {
        QString gw = m_connectionData.value("gateway");
        if (gw.isEmpty()) gw = m_connectionData.value("address");
        m_gatewayEdit->setText(gw);
    }

    // If we have raw vpn.data / vpn.secrets (from edit), parse them
    QString rawData = m_connectionData.value("vpn.data");
    QString rawSecrets = m_connectionData.value("vpn.secrets");

    if (!rawData.isEmpty()) {
        QMap<QString, QString> data = parseVpnData(rawData);
        QMap<QString, QString> secrets = parseVpnSecrets(rawSecrets);

        // strongSwan fields
        if (data.contains("psk")) m_ssPskEdit->setText(data.value("psk"));
        if (data.contains("leftxauthusername")) m_ssUserEdit->setText(data.value("leftxauthusername"));
        if (data.contains("ike")) m_ssIkeEdit->setText(data.value("ike"));
        if (data.contains("esp")) m_ssEspEdit->setText(data.value("esp"));
        if (data.contains("rightsubnet")) m_ssSubnetEdit->setText(data.value("rightsubnet"));
        if (data.value("aggressive") == "yes") m_ssAggressiveCheck->setChecked(true);
        if (data.value("encap") == "yes") m_ssEncapCheck->setChecked(true);
        if (data.value("virtual") == "yes") m_ssVirtualCheck->setChecked(true);
        if (data.contains("method")) {
            int idx = m_ssAuthCombo->findText(data.value("method"), Qt::MatchFixedString);
            if (idx >= 0) m_ssAuthCombo->setCurrentIndex(idx);
        }
        if (secrets.contains("xauthpassword")) m_ssPassEdit->setText(secrets.value("xauthpassword"));
        if (secrets.contains("psk")) m_ssPskEdit->setText(secrets.value("psk"));

        // vpnc fields
        if (data.contains("user")) m_vpncUserEdit->setText(data.value("user"));
        if (data.contains("group-name")) m_vpncGroupNameEdit->setText(data.value("group-name"));
        if (data.contains("ike-dhgroup")) m_vpncIkeDhgroupCombo->setCurrentText(data.value("ike-dhgroup"));
        if (data.contains("pfs")) m_vpncPfsCombo->setCurrentText(data.value("pfs"));
        if (data.contains("perfect-forward-secrecy")) m_vpncPfsModeCombo->setCurrentText(data.value("perfect-forward-secrecy"));
        if (secrets.contains("group-secret")) m_vpncGroupSecretEdit->setText(secrets.value("group-secret"));
        if (secrets.contains("password")) m_vpncPassEdit->setText(secrets.value("password"));
    } else {
        // Direct field values (from import)
        if (m_connectionData.contains("psk")) m_ssPskEdit->setText(m_connectionData.value("psk"));
        if (m_connectionData.contains("username")) {
            m_ssUserEdit->setText(m_connectionData.value("username"));
            m_vpncUserEdit->setText(m_connectionData.value("username"));
        }
        if (m_connectionData.contains("password")) {
            m_ssPassEdit->setText(m_connectionData.value("password"));
            m_vpncPassEdit->setText(m_connectionData.value("password"));
        }
        if (m_connectionData.contains("ike")) m_ssIkeEdit->setText(m_connectionData.value("ike"));
        if (m_connectionData.contains("esp")) m_ssEspEdit->setText(m_connectionData.value("esp"));
        if (m_connectionData.contains("subnet")) m_ssSubnetEdit->setText(m_connectionData.value("subnet"));
        if (m_connectionData.value("aggressive") == "yes") m_ssAggressiveCheck->setChecked(true);
        if (m_connectionData.value("encap") == "yes") m_ssEncapCheck->setChecked(true);
        if (m_connectionData.value("virtual") == "yes") m_ssVirtualCheck->setChecked(true);
    }
}

// ---------------------------------------------------------------------------
// Accept — build and run the nmcli command
// ---------------------------------------------------------------------------

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

    if (m_backendCombo->currentIndex() == BackendVpnc) {
        // ---- Cisco (vpnc) backend ----
        QString user = m_vpncUserEdit->text();
        QString groupName = m_vpncGroupNameEdit->text();
        if (groupName.isEmpty()) groupName = user;
        QString groupSecret = m_vpncGroupSecretEdit->text();
        QString pass = m_vpncPassEdit->text();
        QString ikeDhgroup = m_vpncIkeDhgroupCombo->currentText();
        QString pfs = m_vpncPfsCombo->currentText();
        QString pfsMode = m_vpncPfsModeCombo->currentText();

        QString vpnData = QString("gateway=%1, user=%2, group-name=%3, "
                                  "perfect-forward-secrecy=%4, ike-dhgroup=%5, pfs=%6, vendor=cisco")
                              .arg(gateway, user, groupName, pfsMode, ikeDhgroup, pfs);

        QString vpnSecrets = QString("password=%1, group-secret=%2").arg(pass, groupSecret);

        if (m_mode == Add) {
            runNmcli({"connection", "add", "type", "vpn", "vpn-type", "vpnc",
                      "connection.id", name,
                      "vpn.data", vpnData,
                      "vpn.secrets", vpnSecrets});
        } else {
            QString uuid = m_connectionData.value("uuid");
            runNmcli({"connection", "modify", uuid,
                      "vpn.data", vpnData,
                      "vpn.secrets", vpnSecrets});
        }
    } else {
        // ---- strongSwan backend ----
        QString method = m_ssAuthCombo->currentText().toLower();
        QString psk = m_ssPskEdit->text();
        QString user = m_ssUserEdit->text();
        QString pass = m_ssPassEdit->text();
        QString ike = m_ssIkeEdit->text();
        QString esp = m_ssEspEdit->text();
        QString aggressive = m_ssAggressiveCheck->isChecked() ? "yes" : "no";
        QString encap = m_ssEncapCheck->isChecked() ? "yes" : "no";
        QString virtual_ = m_ssVirtualCheck->isChecked() ? "yes" : "no";
        QString subnet = m_ssSubnetEdit->text().isEmpty() ? "0.0.0.0/0" : m_ssSubnetEdit->text();

        QString vpnData = QString("address=%1, method=%2, psk=%3, leftxauthusername=%4, "
                                  "aggressive=%5, keyexchange=ikev1, ike=%6, esp=%7, "
                                  "encap=%8, virtual=%9, rightsubnet=%10")
                              .arg(gateway, method, psk, user,
                                   aggressive, ike, esp,
                                   encap, virtual_, subnet);

        QString vpnSecrets = QString("xauthpassword=%1").arg(pass);

        if (m_mode == Add) {
            runNmcli({"connection", "add", "type", "vpn", "vpn-type", "strongswan",
                      "connection.id", name,
                      "vpn.data", vpnData,
                      "vpn.secrets", vpnSecrets});
        } else {
            QString uuid = m_connectionData.value("uuid");
            runNmcli({"connection", "modify", uuid,
                      "vpn.data", vpnData,
                      "vpn.secrets", vpnSecrets});
        }
    }

    QDialog::accept();
}

// ---------------------------------------------------------------------------
// Run nmcli (with optional flatpak-spawn prefix) and report errors
// ---------------------------------------------------------------------------

void VpnDialog::runNmcli(const QStringList &args)
{
    QProcess proc;
    if (m_nmcliPrefix.isEmpty())
        proc.start("nmcli", args);
    else
        proc.start("flatpak-spawn", QStringList{"--host", "nmcli"} + args);
    proc.waitForFinished(10000);

    if (proc.exitCode() != 0) {
        QString action = m_mode == Add ? "add" : "modify";
        QMessageBox::critical(this, "Error",
            QString("Failed to %1 connection:\n%2")
                .arg(action, QString::fromUtf8(proc.readAllStandardError())));
    }
}
