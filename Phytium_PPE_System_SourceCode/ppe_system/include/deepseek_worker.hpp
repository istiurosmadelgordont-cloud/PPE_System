/**
 * @file      deepseek_worker.hpp
 * @brief     DeepSeek AI 安全顾问异步请求与降级机制
 * @details   支持 10s 超时、断网检测、无 Key 时的在线仿真 Mock 模式。
 * @author    [双生序章] 团队
 * @version   3.1.0
 * @date      2026-06-18
 */

#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class DeepSeekWorker : public QObject {
  Q_OBJECT

public:
  explicit DeepSeekWorker(QObject *parent = nullptr);
  ~DeepSeekWorker() = default;

  /**
   * @brief 请求针对某违规类型的 AI 安全整改建议 (异步)
   */
  void requestAdvice(const QString &violationType);

  /**
   * @brief 判断当前主机是否有可用的网络连接 (非环回、IPv4 处于 Up 状态)
   */
  static bool isNetworkAvailable();

  /**
   * @brief 获取本地降级预设安全建议
   */
  static QString getFallbackAdvice(const QString &violationType);

signals:
  /**
   * @brief 开始分析时发送 (UI 显示加载状态)
   */
  void analysisStarted();

  /**
   * @brief 分析结束时发送 (返回富文本 HTML 整改意见)
   */
  void analysisFinished(const QString &advice);

private slots:
  void onReplyFinished();
  void onTimeout();

private:
  QNetworkAccessManager *m_networkManager;
  QNetworkReply *m_currentReply;
  QTimer *m_timeoutTimer;
  QString m_violationType;
  bool m_isTimeout;

  // 请求 API 配置
  QString m_apiUrl;
  QString m_apiKey;
  QString m_modelName;
};
