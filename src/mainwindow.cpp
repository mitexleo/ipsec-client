#include "mainwindow.h"
#include "vpndialog.h"

#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QTextStream>
#include "version.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_connectionList(new QListWidget(this))
    , m_addBtn(new QPushButton("Add", this))
    , m_editBtn(new QPushButton("Edit", this))
    , m_deleteBtn(new QPushButton("Delete", this))
    , m_importBtn(new QPushButton("Import", this))
    , m_helpBtn(new QPushButton("Help", this))
    , m_connectBtn(new QPushButton("Connect", this))
    , m_statusLabel(new QLabel("Ready", this))
    , m_process(new QProcess(this))
{
    setWindowTitle("IPsec Client");
    setMinimumSize(640, 480);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(centralWidget);

    auto *leftContainer = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_placeholderLabel = new QLabel("No VPN connections found.\nUse Add or Import to create one.", this);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setStyleSheet("color: gray; font-size: 14px;");
    leftLayout->addWidget(m_placeholderLabel);
    leftLayout->addWidget(m_connectionList);
    mainLayout->addWidget(leftContainer, 3);

    auto *rightLayout = new QVBoxLayout;
    m_statusLabel->setWordWrap(true);
    rightLayout->addWidget(m_statusLabel);
    rightLayout->addStretch();
    rightLayout->addWidget(m_connectBtn);
    rightLayout->addWidget(m_addBtn);
    rightLayout->addWidget(m_editBtn);
    rightLayout->addWidget(m_deleteBtn);
    rightLayout->addWidget(m_importBtn);
    rightLayout->addWidget(m_helpBtn);
    mainLayout->addLayout(rightLayout);

    setCentralWidget(centralWidget);

    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_connectBtn->setEnabled(false);

    connect(m_addBtn, &QPushButton::clicked, this, &MainWindow::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &MainWindow::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MainWindow::onDelete);
    connect(m_importBtn, &QPushButton::clicked, this, &MainWindow::onImport);
    connect(m_helpBtn, &QPushButton::clicked, this, &MainWindow::onHelp);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectToggle);
    connect(m_connectionList, &QListWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onProcessFinished);

    refreshList();
}

void MainWindow::refreshList()
{
    m_connectionList->clear();

    if (m_nmcliPrefix.isEmpty()) {
        QProcess test;
        test.start("nmcli", {"--version"});
        test.waitForFinished(3000);
        if (test.exitCode() != 0) {
            QProcess test2;
            test2.start("flatpak-spawn", {"--host", "nmcli", "--version"});
            test2.waitForFinished(3000);
            if (test2.exitCode() == 0)
                m_nmcliPrefix = "flatpak-spawn --host";
        }
        if (m_nmcliPrefix.isEmpty() && test.exitCode() != 0) {
            m_placeholderLabel->setText("nmcli not found.\nInstall NetworkManager network-manager-strongswan strongswan on the host system");
            m_placeholderLabel->show();
            m_connectionList->hide();
            return;
        }
    }

    QStringList activeUuids;
    QProcess activeProc;
    if (m_nmcliPrefix.isEmpty())
        activeProc.start("nmcli", {"-t", "-f", "NAME,UUID,TYPE,DEVICE", "con", "show", "--active"});
    else
        activeProc.start("flatpak-spawn", {"--host", "nmcli", "-t", "-f", "NAME,UUID,TYPE,DEVICE", "con", "show", "--active"});
    activeProc.waitForFinished(5000);
    QString activeOutput = QString::fromUtf8(activeProc.readAllStandardOutput());
    for (const auto &line : activeOutput.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(':');
        if (parts.size() >= 2)
            activeUuids.append(parts[1]);
    }

    QProcess proc;
    if (m_nmcliPrefix.isEmpty())
        proc.start("nmcli", {"-t", "-f", "NAME,UUID,TYPE,DEVICE", "con", "show"});
    else
        proc.start("flatpak-spawn", {"--host", "nmcli", "-t", "-f", "NAME,UUID,TYPE,DEVICE", "con", "show"});
    proc.waitForFinished(5000);
    QString output = QString::fromUtf8(proc.readAllStandardOutput());

    for (const auto &line : output.split('\n', Qt::SkipEmptyParts)) {
        if (!line.contains(":strongswan:"))
            continue;
        QStringList parts = line.split(':');
        if (parts.size() < 2)
            continue;
        QString name = parts[0];
        QString uuid = parts[1];
        bool isActive = activeUuids.contains(uuid);
        QString display = isActive ? "(Connected) " + name : name;
        auto *item = new QListWidgetItem(display, m_connectionList);
        item->setData(Qt::UserRole, uuid);
    }

    bool empty = m_connectionList->count() == 0;
    m_placeholderLabel->setText("No VPN connections found.\nUse Add or Import to create one.");
    m_placeholderLabel->setVisible(empty);
    m_connectionList->setVisible(!empty);
}

