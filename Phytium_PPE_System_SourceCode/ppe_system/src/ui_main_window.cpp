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

    // ==========================================
    // 全局样式设定 (QSS) - 工业暗色系主题
    // ==========================================
    this->setStyleSheet(
        "QMainWindow { background-color: #120f0d; }"
        "QLabel { color: #e8d6b3; font-family: 'Segoe UI', 'Microsoft YaHei'; }"
        "QFrame#card { background-color: #1e1915; border: 1px solid #332b24; border-radius: 8px; }"
        "QTableWidget { background-color: #1a1613; color: #a8a096; gridline-color: #2a241f; border: none; font-size: 12px; }"
        "QTableWidget::item { border-bottom: 1px solid #2a241f; padding: 4px; }"
        "QTableWidget::item:selected { background-color: #332b24; color: #d48806; }"
        "QHeaderView::section { background-color: #1e1915; color: #d48806; font-weight: bold; border: none; border-bottom: 1px solid #d48806; padding: 4px; }"
        "QProgressBar { border: none; background-color: #2a241f; border-radius: 3px; text-align: center; color: transparent; }"
        "QProgressBar::chunk { background-color: #d48806; border-radius: 3px; }"
        "QScrollBar:vertical { background: #1a1613; width: 8px; }"
        "QScrollBar::handle:vertical { background: #332b24; border-radius: 4px; }"
    );

    // ==========================================
    // 1. 左侧核心视频区
    // ==========================================
    videoLabel = new QLabel(this);
    videoLabel->setMinimumSize(800, 600);
    videoLabel->setAlignment(Qt::AlignCenter);
    videoLabel->setScaledContents(true); 
    videoLabel->setText("等待视频流接入...");
    videoLabel->setStyleSheet("background-color: #120f0d; border: 1px solid #332b24; border-radius: 8px;");

    aiStatusLabel = new QLabel("© YOLOv8 INT8 | 🧬 ByteTrack | 🎯 320x320 | ⚡ NCNN", this);
    aiStatusLabel->setStyleSheet("color: #d48806; font-size: 11px; font-weight: bold;");

    // 顶栏状态标记
    badgeRPMsg = new QLabel("● RPMsg", this);
    badgeRPMsg->setStyleSheet("background-color: #1e3a2f; color: #10b981; border-radius: 10px; padding: 4px 12px; border: 1px solid #10b981; font-weight: bold; font-size: 11px;");
    badgeTemp = new QLabel("↓ --.-°C", this);
    badgeTemp->setStyleSheet("background-color: #1e3a2f; color: #10b981; border-radius: 10px; padding: 4px 12px; border: 1px solid #10b981; font-weight: bold; font-size: 11px;");
    badgeCPU = new QLabel("↓ CPU --%", this);
    badgeCPU->setStyleSheet("background-color: #1e3a2f; color: #10b981; border-radius: 10px; padding: 4px 12px; border: 1px solid #10b981; font-weight: bold; font-size: 11px;");
    badgeMem = new QLabel("■ 42%", this);
    badgeMem->setStyleSheet("background-color: #1e2a3a; color: #3b82f6; border-radius: 10px; padding: 4px 12px; border: 1px solid #3b82f6; font-weight: bold; font-size: 11px;");

    QHBoxLayout* topStatusLayout = new QHBoxLayout;
    topStatusLayout->addStretch();
    topStatusLayout->addWidget(badgeRPMsg);
    topStatusLayout->addWidget(badgeTemp);
    topStatusLayout->addWidget(badgeCPU);
    topStatusLayout->addWidget(badgeMem);

    QVBoxLayout* leftLayout = new QVBoxLayout;
    leftLayout->setContentsMargins(0, 0, 15, 0);
    leftLayout->addLayout(topStatusLayout);
    leftLayout->addWidget(videoLabel, 1);
    leftLayout->addWidget(aiStatusLabel, 0, Qt::AlignCenter);

    // ==========================================
    // 2. 右侧数据面板区
    // ==========================================
    QWidget* rightPanel = new QWidget(this);
    rightPanel->setMaximumWidth(480);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // Card 1: 传感器数据矩阵
    QGridLayout* sensorLayout = new QGridLayout;
    sensorFire = new QLabel("● 安全", this); sensorFire->setStyleSheet("color: #10b981; font-size: 16px; font-weight: bold;");
    sensorGas = new QLabel("● 正常", this); sensorGas->setStyleSheet("color: #3b82f6; font-size: 16px; font-weight: bold;");
    sensorTemp = new QLabel("28.5°C", this); sensorTemp->setStyleSheet("color: #d48806; font-size: 16px; font-weight: bold;");
    sensorHumid = new QLabel("65%", this); sensorHumid->setStyleSheet("color: #3b82f6; font-size: 16px; font-weight: bold;");
    sensorProx = new QLabel("3人", this); sensorProx->setStyleSheet("color: #a8a096; font-size: 16px; font-weight: bold;");
    sensorNoise = new QLabel("62dB", this); sensorNoise->setStyleSheet("color: #10b981; font-size: 16px; font-weight: bold;");
    
    QLabel* lF = new QLabel("🔥 火控探头"); lF->setAlignment(Qt::AlignCenter); lF->setStyleSheet("color: #8c8273; font-size: 11px;");
    QLabel* lG = new QLabel("💨 有害气体"); lG->setAlignment(Qt::AlignCenter); lG->setStyleSheet("color: #8c8273; font-size: 11px;");
    QLabel* lT = new QLabel("🌡️ 温度"); lT->setAlignment(Qt::AlignCenter); lT->setStyleSheet("color: #8c8273; font-size: 11px;");
    QLabel* lH = new QLabel("💧 湿度"); lH->setAlignment(Qt::AlignCenter); lH->setStyleSheet("color: #8c8273; font-size: 11px;");
    QLabel* lP = new QLabel("👤 人员接近"); lP->setAlignment(Qt::AlignCenter); lP->setStyleSheet("color: #8c8273; font-size: 11px;");
    QLabel* lN = new QLabel("🔊 噪声"); lN->setAlignment(Qt::AlignCenter); lN->setStyleSheet("color: #8c8273; font-size: 11px;");
    
    sensorFire->setAlignment(Qt::AlignCenter); sensorGas->setAlignment(Qt::AlignCenter); sensorTemp->setAlignment(Qt::AlignCenter);
    sensorHumid->setAlignment(Qt::AlignCenter); sensorProx->setAlignment(Qt::AlignCenter); sensorNoise->setAlignment(Qt::AlignCenter);

    sensorLayout->addWidget(lF, 0, 0); sensorLayout->addWidget(sensorFire, 1, 0);
    sensorLayout->addWidget(lG, 0, 1); sensorLayout->addWidget(sensorGas, 1, 1);
    sensorLayout->addWidget(lT, 0, 2); sensorLayout->addWidget(sensorTemp, 1, 2);
    sensorLayout->addWidget(lH, 2, 0); sensorLayout->addWidget(sensorHumid, 3, 0);
    sensorLayout->addWidget(lP, 2, 1); sensorLayout->addWidget(sensorProx, 3, 1);
    sensorLayout->addWidget(lN, 2, 2); sensorLayout->addWidget(sensorNoise, 3, 2);
    
    QFrame* card1 = makeCard("🔌 从底层传感器矩阵 (Core 1 裸机前哨站)", sensorLayout);

    // Card 2 & 3: 评分与灯塔
    QHBoxLayout* middleLayout = new QHBoxLayout;
    
    QVBoxLayout* scoreLayout = new QVBoxLayout;
    scoreLabel = new QLabel("84", this);
    scoreLabel->setAlignment(Qt::AlignCenter);
    scoreLabel->setStyleSheet("color: #10b981; font-size: 32px; font-weight: bold; border: 4px solid #10b981; border-radius: 30px; min-width: 60px; max-width: 60px; min-height: 60px; max-height: 60px;");
    scoreDesc = new QLabel("良好 - 违规40处", this);
    scoreDesc->setAlignment(Qt::AlignCenter);
    scoreDesc->setStyleSheet("color: #10b981; font-size: 12px;");
    scoreLayout->addWidget(scoreLabel, 0, Qt::AlignCenter);
    scoreLayout->addWidget(scoreDesc, 0, Qt::AlignCenter);
    QFrame* card2 = makeCard("🏆 今日安全评分", scoreLayout);

    QVBoxLayout* lightLayout = new QVBoxLayout;
    QHBoxLayout* lights = new QHBoxLayout;
    lightR = new QLabel(this); lightR->setFixedSize(20, 20); lightR->setStyleSheet("background-color: #3a1e1e; border-radius: 10px;");
    lightY = new QLabel(this); lightY->setFixedSize(20, 20); lightY->setStyleSheet("background-color: #3a2a1e; border-radius: 10px;");
    lightG = new QLabel(this); lightG->setFixedSize(20, 20); lightG->setStyleSheet("background-color: #10b981; border-radius: 10px; box-shadow: 0 0 10px #10b981;");
    lights->addWidget(lightR); lights->addWidget(lightY); lights->addWidget(lightG);
    safetyLevel = new QLabel("当前状态：● 安全", this);
    safetyLevel->setAlignment(Qt::AlignCenter);
    safetyLevel->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold;");
    lightLayout->addLayout(lights);
    lightLayout->addWidget(safetyLevel);
    QFrame* card3 = makeCard("🚨 三色报警灯塔", lightLayout);

    middleLayout->addWidget(card2);
    middleLayout->addWidget(card3);

    // Card 4: 实时报警日志
    QVBoxLayout* logLayout = new QVBoxLayout;
    logTable = new QTableWidget(0, 3, this);
    logTable->setHorizontalHeaderLabels({"时间", "违规类型", "来源"});
    logTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    logTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    logTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTable->setShowGrid(false);
    logLayout->addWidget(logTable);
    QFrame* card4 = makeCard("🔴 实时报警日志", logLayout);

    // Card 5 & 6: 违规统计与系统监控
    QHBoxLayout* bottomLayout = new QHBoxLayout;

    QVBoxLayout* statLayout = new QVBoxLayout;
    QStringList violNames = {"未戴安全帽", "未穿反光衣", "未戴护目镜", "物理报警"};
    QStringList violColors = {"#ef4444", "#d48806", "#3b82f6", "#ef4444"};
    for (int i=0; i<4; i++) {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* n = new QLabel(violNames[i], this); n->setFixedWidth(65); n->setStyleSheet("font-size: 11px;");
        violBar[i] = new QProgressBar(this); violBar[i]->setFixedHeight(6); violBar[i]->setValue(0);
        violBar[i]->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; border-radius: 3px; }").arg(violColors[i]));
        violCount[i] = new QLabel("0", this); violCount[i]->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 11px;").arg(violColors[i]));
        row->addWidget(n); row->addWidget(violBar[i]); row->addWidget(violCount[i]);
        statLayout->addLayout(row);
    }
    // 异构核心状态
    QLabel* coreTitle = new QLabel("🧠 异构核心", this); coreTitle->setStyleSheet("color: #8c8273; font-size: 11px; margin-top: 5px;");
    statLayout->addWidget(coreTitle);
    QHBoxLayout* coreHBox = new QHBoxLayout;
    for(int i=0; i<3; i++) {
        coreLabel[i] = new QLabel(this);
        coreLabel[i]->setAlignment(Qt::AlignCenter);
        coreHBox->addWidget(coreLabel[i]);
    }
    coreLabel[0]->setText("Core0\nUI+IO\n↓ 38°C"); coreLabel[0]->setStyleSheet("border: 1px solid #10b981; border-radius: 4px; padding: 2px; font-size: 10px; color: #10b981;");
    coreLabel[1]->setText("Core1\n裸机\n↓ 2ms"); coreLabel[1]->setStyleSheet("border: 1px solid #ef4444; border-radius: 4px; padding: 2px; font-size: 10px; color: #ef4444;");
    coreLabel[2]->setText("Core2+3\nYOLO\n↑ 84%"); coreLabel[2]->setStyleSheet("border: 1px solid #d48806; border-radius: 4px; padding: 2px; font-size: 10px; color: #d48806;");
    statLayout->addLayout(coreHBox);
    QFrame* card5 = makeCard("📊 本月违规统计", statLayout);

    QVBoxLayout* healthLayout = new QVBoxLayout;
    QStringList hNames = {"AI 推理", "摄像头", "RPMsg", "通讯心跳", "丢包率"};
    for(int i=0; i<5; i++) {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* icon = new QLabel("●", this); icon->setStyleSheet(i<4 ? "color: #10b981; font-size: 10px;" : "color: #d48806; font-size: 10px;");
        QLabel* name = new QLabel(hNames[i], this); name->setStyleSheet("font-size: 11px;");
        healthLabels[i] = new QLabel(i==0?"42ms":i==1?"在线":i==2?"连接":i==3?"正常":"4.2%", this);
        healthLabels[i]->setStyleSheet(i<4 ? "color: #10b981; font-weight: bold; font-size: 11px;" : "color: #d48806; font-weight: bold; font-size: 11px;");
        healthLabels[i]->setAlignment(Qt::AlignRight);
        row->addWidget(icon); row->addWidget(name); row->addStretch(); row->addWidget(healthLabels[i]);
        healthLayout->addLayout(row);
    }
    QLabel* secTitle = new QLabel("🔒 通信安全", this); secTitle->setStyleSheet("color: #8c8273; font-size: 11px; margin-top: 5px;");
    healthLayout->addWidget(secTitle);
    QGridLayout* secGrid = new QGridLayout;
    QLabel* lCrc = new QLabel("RPMsg校验", this); lCrc->setStyleSheet("color: #8c8273; font-size: 10px;");
    secLabels[0] = new QLabel("✓ CRC", this); secLabels[0]->setStyleSheet("color: #10b981; font-weight: bold; font-size: 12px;");
    secLabels[1] = new QLabel("0 次", this); secLabels[1]->setStyleSheet("color: #a8a096; font-size: 10px;");
    QLabel* lHeart = new QLabel("心跳", this); lHeart->setStyleSheet("color: #8c8273; font-size: 10px;");
    secLabels[2] = new QLabel("✓ 在线", this); secLabels[2]->setStyleSheet("color: #d48806; font-weight: bold; font-size: 12px;");
    secLabels[3] = new QLabel("活跃", this); secLabels[3]->setStyleSheet("color: #d48806; font-size: 10px;");
    secGrid->addWidget(lCrc, 0, 0); secGrid->addWidget(secLabels[0], 1, 0); secGrid->addWidget(secLabels[1], 2, 0);
    secGrid->addWidget(lHeart, 0, 1); secGrid->addWidget(secLabels[2], 1, 1); secGrid->addWidget(secLabels[3], 2, 1);
    healthLayout->addLayout(secGrid);
    QFrame* card6 = makeCard("⚕️ 系统健康", healthLayout);

    bottomLayout->addWidget(card5);
    bottomLayout->addWidget(card6);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLiveStream = new QPushButton("● 实时监控", this);
    btnImportVideo = new QPushButton("📂 导入", this);
    btnExit = new QPushButton("⏻ 退出", this);
    int btnH = 40;
    btnLiveStream->setFixedHeight(btnH); btnImportVideo->setFixedHeight(btnH); btnExit->setFixedHeight(btnH);
    btnLiveStream->setStyleSheet("QPushButton { background-color: #059669; color: white; font-weight: bold; font-size: 14px; border-radius: 20px; } QPushButton:hover { background-color: #047857; }");
    btnImportVideo->setStyleSheet("QPushButton { background-color: #d48806; color: white; font-weight: bold; font-size: 14px; border-radius: 20px; } QPushButton:hover { background-color: #b45309; }");
    btnExit->setStyleSheet("QPushButton { background-color: #dc2626; color: white; font-weight: bold; font-size: 14px; border-radius: 20px; } QPushButton:hover { background-color: #b91c1c; }");
    btnLayout->addWidget(btnLiveStream); btnLayout->addWidget(btnImportVideo); btnLayout->addWidget(btnExit);

    // Assemble right panel
    rightLayout->addWidget(card1);
    rightLayout->addLayout(middleLayout);
    rightLayout->addWidget(card4, 1);
    rightLayout->addLayout(bottomLayout);
    rightLayout->addLayout(btnLayout);

    // ==========================================
    // 3. 全局总装
    // ==========================================
    QHBoxLayout* mainLayout = new QHBoxLayout;
    
    // Top App Header
    QHBoxLayout* appHeaderLayout = new QHBoxLayout;
    appHeaderLayout->setContentsMargins(10, 5, 10, 5);
    QLabel* logoLabel = new QLabel(this);
    QPixmap logoPixmap("logo.png"); // Use local path or resource
    if (!logoPixmap.isNull()) {
        logoLabel->setPixmap(logoPixmap.scaledToHeight(24, Qt::SmoothTransformation));
    } else {
        logoLabel->setText("🛡️ PPE 智能安全监控中枢");
        logoLabel->setStyleSheet("color: #d48806; font-weight: bold; font-size: 18px;");
    }
    QLabel* subTitle = new QLabel(" | 飞腾派 E2000Q · 异构四核", this);
    subTitle->setStyleSheet("color: #8c8273; font-size: 12px;");
    appHeaderLayout->addWidget(logoLabel);
    appHeaderLayout->addWidget(subTitle);
    appHeaderLayout->addStretch();
    
    QVBoxLayout* wrapperLayout = new QVBoxLayout;
    wrapperLayout->setContentsMargins(10, 10, 10, 10);
    wrapperLayout->addLayout(appHeaderLayout);
    wrapperLayout->addLayout(mainLayout, 1);

    mainLayout->addLayout(leftLayout, 6); // 比例 6:4
    mainLayout->addWidget(rightPanel, 4);

    QWidget* centralWidget = new QWidget(this);
    centralWidget->setLayout(wrapperLayout);
    setCentralWidget(centralWidget);
    
    resize(1366, 768); 
    setMinimumSize(1024, 600);

    // ==========================================
    // 事件绑定
    // ==========================================
    connect(btnLiveStream, &QPushButton::clicked, this, &MainWindow::onLiveStreamClicked);
    connect(btnImportVideo, &QPushButton::clicked, this, &MainWindow::onImportVideoClicked);
    connect(btnExit, &QPushButton::clicked, this, &MainWindow::onExitClicked);
    connect(SignalBridge::getInstance(), &SignalBridge::sendFrame, this, &MainWindow::updateFrame, Qt::QueuedConnection);
    connect(SignalBridge::getInstance(), &SignalBridge::sendAlarmLog, this, &MainWindow::addLogEntry, Qt::QueuedConnection);
    connect(logTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::showImageDialog);

    systemTimer = new QTimer(this);
    connect(systemTimer, &QTimer::timeout, this, &MainWindow::updateSystemStats);
    systemTimer->start(2000); 

    connect(SignalBridge::getInstance(), &SignalBridge::sendPhysicalAlarmStatus, this, [this](bool triggered){
        if (triggered) {
            badgeRPMsg->setText("● RPMsg 警报");
            badgeRPMsg->setStyleSheet("background-color: #3a1e1e; color: #ef4444; border-radius: 10px; padding: 4px 12px; border: 1px solid #ef4444; font-weight: bold; font-size: 11px;");
            videoLabel->setStyleSheet("background-color: #120f0d; border: 2px solid #ef4444; border-radius: 8px;");
            sensorFire->setText("🔥 危险"); sensorFire->setStyleSheet("color: #ef4444; font-size: 16px; font-weight: bold;");
            lightR->setStyleSheet("background-color: #ef4444; border-radius: 10px; box-shadow: 0 0 10px #ef4444;");
            lightG->setStyleSheet("background-color: #1e3a2f; border-radius: 10px;");
            safetyLevel->setText("当前状态：● 危险"); safetyLevel->setStyleSheet("color: #ef4444; font-size: 12px; font-weight: bold;");
        } else {
            badgeRPMsg->setText("● RPMsg");
            badgeRPMsg->setStyleSheet("background-color: #1e3a2f; color: #10b981; border-radius: 10px; padding: 4px 12px; border: 1px solid #10b981; font-weight: bold; font-size: 11px;");
            videoLabel->setStyleSheet("background-color: #120f0d; border: 1px solid #332b24; border-radius: 8px;");
            sensorFire->setText("● 安全"); sensorFire->setStyleSheet("color: #10b981; font-size: 16px; font-weight: bold;");
            lightG->setStyleSheet("background-color: #10b981; border-radius: 10px; box-shadow: 0 0 10px #10b981;");
            lightR->setStyleSheet("background-color: #3a1e1e; border-radius: 10px;");
            safetyLevel->setText("当前状态：● 安全"); safetyLevel->setStyleSheet("color: #10b981; font-size: 12px; font-weight: bold;");
        }
    }, Qt::QueuedConnection);
}

