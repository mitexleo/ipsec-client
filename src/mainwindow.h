#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProcess>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onConnectToggle();
    void refreshList();
    void onSelectionChanged();
    void onProcessFinished();

private:
    void runNmcli(const QStringList &args);
    void parseConnectionList(const QString &output, const QStringList &activeUuids);

    QListWidget *m_connectionList;
    QPushButton *m_addBtn;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_connectBtn;
    QLabel *m_statusLabel;
    QProcess *m_process;
    QString m_selectedUuid;
};

#endif
