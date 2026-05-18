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

private:
  SignalBridge() = default;
};

// ==========================================
// 自定义圆形评分组件
// ==========================================
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

// ==========================================
// 主窗口类
// ==========================================
class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

public slots:
  void updateFrame(const cv::Mat &frame);
  void addLogEntry(QString type, QString time, QString imgPath);

private slots:
  void onExitClicked();
  void onLiveStreamClicked();
  void onImportVideoClicked();
  void showImageDialog(int row, int column);
  void updateSystemStats();

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

  // 3. 报警日志
  QTableWidget *logTable;

  // 4. 底部统计与状态
  QProgressBar *barHelmet;
  QProgressBar *barVest;
  QProgressBar *barGoggle;
  QProgressBar *barSmoke;

  QLabel *lblHelmetCnt;
  QLabel *lblVestCnt;
  QLabel *lblGoggleCnt;
  QLabel *lblSmokeCnt;

  QLabel *aiLatencyLabel;
  QLabel *rpmsgStatusLabel;
  QLabel *crcStatusLabel;
  QLabel *heartbeatStatusLabel;

  // 控制按钮
  QPushButton *btnLiveStream;
  QPushButton *btnImportVideo;
  QPushButton *btnExit;

  QTimer *systemTimer;
  unsigned long long prevTotalTicks;
  unsigned long long prevIdleTicks;
};