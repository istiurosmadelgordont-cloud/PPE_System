/**
 * @file      ui_main_window.cpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#include "ui_main_window.hpp"
#include "global_context.hpp"
#include "rpmsg_node.hpp"
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <chrono>

Q_DECLARE_METATYPE(cv::Mat)

// ==========================================
// CircularScoreWidget Implementation
// ==========================================
CircularScoreWidget::CircularScoreWidget(QWidget *parent)
    : QWidget(parent), m_score(84), m_statusText("良好 - 压线40分") {
  setMinimumSize(120, 120);
}

void CircularScoreWidget::setScore(int score, const QString &statusText) {
  m_score = score;
  m_statusText = statusText;
  update();
}

void CircularScoreWidget::paintEvent(QPaintEvent *event) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  int side = qMin(width(), height());
  int x = (width() - side) / 2;
  int y = (height() - side) / 2;
  QRect rect(x + 10, y + 10, side - 20, side - 20);

  // Background circle
  QPen bgPen(QColor("#e0d5c1"), 8);
  painter.setPen(bgPen);
  painter.drawEllipse(rect);

  // Progress arc
  QColor progressColor =
      m_score >= 80 ? QColor("#10B981")
                    : (m_score >= 60 ? QColor("#F59E0B") : QColor("#EF4444"));
  QPen progPen(progressColor, 8);
  progPen.setCapStyle(Qt::RoundCap);
  painter.setPen(progPen);
  int startAngle = 90 * 16;
  int spanAngle = -int((m_score / 100.0) * 360 * 16);
  painter.drawArc(rect, startAngle, spanAngle);

  // Score text
  painter.setPen(QColor("#1f1f1f"));
  QFont font = painter.font();
  font.setPixelSize(36);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(rect, Qt::AlignCenter, QString::number(m_score));

  // Status text
  font.setPixelSize(10);
  font.setBold(false);
  painter.setFont(font);
  painter.setPen(progressColor);
  painter.drawText(QRect(x, rect.bottom() + 5, side, 20), Qt::AlignCenter,
                   m_statusText);
}

// ==========================================
// MainWindow Implementation
// ==========================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), prevTotalTicks(0), prevIdleTicks(0) {
  qRegisterMetaType<cv::Mat>("cv::Mat");

  // 初始化 DeepSeek AI 安全顾问异步 Worker (必须在早期初始化，以防 dummy log 注入时为空指针)
  dsWorker = new DeepSeekWorker(this);
  connect(dsWorker, &DeepSeekWorker::analysisStarted, this, &MainWindow::onDeepSeekAnalysisStarted);
  connect(dsWorker, &DeepSeekWorker::analysisFinished, this, &MainWindow::onDeepSeekAnalysisFinished);

  m_dsAggregationTimer = new QTimer(this);
  m_dsAggregationTimer->setSingleShot(true);
  connect(m_dsAggregationTimer, &QTimer::timeout, this, &MainWindow::triggerAggregatedDeepSeek);

  // 1. 全局样式设置 (暗黑工业风)
  this->setStyleSheet("QMainWindow { background-color: #fffaeb; font-family: Arial, sans-serif; }");
  QWidget *centralWidget = new QWidget(this);
  setCentralWidget(centralWidget);

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(15, 15, 15, 15);
  mainLayout->setSpacing(15);

  // ==========================================
  // 顶部 Header 区域
  // ==========================================
  QHBoxLayout *headerLayout = new QHBoxLayout();

  QLabel *logoIcon = new QLabel();
  QPixmap logoPix("/home/user/logo.png");
  if (logoPix.isNull()) {
    logoIcon->setText("[PPE]");
    logoIcon->setStyleSheet("font-size: 100px; color: #d97757;");
  } else {
    logoIcon->setPixmap(
        logoPix.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }

  QLabel *logoIcon2 = new QLabel();
  QPixmap logoPix2("/home/user/2phytium.jpg");
  if (!logoPix2.isNull()) {
    logoIcon2->setPixmap(logoPix2.scaled(250, 250, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  } else {
    logoIcon2->setText("[PHYTIUM]");
    logoIcon2->setStyleSheet("font-size: 100px; color: #d97757;");
  }

  headerLayout->addWidget(logoIcon);
  headerLayout->addWidget(logoIcon2);

  QLabel *teamLabel = new QLabel("CICC1004607");
  teamLabel->setStyleSheet("color: #d97757; font-size: 26px; font-weight: bold; margin-left: 20px; font-family: 'Arial', sans-serif;");
  headerLayout->addWidget(teamLabel);
  headerLayout->addStretch();

  // 状态指示器徽章
  QString badgeStyle =
      "background-color: #f0e6d2; border: 1px solid #e0d5c1; border-radius: "
      "14px; padding: 6px 14px; font-size: 12px; font-weight: bold;";

  headerRpmsgLabel = new QLabel("● RPMsg");
  headerRpmsgLabel->setStyleSheet(badgeStyle + "color: #1f1f1f;");

  headerTempLabel = new QLabel("温度 --.-°C");
  headerTempLabel->setStyleSheet(badgeStyle + "color: #d97757;");

  headerCpuLabel = new QLabel("CPU --%");
  headerCpuLabel->setStyleSheet(badgeStyle + "color: #d97757;");

  headerRamLabel = new QLabel("内存 42%"); // 占位
  headerRamLabel->setStyleSheet(badgeStyle + "color: #8B5CF6;");

  headerLayout->addWidget(headerRpmsgLabel);
  headerLayout->addSpacing(20);
  headerLayout->addWidget(headerTempLabel);
  headerLayout->addSpacing(20);
  headerLayout->addWidget(headerCpuLabel);
  headerLayout->addSpacing(20);
  headerLayout->addWidget(headerRamLabel);
  headerLayout->addSpacing(15);

  mainLayout->addLayout(headerLayout);

  // ==========================================
  // 核心主体 (左侧视频 + 右侧数据矩阵)
  // ==========================================
  QHBoxLayout *bodyLayout = new QHBoxLayout();
  bodyLayout->setSpacing(15);

  // --- 左侧：视频大屏 ---
  QFrame *videoFrame = new QFrame();
  videoFrame->setStyleSheet("QFrame { background-color: #f0e6d2; border: 1px "
                            "solid #332B25; border-radius: 8px; }");
  QVBoxLayout *videoLayout = new QVBoxLayout(videoFrame);
  videoLayout->setContentsMargins(2, 2, 2, 2);

  videoLabel = new QLabel("等待视频流接入...");
  videoLabel->setAlignment(Qt::AlignCenter);
  videoLabel->setScaledContents(true);
  videoLabel->setStyleSheet("color: #666; background-color: transparent; "
                            "border: none; font-size: 16px;");

  videoLayout->addWidget(videoLabel, 1);

  // --- 中间：DeepSeek AI 顾问区 ---
  QFrame *dsPanel = new QFrame();
  dsPanel->setStyleSheet("QFrame { background-color: #f0e6d2; border: 1px "
                         "solid #1E3A8A; border-radius: 8px; }");
  QVBoxLayout *dsLayout = new QVBoxLayout(dsPanel);
  dsLayout->setContentsMargins(15, 15, 15, 15);
  dsLayout->setSpacing(10);

  QLabel *dsTitle = new QLabel("DeepSeek AI 安全顾问");
  dsTitle->setStyleSheet("color: #fa520f; font-size: 16px; font-weight: bold; "
                         "border: none; margin-bottom: 5px;");

  dsContent = new QTextBrowser();
  dsContent->setHtml("<p>系统正在监控中...<br>AI将在此为您提供专业的实时建议。</p>");
  dsContent->setStyleSheet("QTextBrowser { color: #1f1f1f; font-size: 13px; border: none; background: transparent; }");

  dsLayout->addWidget(dsTitle, 0);
  dsLayout->addWidget(dsContent, 1);

  // --- 右侧：数据矩阵看板 ---
  QVBoxLayout *rightLayout = new QVBoxLayout();
  rightLayout->setSpacing(12);

  // Panel 1: 传感数据
  QFrame *sensorPanel = createPanelFrame();
  QVBoxLayout *sensorL = new QVBoxLayout(sensorPanel);
  QLabel *sensorTitle = new QLabel("从传感器数据矩阵 (Core 1 裸机端赋能)");
  sensorTitle->setStyleSheet("color: #5c554b; font-size: 12px; font-weight: "
                             "bold; border: none; padding-bottom: 5px;");
  sensorL->addWidget(sensorTitle);

  QGridLayout *sensorGrid = new QGridLayout();
  sensorGrid->addWidget(createSensorItem("火焰探头", sensorFlame, "#10B981"),
                        0, 0);
  sensorGrid->addWidget(createSensorItem("有害气体", sensorGas, "#10B981"),
                        0, 1);
  sensorGrid->addWidget(createSensorItem("温度", sensorTemp, "#F59E0B"), 0,
                        2);
  sensorGrid->addWidget(createSensorItem("湿度", sensorHumid, "#3B82F6"), 1,
                        0);
  sensorGrid->addWidget(
      createSensorItem("人员防爆", sensorPerson, "#8B5CF6"), 1, 1);
  sensorGrid->addWidget(createSensorItem("噪声", sensorNoise, "#10B981"), 1,
                        2);
  sensorL->addLayout(sensorGrid);

  // Panel 2: 评分与报警灯塔
  QHBoxLayout *scoreLightLayout = new QHBoxLayout();
  scoreLightLayout->setSpacing(12);

  QFrame *scorePanel = createPanelFrame();
  QVBoxLayout *scoreL = new QVBoxLayout(scorePanel);
  QLabel *scoreTitle = new QLabel("今日安全评分");
  scoreTitle->setStyleSheet(
      "color: #5c554b; font-size: 14px; font-weight: bold; border: none;");
  scoreWidget = new CircularScoreWidget();
  scoreL->addWidget(scoreTitle);
  scoreL->addWidget(scoreWidget, 0, Qt::AlignCenter);

  QFrame *lightPanel = createPanelFrame();
  QVBoxLayout *lightL = new QVBoxLayout(lightPanel);
  QLabel *lightTitle = new QLabel("三色报警灯塔");
  lightTitle->setStyleSheet(
      "color: #5c554b; font-size: 14px; font-weight: bold; border: none;");

  QHBoxLayout *lightsGrid = new QHBoxLayout();
  lightRed = new QLabel();
  lightRed->setFixedSize(24, 24);
  lightRed->setStyleSheet("background-color: #f0e6d2; border-radius: 12px; "
                          "border: 2px solid #551515;");
  lightYellow = new QLabel();
  lightYellow->setFixedSize(24, 24);
  lightYellow->setStyleSheet("background-color: #f0e6d2; border-radius: 12px; "
                             "border: 2px solid #332000;");
  lightGreen = new QLabel();
  lightGreen->setFixedSize(24, 24);
  lightGreen->setStyleSheet("background-color: #f0e6d2; border-radius: 12px; "
                            "border: 2px solid #6EE7B7;");
  lightsGrid->addWidget(lightRed);
  lightsGrid->addWidget(lightYellow);
  lightsGrid->addWidget(lightGreen);
  lightsGrid->setAlignment(Qt::AlignCenter);

  lightStatus = new QLabel("当前状态：● 安全");
  lightStatus->setStyleSheet("color: #1f1f1f; font-size: 12px; font-weight: "
                             "bold; border: none; margin-top: 5px;");
  lightStatus->setAlignment(Qt::AlignCenter);

  lightL->addWidget(lightTitle);
  lightL->addLayout(lightsGrid);
  lightL->addWidget(lightStatus);

  scoreLightLayout->addWidget(scorePanel, 1);
  scoreLightLayout->addWidget(lightPanel, 1);

  // Panel 3: 实时报警日志
  QFrame *logPanel = createPanelFrame();
  QVBoxLayout *logL = new QVBoxLayout(logPanel);
  QLabel *logTitle = new QLabel("实时报警日志");
  logTitle->setStyleSheet("color: #fa520f; font-size: 13px; font-weight: bold; "
                          "border: none; margin-bottom: 5px;");

  logTable = new QTableWidget(0, 4);
  logTable->setHorizontalHeaderLabels({"时间", "违规类型", "来源", ""});
  logTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  logTable->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::ResizeToContents);
  logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  logTable->verticalHeader()->setVisible(false);
  logTable->setShowGrid(false);
  logTable->setStyleSheet(
      "QTableWidget { background-color: transparent; color: #1f1f1f; border: "
      "none; font-size: 14px; outline: none; }"
      "QHeaderView::section { background-color: transparent; color: #5c554b; "
      "border: none; border-bottom: 1px solid #e0d5c1; font-weight: bold; "
      "padding: 4px; text-align: left; font-size: 13px; }"
      "QTableWidget::item { border-bottom: 1px solid #332B25; padding: 4px; }"
      "QTableWidget::item:selected { background-color: rgba(245, 158, 11, "
      "0.15); color: #d97757; border-radius: 4px; }");

  logL->addWidget(logTitle);
  logL->addWidget(logTable);

  // Panel 4: 底座与统计
  QHBoxLayout *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(12);

  // 4.1 左侧：本月违规统计 & 异构核心
  QFrame *statPanel = createPanelFrame();
  QVBoxLayout *statL = new QVBoxLayout(statPanel);

  QLabel *statTitle = new QLabel("本月违规统计");
  statTitle->setStyleSheet("color: #5c554b; font-size: 12px; font-weight: "
                           "bold; border: none; margin-bottom: 5px;");
  statL->addWidget(statTitle);

  auto addStatBar = [&](const QString &name, QProgressBar *&bar, QLabel *&val,
                        const QString &color) {
    QHBoxLayout *l = new QHBoxLayout();
    QLabel *n = new QLabel(name);
    n->setStyleSheet("color: #5c554b; font-size: 13px; border: none;");
    n->setFixedWidth(65);
    bar = createCustomProgressBar(color);
    val = new QLabel("0");
    val->setStyleSheet(
        QString("color: %1; font-size: 13px; font-weight: bold; border: none;")
            .arg(color));
    l->addWidget(n);
    l->addWidget(bar);
    l->addWidget(val);
    statL->addLayout(l);
  };
  addStatBar("未戴安全帽", barHelmet, lblHelmetCnt, "#EF4444");
  addStatBar("未穿背心", barVest, lblVestCnt, "#F59E0B");
  addStatBar("未戴护目镜", barGoggle, lblGoggleCnt, "#3B82F6");
  addStatBar("抽烟报警", barSmoke, lblSmokeCnt, "#8B5CF6");
  addStatBar("未戴手套", barGlove, lblGloveCnt, "#EC4899");

  statL->addSpacing(10); // 分隔线距离

  QLabel *sysTitle = new QLabel("异构核心");
  sysTitle->setStyleSheet("color: #5c554b; font-size: 12px; font-weight: bold; "
                          "border: none; margin-bottom: 5px;");
  statL->addWidget(sysTitle);

  QHBoxLayout *coreStatusLayout = new QHBoxLayout();
  coreStatusLayout->setSpacing(5);
  auto addCoreStatus = [&](const QString &text, const QString &status,
                           const QString &color) {
    QLabel *l = new QLabel(QString("<b style='color:#1f1f1f'>%1</b><br><span "
                                   "style='color:%2'>%3</span>")
                               .arg(text, color, status));
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet("background-color: #f0e6d2; border: 1px solid #e0d5c1; "
                     "border-radius: 4px; font-size: 13px; padding: 4px;");
    coreStatusLayout->addWidget(l);
  };
  addCoreStatus("Core0", "UI+IO<br>● 38°C", "#10B981");
  addCoreStatus("Core1", "裸机<br>● 2ms", "#EF4444");
  addCoreStatus("Core2+3", "YOLO<br>● 84%", "#F59E0B");
  statL->addLayout(coreStatusLayout);

  // 4.2 右侧：系统健康 & 通信安全
  QVBoxLayout *rightBottomCol = new QVBoxLayout();
  rightBottomCol->setSpacing(8);

  QFrame *sysPanel = createPanelFrame();
  QVBoxLayout *sysL = new QVBoxLayout(sysPanel);

  QLabel *healthTitle = new QLabel("系统健康");
  healthTitle->setStyleSheet("color: #5c554b; font-size: 12px; font-weight: "
                             "bold; border: none; margin-bottom: 5px;");
  sysL->addWidget(healthTitle);

  QGridLayout *metricGrid = new QGridLayout();
  metricGrid->setVerticalSpacing(2);

  auto addMetric = [&](int row, const QString &label, QLabel *&valLbl,
                       const QString &valStr, const QString &color) {
    QLabel *lbl = new QLabel("● " + label);
    lbl->setStyleSheet("color:#1f1f1f; font-size:13px; border:none;");
    valLbl = new QLabel(valStr);
    valLbl->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:bold; border:none;")
            .arg(color));
    valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    metricGrid->addWidget(lbl, row, 0);
    metricGrid->addWidget(valLbl, row, 1);
  };

  addMetric(0, "AI 推理", aiLatencyLabel, "--", "#A09080");
  addMetric(1, "摄像头", camStatusLabel, "检测中", "#A09080");
  addMetric(2, "RPMsg", rpmsgStatusLabel, "检测中", "#A09080");
  addMetric(3, "从核心跳", heartbeatStatusLabel, "检测中", "#A09080");
  addMetric(4, "系统", sysLoadLabel, "--%", "#A09080");
  sysL->addLayout(metricGrid);

  sysL->addSpacing(10);

  QLabel *commTitle = new QLabel("通信安全");
  commTitle->setStyleSheet("color: #5c554b; font-size: 12px; font-weight: "
                           "bold; border: none; margin-bottom: 5px;");
  sysL->addWidget(commTitle);

  QHBoxLayout *commLayout = new QHBoxLayout();
  crcStatusLabel = new QLabel(
      "CRC<br><span style='color:#10B981;font-weight:bold;'>0 次</span>");
  crcStatusLabel->setAlignment(Qt::AlignCenter);
  crcStatusLabel->setStyleSheet(
      "color:#10B981; font-size:13px; font-weight:bold; border:none;");

  commHeartbeatLabel =
      new QLabel("心跳<br><span style='color:#1f1f1f;'>检测中</span>");
  commHeartbeatLabel->setAlignment(Qt::AlignCenter);
  commHeartbeatLabel->setStyleSheet(
      "color:#1f1f1f; font-size:13px; font-weight:bold; border:none;");

  commLayout->addWidget(crcStatusLabel);
  commLayout->addWidget(commHeartbeatLabel);
  sysL->addLayout(commLayout);

  rightBottomCol->addWidget(sysPanel);

  // 5. 控制按钮组 (放在右下角)
  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->setSpacing(5);
  btnLiveStream = new QPushButton("● 实时监控");
  btnImportVideo = new QPushButton("导入");
  btnExit = new QPushButton("⏻ 退出");

  QString btnBase =
      "QPushButton { color: #1f1f1f; font-weight: bold; border-radius: 6px; "
      "padding: 10px 5px; border: none; font-size: 13px; }";
  btnLiveStream->setStyleSheet(
      btnBase + "QPushButton { background-color: #f0e6d2; } QPushButton:hover "
                "{ background-color: #059669; }");
  btnImportVideo->setStyleSheet(
      btnBase + "QPushButton { background-color: #d97757; } QPushButton:hover "
                "{ background-color: #D97706; }");
  btnExit->setStyleSheet(btnBase +
                         "QPushButton { background-color: #fa520f; } "
                         "QPushButton:hover { background-color: #DC2626; }");

  btnLayout->addWidget(btnLiveStream);
  btnLayout->addWidget(btnImportVideo);
  btnLayout->addWidget(btnExit);

  rightBottomCol->addLayout(btnLayout);

  bottomRow->addWidget(statPanel, 5);
  bottomRow->addLayout(rightBottomCol, 4);

  // 中间：日志与 DeepSeek 左右分栏并排
  QHBoxLayout *logAndDsLayout = new QHBoxLayout();
  logAndDsLayout->setSpacing(12);
  logAndDsLayout->addWidget(logPanel, 1);
  logAndDsLayout->addWidget(dsPanel, 1);

  // 组装 Right Layout
  rightLayout->addWidget(sensorPanel);
  rightLayout->addLayout(scoreLightLayout);
  rightLayout->addLayout(logAndDsLayout, 1);
  rightLayout->addLayout(bottomRow);

  // 组装 Body Layout (左侧 60% 纯视频，右侧 40% 综合面板)
  bodyLayout->addWidget(videoFrame, 6);
  bodyLayout->addLayout(rightLayout, 4);

  mainLayout->addLayout(bodyLayout, 1);

  // ==========================================
  // 注入占位假数据 (用于视觉展示)
  // ==========================================
  sensorFlame->setText("● 安全");
  sensorGas->setText("● 正常");
  sensorTemp->setText("28.5°C");
  sensorHumid->setText("65%");
  sensorPerson->setText("3人");
  sensorNoise->setText("62dB");

  m_cntHelmet = 0;
  m_cntVest = 0;
  m_cntGoggle = 0;
  m_cntSmoke = 0;
  m_cntGlove = 0;

  barHelmet->setValue(0);
  lblHelmetCnt->setText("0");
  barVest->setValue(0);
  lblVestCnt->setText("0");
  barGoggle->setValue(0);
  lblGoggleCnt->setText("0");
  barSmoke->setValue(0);
  lblSmokeCnt->setText("0");
  barGlove->setValue(0);
  lblGloveCnt->setText("0");

  if (scoreWidget) {
    scoreWidget->setScore(100, "优秀");
  }

  // ==========================================
  // 事件绑定与信号连接
  // ==========================================
  connect(btnLiveStream, &QPushButton::clicked, this,
          &MainWindow::onLiveStreamClicked);
  connect(btnImportVideo, &QPushButton::clicked, this,
          &MainWindow::onImportVideoClicked);
  connect(btnExit, &QPushButton::clicked, this, &MainWindow::onExitClicked);
  connect(logTable, &QTableWidget::cellDoubleClicked, this,
          &MainWindow::showImageDialog);

  connect(SignalBridge::getInstance(), &SignalBridge::sendFrame, this,
          &MainWindow::updateFrame, Qt::QueuedConnection);
  connect(SignalBridge::getInstance(), &SignalBridge::sendAlarmLog, this,
          &MainWindow::addLogEntry, Qt::QueuedConnection);
  connect(SignalBridge::getInstance(), &SignalBridge::sendCrcError, this,
          &MainWindow::updateCrcError, Qt::QueuedConnection);
  connect(SignalBridge::getInstance(), &SignalBridge::sendAiMetrics, this,
          [this](int latencyMs, int personCount) {
            if (aiLatencyLabel) {
              aiLatencyLabel->setText(QString("%1ms").arg(latencyMs));
            }
            if (sensorPerson) {
              sensorPerson->setText(QString("%1人").arg(personCount));
            }
          },
          Qt::QueuedConnection);
  connect(
      SignalBridge::getInstance(), &SignalBridge::sendPhysicalAlarmStatus, this,
      [this](bool triggered) {
        m_fireAlerted = triggered;
        if (triggered) {
          headerRpmsgLabel->setText("● RPMsg 警报");
          headerRpmsgLabel->setStyleSheet(
              "background-color: #fa520f; color: #1f1f1f; border: 1px solid "
              "#7F1D1D; border-radius: 14px; padding: 6px 14px; font-size: "
              "12px; font-weight: bold;");
          videoLabel->setStyleSheet(
              "color: #1f1f1f; background-color: rgba(239, 68, 68, 0.15); "
              "border: 5px solid #fa520f; font-size: 16px;");
          sensorFlame->setText("警报");
          sensorFlame->setStyleSheet("color: #fa520f; font-weight: bold; "
                                     "font-size: 18px; border: none;");
          scoreWidget->setScore(45, "危险 - 立即排查");
        } else {
          headerRpmsgLabel->setText("● RPMsg 正常");
          headerRpmsgLabel->setStyleSheet(
              "background-color: #f0e6d2; color: #1f1f1f; border: 1px solid "
              "#4A3825; border-radius: 14px; padding: 6px 14px; font-size: "
              "12px; font-weight: bold;");
          videoLabel->setStyleSheet(
              "color: #666; background-color: transparent; border: none; "
              "font-size: 16px;");
          sensorFlame->setText("● 安全");
          sensorFlame->setStyleSheet("color: #10B981; font-weight: bold; "
                                     "font-size: 18px; border: none;");
          scoreWidget->setScore(84, "良好 - 压线40分");
        }
        updateThreeColorLights();
      },
      Qt::QueuedConnection);

  connect(SignalBridge::getInstance(), &SignalBridge::sendGasAlarmStatus, this,
          [this](bool alarmed) {
            if (sensorGas) {
              if (alarmed) {
                sensorGas->setText("● 警报");
                sensorGas->setStyleSheet("color: #fa520f; font-weight: bold; font-size: 18px; border: none;");
                if (!m_gasAlerted) {
                  m_gasAlerted = true;
                  addLogEntry("有害气体报警", QDateTime::currentDateTime().toString("HH:mm:ss"), "");
                }
              } else {
                sensorGas->setText("● 正常");
                sensorGas->setStyleSheet("color: #10B981; font-weight: bold; font-size: 18px; border: none;");
                m_gasAlerted = false;
              }
            }
            updateThreeColorLights();
          },
          Qt::QueuedConnection);

  connect(SignalBridge::getInstance(), &SignalBridge::sendEnvMetrics, this,
          [this](double temp, double humid) {
            if (sensorTemp) {
              sensorTemp->setText(QString("%1 °C").arg(temp, 0, 'f', 1));
              if (temp >= 35.0) {
                sensorTemp->setStyleSheet("color: #fa520f; font-weight: bold; font-size: 18px; border: none;");
              } else {
                sensorTemp->setStyleSheet("color: #d97757; font-weight: bold; font-size: 18px; border: none;");
              }
            }
            if (sensorHumid) {
              sensorHumid->setText(QString("%1%").arg(humid, 0, 'f', 1));
            }
            // 温湿度过高判定（湿度≥80%报警，<75%释放；温度≥35°C报警，<33°C释放）
            if (temp >= 35.0 || humid >= 80.0) {
              if (!m_tempHumidAlerted) {
                m_tempHumidAlerted = true;
                addLogEntry("温湿度过高", QDateTime::currentDateTime().toString("HH:mm:ss"), "");
              }
            } else if (temp < 33.0 && humid < 75.0) {
              m_tempHumidAlerted = false; // 迟滞释放，防抖
            }
            updateThreeColorLights();
          },
          Qt::QueuedConnection);

  connect(SignalBridge::getInstance(), &SignalBridge::sendEnvError, this,
          [this]() {
            if (sensorTemp) {
              sensorTemp->setText("断开");
              sensorTemp->setStyleSheet("color: #fa520f; font-weight: bold; font-size: 18px; border: none;");
            }
            if (sensorHumid) {
              sensorHumid->setText("断开");
              sensorHumid->setStyleSheet("color: #fa520f; font-weight: bold; font-size: 18px; border: none;");
            }
          },
          Qt::QueuedConnection);

  m_aiAlarmHoldTimer = new QTimer(this);
  m_aiAlarmHoldTimer->setSingleShot(true);
  connect(m_aiAlarmHoldTimer, &QTimer::timeout, this, [this]() {
    m_aiAlerted = false;
    updateThreeColorLights();
  });

  connect(SignalBridge::getInstance(), &SignalBridge::sendAiAlarmStatus, this,
          [this](bool alarmed) {
            if (alarmed) {
              printf("[UI收到] sendAiAlarmStatus(TRUE) -> m_aiAlerted=true, 启动3秒维持\n");
              fflush(stdout);
              m_aiAlerted = true;
              m_aiAlarmHoldTimer->start(3000); // 维持3秒警告状态
              updateThreeColorLights();
            } else {
              // 只有当维持定时器未运行（已经超时）时，才直接拉低警告状态回到绿色
              if (!m_aiAlarmHoldTimer->isActive()) {
                m_aiAlerted = false;
                updateThreeColorLights();
              }
            }
          },
          Qt::QueuedConnection);

  systemTimer = new QTimer(this);
  connect(systemTimer, &QTimer::timeout, this, &MainWindow::updateSystemStats);
   systemTimer->start(2000);

  updateThreeColorLights();

  resize(1366, 768); // 设置为接近 16:9 工业大屏比例
}

MainWindow::~MainWindow() {}

// 辅助 UI 创建函数
QFrame *MainWindow::createPanelFrame() {
  QFrame *frame = new QFrame();
  frame->setStyleSheet("QFrame { background-color: #f0e6d2; border: 1px solid "
                       "#4A3825; border-radius: 8px; }");
  return frame;
}

QWidget *MainWindow::createSensorItem(const QString &title, QLabel *&valueLabel,
                                      const QString &color) {
  QWidget *w = new QWidget();
  w->setStyleSheet("border: none; background: transparent;");
  QVBoxLayout *l = new QVBoxLayout(w);
  l->setContentsMargins(5, 5, 5, 5);
  l->setSpacing(4);

  QLabel *t = new QLabel(title);
  t->setStyleSheet("color: #5c554b; font-size: 18px; border: none;");
  t->setAlignment(Qt::AlignCenter);

  valueLabel = new QLabel("--");
  valueLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; font-size: 18px; border: none;")
          .arg(color));
  valueLabel->setAlignment(Qt::AlignCenter);

  l->addWidget(t);
  l->addWidget(valueLabel);
  return w;
}

QProgressBar *MainWindow::createCustomProgressBar(const QString &color) {
  QProgressBar *bar = new QProgressBar();
  bar->setFixedHeight(8);
  bar->setTextVisible(false);
  bar->setStyleSheet(
      QString(
          "QProgressBar { background-color: #f0e6d2; border: none; "
          "border-radius: 4px; }"
          "QProgressBar::chunk { background-color: %1; border-radius: 4px; }")
          .arg(color));
  return bar;
}

void MainWindow::updateFrame(const cv::Mat &frame) {
  if (frame.empty())
    return;

  static auto last_time = std::chrono::steady_clock::now();
  static int frames = 0;
  static double current_fps = 0.0;
  static bool rec_blink = false;

  frames++;
  auto now = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(now - last_time).count();

  if (elapsed >= 1.0) {
    current_fps = frames / elapsed;
    frames = 0;
    last_time = now;
    rec_blink = !rec_blink; // 每秒切换一次闪烁状态
  }

  // 【优化：零拷贝】使用 const_cast 引用底层共享图像内存，彻底消灭 Mat 的 clone 深拷贝开销
  cv::Mat &displayFrame = const_cast<cv::Mat&>(frame);

  // 绘制高科技感 FPS
  char fps_buf[32];
  sprintf(fps_buf, "FPS: %.1f", current_fps);
  cv::rectangle(displayFrame, cv::Rect(displayFrame.cols - 110, 10, 100, 30),
                cv::Scalar(11, 158, 245), -1); // BGR: F59E0B
  cv::putText(displayFrame, fps_buf, cv::Point(displayFrame.cols - 100, 32),
              cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);

  // 绘制左上角 REC 录制指示
  if (rec_blink) {
    cv::circle(displayFrame, cv::Point(30, 25), 6, cv::Scalar(68, 68, 239),
               -1); // BGR: EF4444
  }
  cv::putText(displayFrame, "REC", cv::Point(45, 32), cv::FONT_HERSHEY_SIMPLEX,
              0.6, cv::Scalar(68, 68, 239), 2);

  QImage img(displayFrame.data, displayFrame.cols, displayFrame.rows,
             displayFrame.step, QImage::Format_BGR888);
  videoLabel->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::addLogEntry(QString type, QString time, QString imgPath) {
  // 英文标签翻译为中文
  if (type == "Without Helmet") type = "未戴安全帽";
  else if (type == "Without Glass") type = "未戴护目镜";
  else if (type == "Without Glove") type = "未戴手套";
  else if (type == "Without Safety Vest") type = "未穿背心";
  else if (type == "底层物理火警") type = "底层火警探头";

  logTable->insertRow(0);
  logTable->setItem(0, 0, new QTableWidgetItem(time));
  logTable->setItem(0, 1, new QTableWidgetItem(type));

  // 模拟来源
  QString source = "AI 视觉";
  if (type == "未戴安全帽" || type == "未穿背心" || type == "未戴护目镜" || type == "未戴手套")
    source = "AI 视觉";
  else if (type == "底座火焰触发" || type == "底层火警探头")
    source = "从核 GPIO";
  else
    source = "多模态感知";

  logTable->setItem(0, 2, new QTableWidgetItem(source));

  QTableWidgetItem *statusItem = new QTableWidgetItem("查看");
  statusItem->setTextAlignment(Qt::AlignCenter);
  statusItem->setForeground(QBrush(QColor("#fa520f")));
  statusItem->setData(Qt::UserRole, imgPath);
  logTable->setItem(0, 3, statusItem);

  if (logTable->rowCount() > 50) {
    logTable->removeRow(50);
  }

  // 递增对应类型的违规计数器并更新进度条及安全健康度评分
  if (type == "未戴安全帽") {
    m_cntHelmet++;
    if (lblHelmetCnt) lblHelmetCnt->setText(QString::number(m_cntHelmet));
    if (barHelmet) barHelmet->setValue(std::min(m_cntHelmet * 5, 100));
  } else if (type == "未穿背心") {
    m_cntVest++;
    if (lblVestCnt) lblVestCnt->setText(QString::number(m_cntVest));
    if (barVest) barVest->setValue(std::min(m_cntVest * 5, 100));
  } else if (type == "未戴护目镜") {
    m_cntGoggle++;
    if (lblGoggleCnt) lblGoggleCnt->setText(QString::number(m_cntGoggle));
    if (barGoggle) barGoggle->setValue(std::min(m_cntGoggle * 5, 100));
  } else if (type == "抽烟报警") {
    m_cntSmoke++;
    if (lblSmokeCnt) lblSmokeCnt->setText(QString::number(m_cntSmoke));
    if (barSmoke) barSmoke->setValue(std::min(m_cntSmoke * 5, 100));
  } else if (type == "未戴手套") {
    m_cntGlove++;
    if (lblGloveCnt) lblGloveCnt->setText(QString::number(m_cntGlove));
    if (barGlove) barGlove->setValue(std::min(m_cntGlove * 5, 100));
  }

  int total_violations = m_cntHelmet + m_cntVest + m_cntGoggle + m_cntSmoke + m_cntGlove;
  int new_score = 100 - (total_violations * 5);
  if (new_score < 0) new_score = 0;

  QString statusText = "优秀";
  if (new_score >= 90) statusText = "优秀";
  else if (new_score >= 80) statusText = "良好";
  else if (new_score >= 60) statusText = "一般";
  else statusText = "危险 - 立即排查";

  if (scoreWidget) {
    scoreWidget->setScore(new_score, statusText);
  }

  updateThreeColorLights();

  // 触发 DeepSeek AI 安全顾问合并分析 (1.5秒缓存时间)
  if (type == "未戴安全帽" || type == "未穿背心" || type == "未戴护目镜" || 
      type == "未戴手套" || type == "抽烟报警" || type == "底层火警探头" || 
      type == "底座火焰触发" || type == "有害气体报警" || type == "气体报警" || 
      type == "温湿度过高") {
    m_pendingViolations.append(type);
    m_dsAggregationTimer->start(1500); // 1.5秒延迟，合并同一批或相邻帧的所有违规
  }
}

void MainWindow::showDeepSeekSuggestion(const QString &text) {
  // 最简单的静态文本更新，绝不会引发内存崩溃
  if (dsContent) {
    dsContent->setHtml(text);
  }
}

void MainWindow::triggerAggregatedDeepSeek() {
  if (m_pendingViolations.isEmpty()) return;

  // 去重并合并违规信息
  QStringList uniqueViolations;
  for (const QString &v : m_pendingViolations) {
    if (!uniqueViolations.contains(v)) {
      uniqueViolations.append(v);
    }
  }
  m_pendingViolations.clear();

  // 拼接为一条合并的违规事件
  QString combinedType = uniqueViolations.join("、");
  dsWorker->requestAdvice(combinedType);
}

void MainWindow::onDeepSeekAnalysisStarted() {
  if (dsContent) {
    dsContent->setHtml("<p style='color: #fa520f; font-weight: bold;'>DeepSeek 正在分析中...</p>");
  }
}

void MainWindow::onDeepSeekAnalysisFinished(const QString &advice) {
    dsContent->setHtml(advice);
    
    // 【端云联动】触发飞书 Webhook 推送脚本
    // 我们在后台静默运行 python 脚本，把 DeepSeek 的文字传给它
    QString pyCmd = QString("nohup python3 ../scripts/feishu_push.py \"%1\" >/dev/null 2>&1 &").arg(advice);
    system(pyCmd.toUtf8().constData());
}

void MainWindow::showImageDialog(int row, int column) {
  QTableWidgetItem *item = logTable->item(row, 3);
  if (!item)
    return;
  QString imgPath = item->data(Qt::UserRole).toString();

  QImage img(imgPath);
  if (img.isNull()) {
    // QMessageBox::warning(this, "提示",
    // "图片已被系统自动清理或尚未写入磁盘！");
    // 为了美观展示，如果没有图片我们暂不弹丑陋的系统框
    return;
  }

  QDialog dialog(this);
  dialog.setWindowTitle("抓拍证据回放 - " + logTable->item(row, 0)->text());
  dialog.setStyleSheet(
      "background-color: #f0e6d2; color: #1f1f1f; border: 1px solid #e0d5c1;");

  QLabel *imgLabel = new QLabel(&dialog);
  imgLabel->setPixmap(QPixmap::fromImage(img).scaled(
      800, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation));

  QVBoxLayout *layout = new QVBoxLayout(&dialog);
  layout->addWidget(imgLabel);

  dialog.exec();
}

void MainWindow::onLiveStreamClicked() {
  if (current_source_mode == 0)
    return;
  current_source_mode = 0;
  source_changed = true;
  btnLiveStream->setText("● 监控中...");
  btnLiveStream->setStyleSheet(
      "QPushButton { color: #1f1f1f; font-weight: bold; border-radius: 6px; "
      "padding: 10px; border: none; font-size: 14px; background-color: "
      "#059669; }");
  btnImportVideo->setText("导入");
}

void MainWindow::onImportVideoClicked() {
  QString fileName = QFileDialog::getOpenFileName(this, "选择测试视频", "",
                                                  "Video Files (*.mp4 *.avi)");
  if (fileName.isEmpty())
    return;

  video_path = fileName.toStdString();
  current_source_mode = 1;
  source_changed = true;
  btnImportVideo->setText("分析中...");
  btnLiveStream->setText("● 实时监控");
  btnLiveStream->setStyleSheet(
      "QPushButton { color: #1f1f1f; font-weight: bold; border-radius: 6px; "
      "padding: 10px; border: none; font-size: 14px; background-color: "
      "#10B981; }");
}

void MainWindow::onExitClicked() {
  is_running = false;
  QApplication::quit();
}

void MainWindow::updateSystemStats() {
  // 1. 读取温度
  QFile tempFile("/sys/class/thermal/thermal_zone0/temp");
  if (tempFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&tempFile);
    QString line = in.readLine();
    if (!line.isNull()) {
      double temp = line.toDouble() / 1000.0;
      headerTempLabel->setText(QString("温度 %1 °C").arg(temp, 0, 'f', 1));

      QString badgeStyle = "background-color: #f0e6d2; border: 1px solid "
                           "#4A3825; border-radius: 14px; padding: 6px 14px; "
                           "font-size: 12px; font-weight: bold;";
      if (temp >= 75.0) {
        headerTempLabel->setStyleSheet(badgeStyle + "color: #fa520f;");
      } else if (temp >= 65.0) {
        headerTempLabel->setStyleSheet(badgeStyle + "color: #d97757;");
      } else {
        headerTempLabel->setStyleSheet(badgeStyle + "color: #1f1f1f;");
      }
    }
    tempFile.close();
  }

  // 2. 读取 CPU 使用率
  QFile statFile("/proc/stat");
  if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&statFile);
    QString line = in.readLine();
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

        if (prevTotalTicks != 0) {
          unsigned long long totalDiff = total - prevTotalTicks;
          unsigned long long idleDiff = totalIdle - prevIdleTicks;
          double usage = 100.0 * (totalDiff - idleDiff) / (double)totalDiff;

          headerCpuLabel->setText(QString("CPU %1%").arg(usage, 0, 'f', 1));
          if (sysLoadLabel) {
            sysLoadLabel->setText(QString("%1%").arg(usage, 0, 'f', 0));
          }

          QString badgeStyle = "background-color: #f0e6d2; border: 1px solid "
                               "#4A3825; border-radius: 14px; padding: 6px "
                               "14px; font-size: 12px; font-weight: bold;";
          if (usage >= 90.0)
            headerCpuLabel->setStyleSheet(badgeStyle + "color: #fa520f;");
          else if (usage >= 70.0)
            headerCpuLabel->setStyleSheet(badgeStyle + "color: #d97757;");
          else
            headerCpuLabel->setStyleSheet(badgeStyle + "color: #1f1f1f;");
        }
        prevTotalTicks = total;
        prevIdleTicks = totalIdle;
      }
    }
    statFile.close();
  }

  // 3. 读取内存 (RAM) 使用率
  QFile memFile("/proc/meminfo");
  if (memFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream in(&memFile);
    double totalMem = 0.0;
    double freeMem = 0.0;
    double buffersMem = 0.0;
    double cachedMem = 0.0;
    while (!in.atEnd()) {
      QString line = in.readLine();
      QStringList parts = line.simplified().split(' ');
      if (parts.size() >= 2) {
        if (parts[0] == "MemTotal:") {
          totalMem = parts[1].toDouble();
        } else if (parts[0] == "MemFree:") {
          freeMem = parts[1].toDouble();
        } else if (parts[0] == "Buffers:") {
          buffersMem = parts[1].toDouble();
        } else if (parts[0] == "Cached:") {
          cachedMem = parts[1].toDouble();
        }
      }
    }
    memFile.close();
    if (totalMem > 0) {
      double usedMem = totalMem - freeMem - buffersMem - cachedMem;
      double ramUsage = 100.0 * usedMem / totalMem;
      if (ramUsage < 0) ramUsage = 0;
      headerRamLabel->setText(QString("内存 %1%").arg(ramUsage, 0, 'f', 0));
    }
  }

  // 4. 读取并更新 RPMsg 通信及从核状态
  bool connected = RPMsgController::getInstance().isConnected();
  if (rpmsgStatusLabel) {
    rpmsgStatusLabel->setText(connected ? "连接" : "断开");
    rpmsgStatusLabel->setStyleSheet(connected ?
        "color:#10B981; font-size:13px; font-weight:bold; border:none;" :
        "color:#EF4444; font-size:13px; font-weight:bold; border:none;");
  }
  if (heartbeatStatusLabel) {
    heartbeatStatusLabel->setText(connected ? "正常" : "离线");
    heartbeatStatusLabel->setStyleSheet(connected ?
        "color:#10B981; font-size:13px; font-weight:bold; border:none;" :
        "color:#EF4444; font-size:13px; font-weight:bold; border:none;");
  }
  // 通信安全区心跳标签同步
  if (commHeartbeatLabel) {
    commHeartbeatLabel->setText(connected ?
        "心跳<br><span style='color:#10B981;font-weight:bold;'>在线</span>" :
        "心跳<br><span style='color:#EF4444;font-weight:bold;'>离线</span>");
  }

  // 5. 摄像头状态检测（基于帧更新时间戳判定）
  if (camStatusLabel) {
    static auto lastFrameCheck = std::chrono::steady_clock::now();
    bool camOk = false;
    // 只有在硬件直连模式(0)且画面非空时，才认为物理摄像头在线
    if (videoLabel && videoLabel->pixmap() && !videoLabel->pixmap()->isNull() && current_source_mode.load() == 0) {
      camOk = true;
    }
    camStatusLabel->setText(camOk ? "在线" : "离线");
    camStatusLabel->setStyleSheet(camOk ?
        "color:#10B981; font-size:13px; font-weight:bold; border:none;" :
        "color:#EF4444; font-size:13px; font-weight:bold; border:none;");
  }

  // 6. 噪声传感器模拟（无真实传感器）
  static int counter = 0;
  counter++;
  if (sensorNoise) {
    int val = 58 + ((counter + 3) % 7);
    sensorNoise->setText(QString("%1dB").arg(val));
  }
}

void MainWindow::updateThreeColorLights() {
  bool has_emergency = m_fireAlerted || m_gasAlerted;
  bool has_warning = m_aiAlerted || m_tempHumidAlerted;

  static bool prev_emergency = false;
  static bool prev_warning = false;
  static bool first_run = true;

  // 状态未改变时直接返回，避免重绘与 CSS 重构引起的卡顿
  if (!first_run && has_emergency == prev_emergency && has_warning == prev_warning) {
    return;
  }
  first_run = false;
  prev_emergency = has_emergency;
  prev_warning = has_warning;

  printf("[ThreeColorLights] has_emergency: %d (fire: %d, gas: %d), has_warning: %d (aiAlerted: %d, Helmet: %d, Vest: %d, Goggle: %d, Smoke: %d, Glove: %d, tempHumidAlerted: %d)\n",
         (int)has_emergency, (int)m_fireAlerted, (int)m_gasAlerted,
         (int)has_warning, (int)m_aiAlerted, m_cntHelmet, m_cntVest, m_cntGoggle, m_cntSmoke, m_cntGlove, (int)m_tempHumidAlerted);
  fflush(stdout);

  if (has_emergency) {
    // 红色紧急报警闪烁 (亮红)
    if (lightRed) lightRed->setStyleSheet("background-color: #fa520f; border-radius: 12px; border: 2px solid #FCA5A5;");
    if (lightYellow) lightYellow->setStyleSheet("background-color: #e0d5c1; border-radius: 12px; border: 2px solid #d1c7b4;");
    if (lightGreen) lightGreen->setStyleSheet("background-color: #e0d5c1; border-radius: 12px; border: 2px solid #d1c7b4;");
    if (lightStatus) {
      lightStatus->setText("当前状态：● 紧急报警");
      lightStatus->setStyleSheet("color: #fa520f; font-size: 14px; font-weight: bold; border: none; margin-top: 5px;");
    }
  } else if (has_warning) {
    // 黄色一般告警 (亮黄)
    if (lightRed) lightRed->setStyleSheet("background-color: #e0d5c1; border-radius: 12px; border: 2px solid #d1c7b4;");
    if (lightYellow) lightYellow->setStyleSheet("background-color: #d97757; border-radius: 12px; border: 2px solid #FCD34D;");
    if (lightGreen) lightGreen->setStyleSheet("background-color: #e0d5c1; border-radius: 12px; border: 2px solid #d1c7b4;");
    if (lightStatus) {
      lightStatus->setText("当前状态：● 一般告警");
      lightStatus->setStyleSheet("color: #d97757; font-size: 14px; font-weight: bold; border: none; margin-top: 5px;");
    }
  } else {
    // 绿色完全正常 (亮绿)
    if (lightRed) lightRed->setStyleSheet("background-color: #e0d5c1; border-radius: 12px; border: 2px solid #d1c7b4;");
    if (lightYellow) lightYellow->setStyleSheet("background-color: #e0d5c1; border-radius: 12px; border: 2px solid #d1c7b4;");
    if (lightGreen) lightGreen->setStyleSheet("background-color: #10B981; border-radius: 12px; border: 2px solid #34D399;");
    if (lightStatus) {
      lightStatus->setText("当前状态：● 安全");
      lightStatus->setStyleSheet("color: #1f1f1f; font-size: 14px; font-weight: bold; border: none; margin-top: 5px;");
    }
  }
}

void MainWindow::updateCrcError(int count) {
  if (crcStatusLabel) {
    crcStatusLabel->setText(QString("CRC<br><span style='color:#EF4444;font-weight:bold;'>%1 次</span>").arg(count));
  }
}