// 辅助函数: 创建带标题的 Card 边框
QFrame* MainWindow::makeCard(const QString& title, QLayout* inner) {
    QFrame* frame = new QFrame(this);
    frame->setObjectName("card");
    QVBoxLayout* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(15, 10, 15, 15);
    layout->setSpacing(8);
    
    QLabel* titleLabel = new QLabel(title, frame);
    titleLabel->setStyleSheet("color: #d48806; font-size: 13px; font-weight: bold; border: none;");
    layout->addWidget(titleLabel);
    
    QFrame* line = new QFrame(frame);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("background-color: #332b24; margin-bottom: 5px;");
    layout->addWidget(line);
    
    QWidget* container = new QWidget(frame);
    container->setStyleSheet("border: none; background-color: transparent;");
    container->setLayout(inner);
    layout->addWidget(container);
    
    return frame;
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
    QString fpsText = QString("FPS: %1").arg(current_fps, 0, 'f', 1);
    QRect textRect = pixmap.rect().adjusted(0, 15, -15, 0); 
    painter.setPen(QColor(0, 0, 0));
    painter.drawText(textRect.translated(2, 2), Qt::AlignTop | Qt::AlignRight, fpsText);
    painter.setPen(QColor(212, 136, 6)); // #d48806
    painter.drawText(textRect, Qt::AlignTop | Qt::AlignRight, fpsText);
    painter.end();

    videoLabel->setPixmap(pixmap);
}

