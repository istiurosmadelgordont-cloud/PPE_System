/**
 * @file      ui_main_window.cpp
 * @brief     Qt5 零拷贝全栈可视化与防抖监控中枢
 * @author    [双生序章]
 * @version   2.0.0
 * @date      2026-04-16
 * @note      运行于 Core 0 (小核)。负责基于 SignalBridge 的跨线程 OpenCV 图像零拷贝渲染、
 *            双向防抖状态机判定，以及底层硬件报警信号的 UI 强制瞬间重绘。
 */
#include "ui_main_window.hpp"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>  
#include <QDialog>   
#include <chrono>    
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QStringList> //用于分割字符串
#include <QDateTime>
#include "rpmsg_node.hpp"

Q_DECLARE_METATYPE(cv::Mat)

extern int current_source_mode; 
extern std::string video_path;
extern bool source_changed;
extern bool is_running;

//初始化CPU历史滴答数为0
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), prevTotalTicks(0), prevIdleTicks(0) {
    qRegisterMetaType<cv::Mat>("cv::Mat");

    //1.视频显示区
    videoLabel = new QLabel(this);
    videoLabel->setMinimumSize(640, 480);
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setScaledContents(true); 
    videoLabel->setText("等待视频流接入...");
    videoLabel->setStyleSheet("color: white; background-color: #1e1e1e; border: 2px solid #555;");

    //2.违规日志表格
    logTable = new QTableWidget(0, 3, this);
    logTable->setHorizontalHeaderLabels({"报警时间", "违规类型", "处理状态"});
    logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTable->setMinimumHeight(150);
    logTable->setStyleSheet(
        "QTableWidget {"
        "   background-color: #FCFBF8;"
        "   color: #4A4643;"
        "   gridline-color: #EAE6DF;"
        "   border: 1px solid #E0DCD3;"
        "   border-radius: 4px;"
        "   font-size: 13px;"
        "}"
        "QTableWidget::item:selected {"
        "   background-color: #FDF3E1;" // 选中时变成温暖的浅琥珀色
        "   color: #D97706;"
        "}"
        "QHeaderView::section {"
        "   background-color: #F3EFE6;"
        "   color: #5C5750;"
        "   font-weight: bold;"
        "   border: none;"
        "   border-bottom: 1px solid #E0DCD3;"
        "   padding: 4px;"
        "}"
    );

    //3.按钮与监控标签创建
    btnLiveStream = new QPushButton(" 实时监控", this);
    btnImportVideo = new QPushButton(" 导入录像", this);
    btnExit = new QPushButton(" 安全退出", this);

    int btnHeight = 45;
    btnLiveStream->setMinimumHeight(btnHeight);
    btnImportVideo->setMinimumHeight(btnHeight);
    btnExit->setMinimumHeight(btnHeight);
    
     btnLiveStream->setStyleSheet("QPushButton { background-color: #10B981; color: white; font-weight: bold; border-radius: 4px; border: none; } QPushButton:hover { background-color: #059669; }");
    btnImportVideo->setStyleSheet("QPushButton { background-color: #F59E0B; color: white; font-weight: bold; border-radius: 4px; border: none; } QPushButton:hover { background-color: #D97706; }");
    btnExit->setStyleSheet("QPushButton { background-color: #EF4444; color: white; font-weight: bold; border-radius: 4px; border: none; } QPushButton:hover { background-color: #DC2626; }");

    tempLabel = new QLabel("温度: --.- °C", this);
    tempLabel->setStyleSheet("color: #5cb85c; font-weight: bold; font-size: 16px; padding: 2px;");
    tempLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); 

    //CPU使用率标签
    cpuUsageLabel = new QLabel("CPU: --%", this);
    cpuUsageLabel->setStyleSheet("color: #5cb85c; font-weight: bold; font-size: 16px; padding: 2px; margin-left: 15px;");
    cpuUsageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); 

    //4.布局拼装
    
    // 【初始化新增的标签】：直面物理现实（11类模型，低电平触发蜂鸣器）
    hardwareStatusLabel = new QLabel("物理警报(RPMsg): 蜂鸣器挂起", this);
    hardwareStatusLabel->setStyleSheet("color: #00C851; font-weight: bold; font-size: 14px; margin-right: 15px;");
    
    aiStatusLabel = new QLabel(" 引擎: YOLOv8 INT8 | 追踪: ByteTrack", this);
    aiStatusLabel->setStyleSheet("color: #5bc0de; font-weight: bold; font-size: 12px; margin-top: 5px;");

    // ------------------------------------------
    // 模块 1：全局顶栏 (Header)
    // ------------------------------------------
    QHBoxLayout* headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 10);

    QLabel* logoLabel = new QLabel(this);
    QPixmap logoPixmap("/home/user/logo.png");
    if (!logoPixmap.isNull()) {
        logoLabel->setPixmap(logoPixmap.scaledToHeight(60, Qt::SmoothTransformation)); // 绝对锁死高度
    } else {
        logoLabel->setText("全国大学生集成电路创新创业大赛");
        logoLabel->setStyleSheet("color: white; font-weight: bold; font-size: 18px;");
    }
    
    QHBoxLayout* statusLayout = new QHBoxLayout;
    statusLayout->addWidget(hardwareStatusLabel);
    statusLayout->addWidget(tempLabel);
    statusLayout->addWidget(cpuUsageLabel);

    headerLayout->addWidget(logoLabel);
    headerLayout->addStretch(); // 中间推开
    headerLayout->addLayout(statusLayout);

    // ------------------------------------------
    // 模块 2：主体区域 (Body - 左右分栏)
    // ------------------------------------------
    QHBoxLayout* bodyLayout = new QHBoxLayout;

    // 2.1 左侧：核心视频区 (视觉主权)
    QVBoxLayout* leftLayout = new QVBoxLayout;
    leftLayout->setContentsMargins(0, 0, 15, 0); 
    leftLayout->addWidget(videoLabel, 1);        // 权重1：吸收剩余空间
    leftLayout->addWidget(aiStatusLabel, 0);     // 权重0：紧贴底部

    // 2.2 右侧：侧边栏 (日志 + 控制)
    QWidget* rightPanel = new QWidget(this);
    rightPanel->setMaximumWidth(320); // 物理隔离：绝不反噬视频区域
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setAlignment(Qt::AlignTop);

    QLabel* logTitle = new QLabel(" 实时违规抓拍日志", this);
    logTitle->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");
    rightLayout->addWidget(logTitle);
    
    logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents); 
    logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);          
    logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents); 
    rightLayout->addWidget(logTable, 1); 
    rightLayout->addSpacing(15);

    QVBoxLayout* buttonLayout = new QVBoxLayout;
    buttonLayout->addWidget(btnLiveStream);
    buttonLayout->addWidget(btnImportVideo);
    buttonLayout->addWidget(btnExit);
    rightLayout->addLayout(buttonLayout);

    bodyLayout->addLayout(leftLayout, 1);    
    bodyLayout->addWidget(rightPanel, 0);    

    // ------------------------------------------
    // 模块 3：全局总装
    // ------------------------------------------
    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(headerLayout, 0); // 顶栏权重0
    mainLayout->addLayout(bodyLayout, 1);   // 主体权重1

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    
    // 采用护眼的工业暖米色作为全局底色
    centralWidget->setStyleSheet("background-color: #F8F6F0; color: #2C2A29;");
    setCentralWidget(centralWidget);
    
    resize(1024, 600); 


    //5.事件绑定
    connect(btnLiveStream, &QPushButton::clicked, this, &MainWindow::onLiveStreamClicked);
    connect(btnImportVideo, &QPushButton::clicked, this, &MainWindow::onImportVideoClicked);
    connect(btnExit, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    
    connect(SignalBridge::getInstance(), &SignalBridge::sendFrame, 
            this, &MainWindow::updateFrame, Qt::QueuedConnection);

    connect(SignalBridge::getInstance(), &SignalBridge::sendAlarmLog, 
            this, &MainWindow::addLogEntry, Qt::QueuedConnection);
            
    connect(logTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::showImageDialog);

    //启动系统状态监控定时器(2秒刷新一次)
    systemTimer = new QTimer(this);
    connect(systemTimer, &QTimer::timeout, this, &MainWindow::updateSystemStats);
    systemTimer->start(2000); 
    // 接收到跨核火警信号的微秒级瞬间，立刻强制 UI 变红！
    connect(SignalBridge::getInstance(), &SignalBridge::sendPhysicalAlarmStatus, 
            this, [this](bool triggered){
        if (triggered) {
            hardwareStatusLabel->setText("物理警报(RPMsg): 🚨 触发！(低电平)");
            hardwareStatusLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px; background-color: #ff4444; border-radius: 4px; padding: 4px;");
            // 顺便让视频大边框也瞬间变红，视觉冲击力拉满
            videoLabel->setStyleSheet("color: white; background-color: #1e1e1e; border: 6px solid #ff0000;");
        } else {
            hardwareStatusLabel->setText("物理警报(RPMsg): 蜂鸣器挂起(高电平)");
            hardwareStatusLabel->setStyleSheet("color: #059669; font-weight: bold; font-size: 14px; margin-right: 15px;");
            videoLabel->setStyleSheet("color: white; background-color: #1e1e1e; border: 2px solid #555;");
        }
    }, Qt::QueuedConnection);
}

