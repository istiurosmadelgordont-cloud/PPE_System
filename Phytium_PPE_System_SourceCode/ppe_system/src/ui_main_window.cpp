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
  QPen bgPen(QColor("#2C2A29"), 8);
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
  painter.setPen(QColor("#FFFFFF"));
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

  // 1. 全局样式设置 (暗黑工业风)
  this->setStyleSheet("QMainWindow { background-color: #1A1615; font-family: "
                      "'Segoe UI', 'Microsoft YaHei', sans-serif; }");
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
    logoIcon->setText("🛡️");
    logoIcon->setStyleSheet("font-size: 28px; color: #F59E0B;");
  } else {
    logoIcon->setPixmap(
        logoPix.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  }

  QLabel *titleLabel = new QLabel("PPE 智能安全监控中枢");
  titleLabel->setStyleSheet(
      "color: #F59E0B; font-size: 22px; font-weight: bold; margin-left: 5px;");

  QLabel *subtitleLabel = new QLabel("飞腾派 E2000Q · 异构四核");
  subtitleLabel->setStyleSheet(
      "color: #888888; font-size: 13px; margin-left: 15px; margin-top: 5px;");

  headerLayout->addWidget(logoIcon);
  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(subtitleLabel);
  headerLayout->addStretch();

  // 状态指示器徽章
  QString badgeStyle =
      "background-color: #25201F; border: 1px solid #4A3825; border-radius: "
      "14px; padding: 6px 14px; font-size: 12px; font-weight: bold;";

  headerRpmsgLabel = new QLabel("● RPMsg");
  headerRpmsgLabel->setStyleSheet(badgeStyle + "color: #10B981;");

  headerTempLabel = new QLabel("🌡️ --.-°C");
  headerTempLabel->setStyleSheet(badgeStyle + "color: #F59E0B;");

  headerCpuLabel = new QLabel("⚡ CPU --%");
  headerCpuLabel->setStyleSheet(badgeStyle + "color: #F59E0B;");

  headerRamLabel = new QLabel("💾 42%"); // 占位
  headerRamLabel->setStyleSheet(badgeStyle + "color: #8B5CF6;");

  headerLayout->addWidget(headerRpmsgLabel);
  headerLayout->addSpacing(8);
  headerLayout->addWidget(headerTempLabel);
  headerLayout->addSpacing(8);
  headerLayout->addWidget(headerCpuLabel);
  headerLayout->addSpacing(8);
  headerLayout->addWidget(headerRamLabel);

  mainLayout->addLayout(headerLayout);

  // ==========================================
  // 核心主体 (左侧视频 + 右侧数据矩阵)
  // ==========================================
  QHBoxLayout *bodyLayout = new QHBoxLayout();
  bodyLayout->setSpacing(15);

  // --- 左侧：视频大屏 ---
  QFrame *videoFrame = new QFrame();
  videoFrame->setStyleSheet("QFrame { background-color: #12100F; border: 1px "
                            "solid #332B25; border-radius: 8px; }");
  QVBoxLayout *videoLayout = new QVBoxLayout(videoFrame);
  videoLayout->setContentsMargins(2, 2, 2, 2);

  videoLabel = new QLabel("等待视频流接入...");
  videoLabel->setAlignment(Qt::AlignCenter);
  videoLabel->setScaledContents(true);
  videoLabel->setStyleSheet("color: #666; background-color: transparent; "
                            "border: none; font-size: 16px;");

  QLabel *videoFooter =
      new QLabel("© YOLOv8 INT8  |  ☄️ ByteTrack  |  📐 320x320  |  🧠 NCNN");
  videoFooter->setAlignment(Qt::AlignCenter);
  videoFooter->setStyleSheet(
      "color: #A09080; font-size: 11px; font-weight: bold; background: "
      "rgba(0,0,0,0.4); padding: 6px; border: none; border-bottom-left-radius: "
      "8px; border-bottom-right-radius: 8px;");

  videoLayout->addWidget(videoLabel, 1);
  videoLayout->addWidget(videoFooter, 0);

  // --- 右侧：数据矩阵看板 ---
  QVBoxLayout *rightLayout = new QVBoxLayout();
  rightLayout->setSpacing(12);
  int rightPanelWidth = 420;

  // Panel 1: 传感数据
  QFrame *sensorPanel = createPanelFrame();
  sensorPanel->setMaximumWidth(rightPanelWidth);
  QVBoxLayout *sensorL = new QVBoxLayout(sensorPanel);
  QLabel *sensorTitle = new QLabel("📡 从传感数据矩阵 (Core 1 裸机端赋能)");
  sensorTitle->setStyleSheet("color: #A09080; font-size: 12px; font-weight: "
                             "bold; border: none; padding-bottom: 5px;");
  sensorL->addWidget(sensorTitle);

  QGridLayout *sensorGrid = new QGridLayout();
  sensorGrid->addWidget(createSensorItem("🔥 火焰探头", sensorFlame, "#10B981"),
                        0, 0);
  sensorGrid->addWidget(createSensorItem("💨 有害气体", sensorGas, "#10B981"),
                        0, 1);
  sensorGrid->addWidget(createSensorItem("🌡️ 温度", sensorTemp, "#F59E0B"), 0,
                        2);
  sensorGrid->addWidget(createSensorItem("💧 湿度", sensorHumid, "#3B82F6"), 1,
                        0);
  sensorGrid->addWidget(
      createSensorItem("👤 人员防爆", sensorPerson, "#8B5CF6"), 1, 1);
  sensorGrid->addWidget(createSensorItem("🔊 噪声", sensorNoise, "#10B981"), 1,
                        2);
  sensorL->addLayout(sensorGrid);

  // Panel 2: 评分与报警灯塔
  QHBoxLayout *scoreLightLayout = new QHBoxLayout();
  scoreLightLayout->setSpacing(12);

  QFrame *scorePanel = createPanelFrame();
  QVBoxLayout *scoreL = new QVBoxLayout(scorePanel);
  QLabel *scoreTitle = new QLabel("🏆 今日安全评分");
  scoreTitle->setStyleSheet(
      "color: #A09080; font-size: 12px; font-weight: bold; border: none;");
  scoreWidget = new CircularScoreWidget();
  scoreL->addWidget(scoreTitle);
  scoreL->addWidget(scoreWidget, 0, Qt::AlignCenter);

  QFrame *lightPanel = createPanelFrame();
  QVBoxLayout *lightL = new QVBoxLayout(lightPanel);
  QLabel *lightTitle = new QLabel("🚨 三色报警灯塔");
  lightTitle->setStyleSheet(
      "color: #A09080; font-size: 12px; font-weight: bold; border: none;");

  QHBoxLayout *lightsGrid = new QHBoxLayout();
  lightRed = new QLabel();
  lightRed->setFixedSize(24, 24);
  lightRed->setStyleSheet("background-color: #551515; border-radius: 12px; "
                          "border: 2px solid #551515;");
  lightYellow = new QLabel();
  lightYellow->setFixedSize(24, 24);
  lightYellow->setStyleSheet("background-color: #332000; border-radius: 12px; "
                             "border: 2px solid #332000;");
  lightGreen = new QLabel();
  lightGreen->setFixedSize(24, 24);
  lightGreen->setStyleSheet("background-color: #10B981; border-radius: 12px; "
                            "border: 2px solid #6EE7B7;");
  lightsGrid->addWidget(lightRed);
  lightsGrid->addWidget(lightYellow);
  lightsGrid->addWidget(lightGreen);
  lightsGrid->setAlignment(Qt::AlignCenter);

  QLabel *lightStatus = new QLabel("当前状态：● 安全");
  lightStatus->setStyleSheet("color: #10B981; font-size: 12px; font-weight: "
                             "bold; border: none; margin-top: 5px;");
  lightStatus->setAlignment(Qt::AlignCenter);

  lightL->addWidget(lightTitle);
  lightL->addLayout(lightsGrid);
  lightL->addWidget(lightStatus);

  scoreLightLayout->addWidget(scorePanel, 1);
  scoreLightLayout->addWidget(lightPanel, 1);

  // Panel 3: 实时报警日志
  QFrame *logPanel = createPanelFrame();
  logPanel->setMaximumWidth(rightPanelWidth);
  QVBoxLayout *logL = new QVBoxLayout(logPanel);
  QLabel *logTitle = new QLabel("🔴 实时报警日志");
  logTitle->setStyleSheet("color: #EF4444; font-size: 13px; font-weight: bold; "
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
      "QTableWidget { background-color: transparent; color: #D4D4D4; border: "
      "none; font-size: 12px; outline: none; }"
      "QHeaderView::section { background-color: transparent; color: #A09080; "
      "border: none; border-bottom: 1px solid #4A3825; font-weight: bold; "
      "padding: 4px; text-align: left; }"
      "QTableWidget::item { border-bottom: 1px solid #332B25; padding: 4px; }"
      "QTableWidget::item:selected { background-color: rgba(245, 158, 11, "
      "0.15); color: #F59E0B; border-radius: 4px; }");

  logL->addWidget(logTitle);
  logL->addWidget(logTable);

  // Panel 4: 底座与统计
  QHBoxLayout *bottomRow = new QHBoxLayout();
  bottomRow->setSpacing(12);

  // 4.1 左侧：本月违规统计 & 异构核心
  QFrame *statPanel = createPanelFrame();
  QVBoxLayout *statL = new QVBoxLayout(statPanel);

  QLabel *statTitle = new QLabel("📊 本月违规统计");
  statTitle->setStyleSheet("color: #A09080; font-size: 12px; font-weight: "
                           "bold; border: none; margin-bottom: 5px;");
  statL->addWidget(statTitle);

  auto addStatBar = [&](const QString &name, QProgressBar *&bar, QLabel *&val,
                        const QString &color) {
    QHBoxLayout *l = new QHBoxLayout();
    QLabel *n = new QLabel(name);
    n->setStyleSheet("color: #A09080; font-size: 11px; border: none;");
    n->setFixedWidth(65);
    bar = createCustomProgressBar(color);
    val = new QLabel("0");
    val->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: bold; border: none;")
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

  statL->addSpacing(10); // 分隔线距离

  QLabel *sysTitle = new QLabel("💻 异构核心");
  sysTitle->setStyleSheet("color: #A09080; font-size: 12px; font-weight: bold; "
                          "border: none; margin-bottom: 5px;");
  statL->addWidget(sysTitle);

  QHBoxLayout *coreStatusLayout = new QHBoxLayout();
  coreStatusLayout->setSpacing(5);
  auto addCoreStatus = [&](const QString &text, const QString &status,
                           const QString &color) {
    QLabel *l = new QLabel(QString("<b style='color:#A09080'>%1</b><br><span "
                                   "style='color:%2'>%3</span>")
                               .arg(text, color, status));
    l->setAlignment(Qt::AlignCenter);
    l->setStyleSheet("background-color: #1A1615; border: 1px solid #4A3825; "
                     "border-radius: 4px; font-size: 10px; padding: 4px;");
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

  QLabel *healthTitle = new QLabel("🏥 系统健康");
  healthTitle->setStyleSheet("color: #A09080; font-size: 12px; font-weight: "
                             "bold; border: none; margin-bottom: 5px;");
  sysL->addWidget(healthTitle);

  QGridLayout *metricGrid = new QGridLayout();
  metricGrid->setVerticalSpacing(2);

  auto addMetric = [&](int row, const QString &label, QLabel *&valLbl,
                       const QString &valStr, const QString &color) {
    QLabel *lbl = new QLabel("● " + label);
    lbl->setStyleSheet("color:#A09080; font-size:11px; border:none;");
    valLbl = new QLabel(valStr);
    valLbl->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:bold; border:none;")
            .arg(color));
    valLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    metricGrid->addWidget(lbl, row, 0);
    metricGrid->addWidget(valLbl, row, 1);
  };

  addMetric(0, "AI 推理", aiLatencyLabel, "42ms", "#10B981");
  QLabel *camLbl;
  addMetric(1, "摄像头", camLbl, "在线", "#10B981");
  addMetric(2, "RPMsg", rpmsgStatusLabel, "连接", "#10B981");
  QLabel *hbLbl;
  addMetric(3, "从核心跳", hbLbl, "正常", "#10B981");
  QLabel *sysLoadLbl;
  addMetric(4, "系统", sysLoadLbl, "42%", "#F59E0B");
  sysL->addLayout(metricGrid);

  sysL->addSpacing(10);

  QLabel *commTitle = new QLabel("🔒 通信安全");
  commTitle->setStyleSheet("color: #A09080; font-size: 12px; font-weight: "
                           "bold; border: none; margin-bottom: 5px;");
  sysL->addWidget(commTitle);

  QHBoxLayout *commLayout = new QHBoxLayout();
  QLabel *crcLbl = new QLabel(
      "✓ CRC<br><span style='color:#A09080;font-weight:normal;'>0 次</span>");
  crcLbl->setAlignment(Qt::AlignCenter);
  crcLbl->setStyleSheet(
      "color:#10B981; font-size:11px; font-weight:bold; border:none;");

  QLabel *heartLbl =
      new QLabel("心跳<br><span style='color:#F59E0B;'>✓ 在线</span>");
  heartLbl->setAlignment(Qt::AlignCenter);
  heartLbl->setStyleSheet(
      "color:#A09080; font-size:11px; font-weight:bold; border:none;");

  commLayout->addWidget(crcLbl);
  commLayout->addWidget(heartLbl);
  sysL->addLayout(commLayout);

  rightBottomCol->addWidget(sysPanel);

  // 5. 控制按钮组 (放在右下角)
  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->setSpacing(5);
  btnLiveStream = new QPushButton("● 实时监控");
  btnImportVideo = new QPushButton("📂 导入");
  btnExit = new QPushButton("⏻ 退出");

  QString btnBase =
      "QPushButton { color: white; font-weight: bold; border-radius: 6px; "
      "padding: 10px 5px; border: none; font-size: 13px; }";
  btnLiveStream->setStyleSheet(
      btnBase + "QPushButton { background-color: #10B981; } QPushButton:hover "
                "{ background-color: #059669; }");
  btnImportVideo->setStyleSheet(
      btnBase + "QPushButton { background-color: #F59E0B; } QPushButton:hover "
                "{ background-color: #D97706; }");
  btnExit->setStyleSheet(btnBase +
                         "QPushButton { background-color: #EF4444; } "
                         "QPushButton:hover { background-color: #DC2626; }");

  btnLayout->addWidget(btnLiveStream);
  btnLayout->addWidget(btnImportVideo);
  btnLayout->addWidget(btnExit);

  rightBottomCol->addLayout(btnLayout);

  bottomRow->addWidget(statPanel, 5);
  bottomRow->addLayout(rightBottomCol, 4);

  // 组装 Right Layout
  rightLayout->addWidget(sensorPanel);
  rightLayout->addLayout(scoreLightLayout);
  rightLayout->addWidget(logPanel, 1);
  rightLayout->addLayout(bottomRow);

  // 组装 Body Layout
  bodyLayout->addWidget(videoFrame, 7);
  bodyLayout->addLayout(rightLayout, 3);

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

  barHelmet->setValue(85);
  lblHelmetCnt->setText("18");
  barVest->setValue(60);
  lblVestCnt->setText("12");
  barGoggle->setValue(35);
  lblGoggleCnt->setText("7");
  barSmoke->setValue(10);
  lblSmokeCnt->setText("1");

  // 假日志注入
  for (int i = 0; i < 5; i++) {
    addLogEntry("未戴安全帽", QDateTime::currentDateTime().toString("HH:mm:ss"),
                "");
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
  connect(
      SignalBridge::getInstance(), &SignalBridge::sendPhysicalAlarmStatus, this,
      [this](bool triggered) {
        if (triggered) {
          headerRpmsgLabel->setText("● RPMsg 警报");
          headerRpmsgLabel->setStyleSheet(
              "background-color: #EF4444; color: white; border: 1px solid "
              "#7F1D1D; border-radius: 14px; padding: 6px 14px; font-size: "
              "12px; font-weight: bold;");
          videoLabel->setStyleSheet(
              "color: white; background-color: rgba(239, 68, 68, 0.15); "
              "border: 2px solid #EF4444; font-size: 16px;");
          sensorFlame->setText("🔥 警报");
          sensorFlame->setStyleSheet("color: #EF4444; font-weight: bold; "
                                     "font-size: 14px; border: none;");
          scoreWidget->setScore(45, "危险 - 立即排查");
          lightRed->setStyleSheet("background-color: #EF4444; border-radius: "
                                  "12px; border: 2px solid #FCA5A5;");
          lightGreen->setStyleSheet("background-color: #064E3B; border-radius: "
                                    "12px; border: 2px solid #064E3B;");
        } else {
          headerRpmsgLabel->setText("● RPMsg 正常");
          headerRpmsgLabel->setStyleSheet(
              "background-color: #25201F; color: #10B981; border: 1px solid "
              "#4A3825; border-radius: 14px; padding: 6px 14px; font-size: "
              "12px; font-weight: bold;");
          videoLabel->setStyleSheet(
              "color: #666; background-color: transparent; border: none; "
              "font-size: 16px;");
          sensorFlame->setText("● 安全");
          sensorFlame->setStyleSheet("color: #10B981; font-weight: bold; "
                                     "font-size: 14px; border: none;");
          scoreWidget->setScore(84, "良好 - 压线40分");
          lightRed->setStyleSheet("background-color: #551515; border-radius: "
                                  "12px; border: 2px solid #551515;");
          lightGreen->setStyleSheet("background-color: #10B981; border-radius: "
                                    "12px; border: 2px solid #6EE7B7;");
        }
      },
      Qt::QueuedConnection);

  systemTimer = new QTimer(this);
  connect(systemTimer, &QTimer::timeout, this, &MainWindow::updateSystemStats);
  systemTimer->start(2000);

  resize(1366, 768); // 设置为接近 16:9 工业大屏比例
}

MainWindow::~MainWindow() {}

// 辅助 UI 创建函数
QFrame *MainWindow::createPanelFrame() {
  QFrame *frame = new QFrame();
  frame->setStyleSheet("QFrame { background-color: #25201F; border: 1px solid "
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
  t->setStyleSheet("color: #A09080; font-size: 11px; border: none;");
  t->setAlignment(Qt::AlignCenter);

  valueLabel = new QLabel("--");
  valueLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; font-size: 15px; border: none;")
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
          "QProgressBar { background-color: #1A1615; border: none; "
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

  cv::Mat displayFrame = frame.clone();

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
  logTable->insertRow(0);
  logTable->setItem(0, 0, new QTableWidgetItem(time));
  logTable->setItem(0, 1, new QTableWidgetItem(type));

  // 模拟来源
  QString source = "AI 视觉";
  if (type == "未戴安全帽" || type == "未穿背心")
    source = "AI 视觉";
  else if (type == "底座火焰触发")
    source = "从核 GPIO";
  else
    source = "多模态感知";

  logTable->setItem(0, 2, new QTableWidgetItem(source));

  QTableWidgetItem *statusItem = new QTableWidgetItem("🔍");
  statusItem->setTextAlignment(Qt::AlignCenter);
  statusItem->setForeground(QBrush(QColor("#3B82F6")));
  statusItem->setData(Qt::UserRole, imgPath);
  logTable->setItem(0, 3, statusItem);

  if (logTable->rowCount() > 50) {
    logTable->removeRow(50);
  }
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
      "background-color: #1A1615; color: white; border: 1px solid #4A3825;");

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
      "QPushButton { color: white; font-weight: bold; border-radius: 6px; "
      "padding: 10px; border: none; font-size: 14px; background-color: "
      "#059669; }");
  btnImportVideo->setText("📂 导入");
}

void MainWindow::onImportVideoClicked() {
  QString fileName = QFileDialog::getOpenFileName(this, "选择测试视频", "",
                                                  "Video Files (*.mp4 *.avi)");
  if (fileName.isEmpty())
    return;

  video_path = fileName.toStdString();
  current_source_mode = 1;
  source_changed = true;
  btnImportVideo->setText("🔄 分析中...");
  btnLiveStream->setText("● 实时监控");
  btnLiveStream->setStyleSheet(
      "QPushButton { color: white; font-weight: bold; border-radius: 6px; "
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
      headerTempLabel->setText(QString("🌡️ %1 °C").arg(temp, 0, 'f', 1));
      sensorTemp->setText(QString("%1 °C").arg(temp, 0, 'f', 1));

      QString badgeStyle = "background-color: #25201F; border: 1px solid "
                           "#4A3825; border-radius: 14px; padding: 6px 14px; "
                           "font-size: 12px; font-weight: bold;";
      if (temp >= 75.0) {
        headerTempLabel->setStyleSheet(badgeStyle + "color: #EF4444;");
        sensorTemp->setStyleSheet("color: #EF4444; font-weight: bold; "
                                  "font-size: 15px; border: none;");
      } else if (temp >= 65.0) {
        headerTempLabel->setStyleSheet(badgeStyle + "color: #F59E0B;");
        sensorTemp->setStyleSheet("color: #F59E0B; font-weight: bold; "
                                  "font-size: 15px; border: none;");
      } else {
        headerTempLabel->setStyleSheet(badgeStyle + "color: #10B981;");
        sensorTemp->setStyleSheet("color: #10B981; font-weight: bold; "
                                  "font-size: 15px; border: none;");
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

          headerCpuLabel->setText(QString("⚡ CPU %1%").arg(usage, 0, 'f', 1));

          QString badgeStyle = "background-color: #25201F; border: 1px solid "
                               "#4A3825; border-radius: 14px; padding: 6px "
                               "14px; font-size: 12px; font-weight: bold;";
          if (usage >= 90.0)
            headerCpuLabel->setStyleSheet(badgeStyle + "color: #EF4444;");
          else if (usage >= 70.0)
            headerCpuLabel->setStyleSheet(badgeStyle + "color: #F59E0B;");
          else
            headerCpuLabel->setStyleSheet(badgeStyle + "color: #10B981;");
        }
        prevTotalTicks = total;
        prevIdleTicks = totalIdle;
      }
    }
    statFile.close();
  }
}
