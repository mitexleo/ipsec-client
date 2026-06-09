#include "mainwindow.h"
#include "vpndialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSet>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_connectionList(new QListWidget(this))
    , m_addBtn(new QPushButton("Add", this))
    , m_editBtn(new QPushButton("Edit", this))
    , m_deleteBtn(new QPushButton("Delete", this))
    , m_connectBtn(new QPushButton("Connect", this))
    , m_statusLabel(new QLabel("Ready", this))
    , m_process(new QProcess(this))
{
    setWindowTitle("IPsec Client");
    setMinimumSize(640, 480);

    auto *centralWidget = new QWidget(this);
    auto *mainLayout = new QHBoxLayout(centralWidget);

    mainLayout->addWidget(m_connectionList, 1);

    auto *rightLayout = new QVBoxLayout;
    m_statusLabel->setWordWrap(true);
    rightLayout->addWidget(m_statusLabel);
    rightLayout->addStretch();
    rightLayout->addWidget(m_connectBtn);
    rightLayout->addWidget(m_addBtn);
    rightLayout->addWidget(m_editBtn);
    rightLayout->addWidget(m_deleteBtn);
    mainLayout->addLayout(rightLayout);

    setCentralWidget(centralWidget);

    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_connectBtn->setEnabled(false);

    connect(m_addBtn, &QPushButton::clicked, this, &MainWindow::onAdd);
    connect(m_editBtn, &QPushButton::clicked, this, &MainWindow::onEdit);
    connect(m_deleteBtn, &QPushButton::clicked, this, &MainWindow::onDelete);
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectToggle);
    connect(m_connectionList, &QListWidget::itemSelectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onProcessFinished);

    refreshList();
}

void MainWindow::refreshList()
{
    QStringList activeUuids;
    QProcess activeProc;
    activeProc.start("nmcli", {"-t", "-f", "NAME,UUID,TYPE,DEVICE", "con", "show", "--active"});
    activeProc.waitForFinished(5000);
    QString activeOutput = QString::fromUtf8(activeProc.readAllStandardOutput());
    for (const auto &line : activeOutput.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(':');
        if (parts.size() >= 2)
            activeUuids.append(parts[1]);
    }

    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "NAME,UUID,TYPE,DEVICE", "con", "show"});
    proc.waitForFinished(5000);
    QString output = QString::fromUtf8(proc.readAllStandardOutput());

    m_connectionList->clear();

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
}

void MainWindow::onAdd()
{
    VpnDialog dlg(VpnDialog::Add, this);
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

    VpnDialog dlg(VpnDialog::Edit, this, data);
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
    m_process->start("nmcli", args);
}