MainWindow::~MainWindow() {}

void MainWindow::updateFrame(const cv::Mat& frame) {
    if (frame.empty()) return;

    static auto last_time = std::chrono::steady_clock::now();
    static int frames = 0;
    static double current_fps = 0.0;

    frames++;
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_time).count();
    
    if (elapsed >= 1.0) {
        current_fps = frames / elapsed;
        frames = 0;
        last_time = now;
    }

    QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_BGR888);
    QPixmap pixmap = QPixmap::fromImage(img);

    QPainter painter(&pixmap);
    painter.setFont(QFont("Arial", 16, QFont::Bold));
    QString fpsText = QString("UI FPS: %1").arg(current_fps, 0, 'f', 1);
    QRect textRect = pixmap.rect().adjusted(0, 15, -15, 0); 
    painter.setPen(QColor(0, 0, 0));
    painter.drawText(textRect.translated(2, 2), Qt::AlignTop | Qt::AlignRight, fpsText);
    painter.setPen(QColor(0, 255, 0));
    painter.drawText(textRect, Qt::AlignTop | Qt::AlignRight, fpsText);
    painter.end();

    videoLabel->setPixmap(pixmap);
}

void MainWindow::addLogEntry(QString type, QString time, QString imgPath) {
    logTable->insertRow(0); 
    logTable->setItem(0, 0, new QTableWidgetItem(time));
    logTable->setItem(0, 1, new QTableWidgetItem(type));
    
    QTableWidgetItem* statusItem = new QTableWidgetItem("🔍 双击查看图片");
    statusItem->setForeground(QBrush(Qt::cyan));
    
    statusItem->setData(Qt::UserRole, imgPath); 
    
    logTable->setItem(0, 2, statusItem);

    if (logTable->rowCount() > 50) {
        logTable->removeRow(50);
    }
}

