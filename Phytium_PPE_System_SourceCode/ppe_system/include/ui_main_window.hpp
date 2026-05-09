#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTableWidget>
#include <QImage>
#include <QPixmap>
#include <QTimer> 
#include <opencv2/opencv.hpp>

// ==========================================
// 跨线程通信桥梁：大核 AI -> 小核 UI
// ==========================================
class SignalBridge : public QObject {
    Q_OBJECT
public:
    static SignalBridge* getInstance() {
        static SignalBridge instance;
        return &instance;
    }
signals:
    void sendFrame(const cv::Mat& frame);          
    void sendAlarmLog(QString type, QString time, QString imgPath); 
    void sendPhysicalAlarmStatus(bool triggered);
private:
    SignalBridge() = default;
};

// ==========================================
// 主窗口类
// ==========================================
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
    
    // 【修改】：统一更名为更新系统状态（包含温度和CPU）
    void updateSystemStats();                  

private:
    QLabel* videoLabel;
    QTableWidget* logTable; 
    QPushButton* btnLiveStream;
    QPushButton* btnImportVideo;
    QPushButton* btnExit;
    
    QLabel* tempLabel;        
    QLabel* cpuUsageLabel;    // 【新增】：声明 CPU 使用率标签
    QTimer* systemTimer;      // 【修改】：更名为系统定时器

    QLabel* hardwareStatusLabel; 
    QLabel* aiStatusLabel;
    
    // 【新增】：用于计算 CPU 使用率的历史数据
    unsigned long long prevTotalTicks;
    unsigned long long prevIdleTicks;
};