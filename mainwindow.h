#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class SerialPortManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_pushButtonOpen_clicked();
    void on_pushButtonSend_clicked();
    void on_checkBoxAutoSend_stateChanged(int state);

    void onConnectionStateChanged(bool connected);
    void onDataReceived(const QByteArray &data);
    void onErrorOccurred(const QString &message);
    void onAutoSendTriggered();

    void refreshPortList();

private:
    void setupUiDefaults();
    void connectSignals();
    QByteArray buildSendData(bool showErrors = true);
    void appendReceiveData(const QByteArray &data);

    Ui::MainWindow *ui;
    SerialPortManager *m_portManager = nullptr;
    QButtonGroup *m_rxFormatGroup = nullptr;
    QButtonGroup *m_txFormatGroup = nullptr;
    bool m_isHexReceive = true;
    bool m_isHexSend = false;
};

#endif // MAINWINDOW_H