void MainWindow::showImageDialog(int row, int column) {
    QTableWidgetItem* item = logTable->item(row, 2);
    if (!item) return;
    QString imgPath = item->data(Qt::UserRole).toString();
    
    QImage img(imgPath);
    if(img.isNull()) {
        QMessageBox::warning(this, "提示", "图片已被系统自动清理或尚未写入磁盘！");
        return;
    }
    
    QDialog dialog(this);
    dialog.setWindowTitle("抓拍证据回放 - " + logTable->item(row, 0)->text());
    dialog.setStyleSheet("background-color: #2b2b2b; color: white;");
    
    QLabel* imgLabel = new QLabel(&dialog);
    imgLabel->setPixmap(QPixmap::fromImage(img).scaled(800, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(imgLabel);
    
    dialog.exec(); 
}

void MainWindow::onLiveStreamClicked() {
    if (current_source_mode == 0) return;
    current_source_mode = 0;
    source_changed = true;
    btnLiveStream->setText(" 监控中...");
    btnImportVideo->setText(" 导入录像");
}

void MainWindow::onImportVideoClicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "选择测试视频", "", "Video Files (*.mp4 *.avi)");
    if (fileName.isEmpty()) return;

    video_path = fileName.toStdString();
    current_source_mode = 1;
    source_changed = true;
    btnImportVideo->setText("分析中...");
    btnLiveStream->setText(" 实时监控");
}