void MainWindow::addLogEntry(QString type, QString time, QString imgPath) {
    logTable->insertRow(0); 
    logTable->setItem(0, 0, new QTableWidgetItem(time));
    logTable->setItem(0, 1, new QTableWidgetItem(type));
    
    QTableWidgetItem* statusItem = new QTableWidgetItem("🔍 查看");
    statusItem->setForeground(QBrush(QColor("#3b82f6")));
    
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
            QString tempText = QString("%1 %2°C").arg(temp >= 75.0 ? "↑" : "↓").arg(temp, 0, 'f', 1);
            badgeTemp->setText(tempText);
            
            if (temp >= 75.0) {
                badgeTemp->setStyleSheet("background-color: #3a1e1e; color: #ef4444; border-radius: 10px; padding: 4px 12px; border: 1px solid #ef4444; font-weight: bold; font-size: 11px;"); 
            } else if (temp >= 65.0) {
                badgeTemp->setStyleSheet("background-color: #3a2a1e; color: #d48806; border-radius: 10px; padding: 4px 12px; border: 1px solid #d48806; font-weight: bold; font-size: 11px;"); 
            } else {
                badgeTemp->setStyleSheet("background-color: #1e3a2f; color: #10b981; border-radius: 10px; padding: 4px 12px; border: 1px solid #10b981; font-weight: bold; font-size: 11px;"); 
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
                    
                    QString cpuText = QString("%1 CPU %2%").arg(usage >= 90.0 ? "↑" : "↓").arg(usage, 0, 'f', 0);
                    badgeCPU->setText(cpuText);
                    
                    if (usage >= 90.0) {
                        badgeCPU->setStyleSheet("background-color: #3a1e1e; color: #ef4444; border-radius: 10px; padding: 4px 12px; border: 1px solid #ef4444; font-weight: bold; font-size: 11px;");
                    } else if (usage >= 70.0) {
                        badgeCPU->setStyleSheet("background-color: #3a2a1e; color: #d48806; border-radius: 10px; padding: 4px 12px; border: 1px solid #d48806; font-weight: bold; font-size: 11px;");
                    } else {
                        badgeCPU->setStyleSheet("background-color: #1e3a2f; color: #10b981; border-radius: 10px; padding: 4px 12px; border: 1px solid #10b981; font-weight: bold; font-size: 11px;");
                    }
                }
                
                prevTotalTicks = total;
                prevIdleTicks = totalIdle;
            }
        }
        statFile.close();
    }
      // ==========================================
   

}
