#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QFrame>
#include <QProgressBar>
#include <opencv2/opencv.hpp>

class SignalBridge : public QObject {
    Q_OBJECT
public:
    static SignalBridge* getInstance() { static SignalBridge instance; return &instance; }
signals:
    void sendFrame(const cv::Mat& frame);
    void sendAlarmLog(QString type, QString time, QString imgPath);
    void sendPhysicalAlarmStatus(bool triggered);
private:
    SignalBridge() = default;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
public slots:
    void updateFrame(const cv::Mat& frame);
    void addLogEntry(QString type, QString time, QString imgPath);
private slots:
    void onExitClicked();
    void onLiveStreamClicked();
    void onImportVideoClicked();
    void showImageDialog(int row, int column);
    void updateSystemStats();
private:
    QLabel* videoLabel;
    QTableWidget* logTable;
    QPushButton* btnLiveStream;
    QPushButton* btnImportVideo;
    QPushButton* btnExit;
    QLabel* badgeRPMsg;
    QLabel* badgeTemp;
    QLabel* badgeCPU;
    QLabel* badgeMem;
    QLabel* aiStatusLabel;
    // 传感器
    QLabel* sensorFire;
    QLabel* sensorGas;
    QLabel* sensorTemp;
    QLabel* sensorHumid;
    QLabel* sensorProx;
    QLabel* sensorNoise;
    // 安全评分与灯塔
    QLabel* scoreLabel;
    QLabel* scoreDesc;
    QLabel* lightR;
    QLabel* lightY;
    QLabel* lightG;
    QLabel* safetyLevel;
    // 违规统计
    QProgressBar* violBar[4];
    QLabel* violCount[4];
    // 核心架构
    QLabel* coreLabel[3];
    // 系统健康
    QLabel* healthLabels[5];
    // 通信安全
    QLabel* secLabels[4];
    // 系统
    QTimer* systemTimer;
    unsigned long long prevTotalTicks;
    unsigned long long prevIdleTicks;
    int violTotals[4]; // helmet, vest, glass, physical
    QFrame* makeCard(const QString& title, QLayout* inner);
};