void MainWindow::onExitClicked() {
    is_running = false; 
    QApplication::quit();
}

// ==========================================
// 【综合系统监控】：读取 CPU 温度与综合使用率
// ==========================================
void MainWindow::updateSystemStats() {
    // 1. 读取温度
    QFile tempFile("/sys/class/thermal/thermal_zone0/temp");
    if (tempFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&tempFile);
        QString line = in.readLine();
        if (!line.isNull()) {
            double temp = line.toDouble() / 1000.0;
            tempLabel->setText(QString("温度: %1 °C").arg(temp, 0, 'f', 1));
            
            if (temp >= 75.0) {
                tempLabel->setStyleSheet("color: #ff4444; font-weight: bold; font-size: 16px; padding: 2px;"); 
            } else if (temp >= 65.0) {
                tempLabel->setStyleSheet("color: #ffbb33; font-weight: bold; font-size: 16px; padding: 2px;"); 
            } else {
                tempLabel->setStyleSheet("color: #00C851; font-weight: bold; font-size: 16px; padding: 2px;"); 
            }
        }
        tempFile.close();
    }

    // 2.读取CPU使用率(解析 /proc/stat)
    QFile statFile("/proc/stat");
    if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&statFile);
        QString line = in.readLine(); // 读取第一行 "cpu ..."
        
        if (!line.isNull() && line.startsWith("cpu ")) {
            // 用简化方法清理多余空格并分割
            QStringList parts = line.simplified().split(' ');
            
            if (parts.size() > 4) {
                unsigned long long user = parts[1].toULongLong();
                unsigned long long nice = parts[2].toULongLong();
                unsigned long long system = parts[3].toULongLong();
                unsigned long long idle = parts[4].toULongLong();
                unsigned long long iowait = parts[5].toULongLong();
                unsigned long long irq = parts[6].toULongLong();
                unsigned long long softirq = parts[7].toULongLong();

                unsigned long long totalIdle = idle + iowait;
                unsigned long long totalNonIdle = user + nice + system + irq + softirq;
                unsigned long long total = totalIdle + totalNonIdle;

                // 计算时间差 Delta
                if (prevTotalTicks != 0) {
                    unsigned long long totalDiff = total - prevTotalTicks;
                    unsigned long long idleDiff = totalIdle - prevIdleTicks;
                    
                    double usage = 100.0 * (totalDiff - idleDiff) / (double)totalDiff;
                    
                    cpuUsageLabel->setText(QString("CPU: %1%").arg(usage, 0, 'f', 1));
                    
                    // 同样加入动态变色预警
                    if (usage >= 90.0) {
                        cpuUsageLabel->setStyleSheet("color: #ff4444; font-weight: bold; font-size: 16px; padding: 2px; margin-left: 15px;");
                    } else if (usage >= 70.0) {
                        cpuUsageLabel->setStyleSheet("color: #ffbb33; font-weight: bold; font-size: 16px; padding: 2px; margin-left: 15px;");
                    } else {
                        cpuUsageLabel->setStyleSheet("color: #00C851; font-weight: bold; font-size: 16px; padding: 2px; margin-left: 15px;");
                    }
                }
                
                // 保存当前状态供下次计算使用
                prevTotalTicks = total;
                prevIdleTicks = totalIdle;
            }
        }
        statFile.close();
    }
      // ==========================================
   

}