void MainWindow::onAdd()
{
    VpnDialog dlg(VpnDialog::Add, this, {}, m_nmcliPrefix);
    if (dlg.exec() == QDialog::Accepted)
        refreshList();
}

void MainWindow::onEdit()
{
    if (m_selectedUuid.isEmpty())
        return;

    auto *item = m_connectionList->currentItem();
    if (!item)
        return;

    QMap<QString, QString> data;
    data["name"] = item->text().remove(QRegularExpression("^\\(Connected\\) "));
    data["uuid"] = m_selectedUuid;

    VpnDialog dlg(VpnDialog::Edit, this, data, m_nmcliPrefix);
    if (dlg.exec() == QDialog::Accepted)
        refreshList();
}

void MainWindow::onDelete()
{
    if (m_selectedUuid.isEmpty())
        return;

    auto reply = QMessageBox::question(this, "Delete Connection",
        "Are you sure you want to delete this connection?",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        runNmcli({"connection", "delete", m_selectedUuid});
    }
}

void MainWindow::onConnectToggle()
{
    if (m_selectedUuid.isEmpty())
        return;

    auto *item = m_connectionList->currentItem();
    if (!item)
        return;

    bool isConnected = item->text().startsWith("(Connected)");
    if (isConnected)
        runNmcli({"connection", "down", m_selectedUuid});
    else
        runNmcli({"connection", "up", m_selectedUuid});
}

void MainWindow::onSelectionChanged()
{
    auto *item = m_connectionList->currentItem();
    bool hasSelection = item != nullptr;

    m_editBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
    m_connectBtn->setEnabled(hasSelection);

    if (hasSelection) {
        m_selectedUuid = item->data(Qt::UserRole).toString();
        bool isConnected = item->text().startsWith("(Connected)");
        m_connectBtn->setText(isConnected ? "Disconnect" : "Connect");
    } else {
        m_selectedUuid.clear();
        m_connectBtn->setText("Connect");
    }
}

