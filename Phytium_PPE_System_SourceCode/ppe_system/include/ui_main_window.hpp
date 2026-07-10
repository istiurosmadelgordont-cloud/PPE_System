/**
 * @file      ui_main_window.hpp
 * @brief     PPE智能安全监控中枢系统
 * @details   飞腾派 E2000Q 异构多核平台定制化开发
 * @author    [双生序章] 团队
 * @version   3.1.0 (极致稳定版)
 * @date      2026-05-18
 * @copyright Copyright (c) 2026. All rights reserved.
 */

#pragma once
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <opencv2/opencv.hpp>
#include "deepseek_worker.hpp"

// ==========================================
// 跨线程通信桥梁：大核 AI -> 小核 UI
// ==========================================
class SignalBridge : public QObject {
  Q_OBJECT
public:
  static SignalBridge *getInstance() {
    static SignalBridge instance;
    return &instance;
  }
signals:
  void sendFrame(const cv::Mat &frame);
  void sendAlarmLog(QString type, QString time, QString imgPath);
  void sendPhysicalAlarmStatus(bool triggered);
  void sendAiMetrics(int latencyMs, int personCount);
  void sendGasAlarmStatus(bool alarmed);
  void sendEnvMetrics(double temp, double humid);
  void sendAiAlarmStatus(bool alarmed);
  void sendEnvError();
  void sendCrcError(int count);

private:
  SignalBridge() = default;
};

/**
 * @class CircularScoreWidget
 * @brief 自定义工业级圆形评分组件
 * @details 通过 QPainter 重写 paintEvent
 * 实现动态圆环绘制，用于展示系统当前的整体安全健康度。
 */
class CircularScoreWidget : public QWidget {
  Q_OBJECT
public:
  explicit CircularScoreWidget(QWidget *parent = nullptr);
  void setScore(int score, const QString &statusText);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  int m_score;
  QString m_statusText;
};

/**
 * @class MainWindow
 * @brief PPE 系统主控展示窗口
 * @details 基于 16:9 比例的暗黑工业风大屏设计，集成 6:4 智能排版。
 *          涵盖视频实时渲染、多模传感矩阵、AI 告警日志及 DeepSeek
 * 整改顾问面板。
 */
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

public slots:
  void updateFrame(const cv::Mat &frame);
  void addLogEntry(QString type, QString time, QString imgPath);
  void updateCrcError(int count);

private slots:
  void onExitClicked();
  void onLiveStreamClicked();
  void onImportVideoClicked();
  void showImageDialog(int row, int column);
  void updateSystemStats();
  void showDeepSeekSuggestion(const QString &text);
  void onDeepSeekAnalysisStarted();
  void onDeepSeekAnalysisFinished(const QString &advice);
  void triggerAggregatedDeepSeek();

  void updateThreeColorLights();

private:
  // 辅助函数：创建统一样式的面板卡片
  QFrame *createPanelFrame();
  QWidget *createSensorItem(const QString &title, QLabel *&valueLabel,
                            const QString &color);
  QProgressBar *createCustomProgressBar(const QString &color);

  // --- 顶部 Header ---
  QLabel *headerRpmsgLabel;
  QLabel *headerTempLabel;
  QLabel *headerCpuLabel;
  QLabel *headerRamLabel;

  // --- 左侧主视觉 ---
  QLabel *videoLabel;

  // --- 中间 DeepSeek ---
  QLabel *dsContent;

  // --- 右侧数据矩阵 ---
  // 1. 传感数据
  QLabel *sensorFlame;
  QLabel *sensorGas;
  QLabel *sensorTemp;
  QLabel *sensorHumid;
  QLabel *sensorPerson;
  QLabel *sensorNoise;

  // 2. 评分与灯塔
  CircularScoreWidget *scoreWidget;
  QLabel *lightRed;
  QLabel *lightYellow;
  QLabel *lightGreen;
  QLabel *lightStatus;

  // 3. 报警日志
  QTableWidget *logTable;

  // 4. 底部统计与状态
  QProgressBar *barHelmet;
  QProgressBar *barVest;
  QProgressBar *barGoggle;
  QProgressBar *barSmoke;
  QProgressBar *barGlove;

  QLabel *lblHelmetCnt;
  QLabel *lblVestCnt;
  QLabel *lblGoggleCnt;
  QLabel *lblSmokeCnt;
  QLabel *lblGloveCnt;

  QLabel *aiLatencyLabel;
  QLabel *camStatusLabel;
  QLabel *rpmsgStatusLabel;
  QLabel *crcStatusLabel;
  QLabel *heartbeatStatusLabel;
  QLabel *commHeartbeatLabel;
  QLabel *sysLoadLabel;

  // 控制按钮
  QPushButton *btnLiveStream;
  QPushButton *btnImportVideo;
  QPushButton *btnExit;

  QTimer *systemTimer;
  unsigned long long prevTotalTicks;
  unsigned long long prevIdleTicks;
  DeepSeekWorker *dsWorker;
  QTimer *m_dsAggregationTimer;
  QTimer *m_aiAlarmHoldTimer;
  QStringList m_pendingViolations;

  // 累加违规计数器
  int m_cntHelmet = 0;
  int m_cntVest = 0;
  int m_cntGoggle = 0;
  int m_cntSmoke = 0;
  int m_cntGlove = 0;

  // 警报触发状态监控（防重复触发）
  bool m_fireAlerted = false;
  bool m_gasAlerted = false;
  bool m_tempHumidAlerted = false;
  bool m_aiAlerted = false;
};