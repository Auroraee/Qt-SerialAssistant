#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QButtonGroup>
#include <QByteArray>
#include <QString>
#include <QTimer>

#include "frametypes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
class QCloseEvent;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QModelIndex;
class QSpinBox;
class QTableView;
class QTabWidget;
QT_END_NAMESPACE

class FrameFilterProxyModel;
class SerialSessionController;
struct SerialPortConfig;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_pushButtonOpen_clicked();
    void on_pushButtonSend_clicked();
    void on_checkBoxAutoSend_stateChanged(int state);

    void onConnectionStateChanged(ConnectionState state);
    void onDataReceived(const QByteArray &data);
    void onErrorOccurred(const QString &message);
    void onCriticalErrorOccurred(const QString &message);
    void onFrameRowsInserted(const QModelIndex &parent, int first, int last);

    void refreshPortList();
    void calculateChecksum();
    void exportLog();
    void applyFrameSettings();
    void applyFrameFilter();
    void updateAutoSendPayload();

private:
    void setupUiDefaults();
    void setupProtocolWorkbench();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    SerialPortConfig currentSerialConfig(bool *ok);
    QByteArray buildSendDataFromText(const QString &input, bool showErrors, bool *ok);
    QByteArray buildChecksumData(bool *ok);
    QByteArray lineEndingBytes() const;
    QString formatData(const QByteArray &data, bool asHex) const;
    void sendUserText(const QString &input, bool addHistory, bool echoTx, bool showErrors);
    void appendReceiveData(const QByteArray &data);
    void appendSentData(const QByteArray &data, bool echo);
    void appendLogLine(const QString &direction, const QString &format, const QString &message);
    void appendErrorLog(const QString &message);
    void addSendHistory(const QString &input);
    void setSerialControlsEnabled(bool enabled);
    void setSendControlsEnabled(bool connected);
    void updateCounters();
    void clearCounters();
    void showStatusMessage(const QString &message, int timeoutMs = 5000);

    Ui::MainWindow *ui;
    SerialSessionController *m_session = nullptr;
    FrameFilterProxyModel *m_frameProxy = nullptr;
    QButtonGroup *m_rxFormatGroup = nullptr;
    QButtonGroup *m_txFormatGroup = nullptr;
    bool m_isHexReceive = true;
    bool m_isHexSend = false;

    QTimer *m_portRefreshTimer = nullptr;
    qint64 m_rxBytes = 0;
    qint64 m_txBytes = 0;

    QTabWidget *m_receiveTabs = nullptr;
    QTableView *m_frameTable = nullptr;
    QComboBox *m_frameModeCombo = nullptr;
    QSpinBox *m_frameValueSpin = nullptr;
    QLineEdit *m_frameDelimiterEdit = nullptr;
    QCheckBox *m_includeDelimiterCheck = nullptr;
    QComboBox *m_filterDirectionCombo = nullptr;
    QComboBox *m_filterProtocolCombo = nullptr;
    QSpinBox *m_filterStationSpin = nullptr;
    QSpinBox *m_filterFunctionSpin = nullptr;
    QComboBox *m_filterCrcCombo = nullptr;
    QLineEdit *m_filterKeywordEdit = nullptr;
    QComboBox *m_languageCombo = nullptr;
};

#endif // MAINWINDOW_H