void MainWindow::onImport()
{
    QFileDialog dialog(this, "Import VPN Configuration");
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::ReadOnly);
    dialog.setNameFilter("All files (*)");
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString filePath = dialog.selectedFiles().first();
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QMap<QString, QString> data;

    if (content.trimmed().startsWith("conn ")) {
        QRegularExpression connRx("^conn\\s+(\\S+)", QRegularExpression::MultilineOption);
        auto connMatch = connRx.match(content);
        if (connMatch.hasMatch())
            data["name"] = connMatch.captured(1);

        auto extract = [&](const QString &key) {
            QRegularExpression rx(QString("^\\s+%1\\s*=\\s*(\\S+)").arg(key),
                                  QRegularExpression::MultilineOption);
            auto m = rx.match(content);
            return m.hasMatch() ? m.captured(1) : QString();
        };

        QString gateway = extract("right");
        if (!gateway.isEmpty()) data["gateway"] = gateway;
        QString user = extract("leftxauthusername");
        if (!user.isEmpty()) data["username"] = user;
        QString ike = extract("ike");
        if (!ike.isEmpty()) data["ike"] = ike;
        QString esp = extract("esp");
        if (!esp.isEmpty()) data["esp"] = esp;
        QString aggressive = extract("aggressive");
        if (!aggressive.isEmpty()) data["aggressive"] = aggressive;
        QString subnet = extract("rightsubnet");
        if (!subnet.isEmpty()) data["subnet"] = subnet;

        QFileInfo fi(filePath);
        QString secretsPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".secrets";
        QFile secretsFile(secretsPath);
        if (secretsFile.exists() && secretsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString secretsContent = QString::fromUtf8(secretsFile.readAll());
            secretsFile.close();
            QRegularExpression pskRx(": PSK\\s+\"([^\"]+)\"");
            auto pskMatch = pskRx.match(secretsContent);
            if (pskMatch.hasMatch())
                data["psk"] = pskMatch.captured(1);
            QRegularExpression xauthRx("\\S+\\s+: XAUTH\\s+\"([^\"]+)\"");
            auto xauthMatch = xauthRx.match(secretsContent);
            if (xauthMatch.hasMatch())
                data["password"] = xauthMatch.captured(1);
        }
    } else if (content.contains("[vpn]")) {
        QSettings settings(filePath, QSettings::IniFormat);

        QString name = settings.value("connection/id").toString();
        if (!name.isEmpty()) data["name"] = name;

        QString gateway = settings.value("vpn/address").toString();
        if (!gateway.isEmpty()) data["gateway"] = gateway;

        QString psk = settings.value("vpn/psk").toString();
        if (!psk.isEmpty()) data["psk"] = psk;

        QString username = settings.value("vpn/leftxauthusername").toString();
        if (!username.isEmpty()) data["username"] = username;

        QString password = settings.value("vpn-secrets/xauthpassword").toString();
        if (!password.isEmpty()) data["password"] = password;

        QString ike = settings.value("vpn/ike").toString();
        if (!ike.isEmpty()) data["ike"] = ike;

        QString esp = settings.value("vpn/esp").toString();
        if (!esp.isEmpty()) data["esp"] = esp;

        QString aggressive = settings.value("vpn/aggressive").toString();
        if (!aggressive.isEmpty()) data["aggressive"] = aggressive;

        QString encap = settings.value("vpn/encap").toString();
        if (!encap.isEmpty()) data["encap"] = encap;

        QString virtual_ = settings.value("vpn/virtual").toString();
        if (!virtual_.isEmpty()) data["virtual"] = virtual_;

        QString subnet = settings.value("vpn/rightsubnet").toString();
        if (!subnet.isEmpty()) data["subnet"] = subnet;

        if (psk.isEmpty())
            psk = settings.value("vpn-secrets/psk").toString();
        if (!psk.isEmpty() && data["psk"].isEmpty())
            data["psk"] = psk;
    }

    VpnDialog dlg(VpnDialog::Add, this, data, m_nmcliPrefix);
    if (dlg.exec() == QDialog::Accepted)
        refreshList();
}

void MainWindow::onHelp()
{
    QMessageBox::about(this, "About IPsec Client",
        QStringLiteral("<h3>IPsec Client v%1</h3>"
        "<p>A GUI for managing strongSwan VPN connections.</p>"
        "<p>Use the <b>Add</b> button to create a new connection, "
        "or <b>Import</b> to load a .conf or .nmconnection file.</p>"
        "<p>Select a connection and click <b>Connect</b>/<b>Disconnect</b> to toggle it.</p>")
        .arg(IPSEC_CLIENT_VERSION));
}

void MainWindow::onProcessFinished()
{
    QString stderr = m_process->readAllStandardError();
    if (!stderr.isEmpty())
        m_statusLabel->setText("Error: " + stderr);
    else
        m_statusLabel->setText("Operation completed successfully");

    refreshList();
}

void MainWindow::runNmcli(const QStringList &args)
{
    m_statusLabel->setText("Running: nmcli " + args.join(' '));
    if (m_nmcliPrefix.isEmpty())
        m_process->start("nmcli", args);
    else
        m_process->start("flatpak-spawn", QStringList{"--host", "nmcli"} + args);
}
