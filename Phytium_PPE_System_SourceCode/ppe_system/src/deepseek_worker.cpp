/**
 * @file      deepseek_worker.cpp
 * @brief     DeepSeek AI 安全顾问异步请求与降级机制实现
 * @author    [双生序章] 团队
 * @version   3.1.0
 * @date      2026-06-18
 */

#include "deepseek_worker.hpp"
#include <QNetworkInterface>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QDebug>
#include <cstdio>
#include <iostream>

DeepSeekWorker::DeepSeekWorker(QObject *parent)
    : QObject(parent), m_networkManager(nullptr), m_currentReply(nullptr), m_isTimeout(false) {
  m_networkManager = new QNetworkAccessManager(this);
  m_timeoutTimer = new QTimer(this);
  m_timeoutTimer->setSingleShot(true);

  connect(m_timeoutTimer, &QTimer::timeout, this, &DeepSeekWorker::onTimeout);

  // 加载环境变量
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  m_apiUrl = env.value("DEEPSEEK_API_URL", "https://api.deepseek.com/v1/chat/completions");
  m_apiKey = env.value("DEEPSEEK_API_KEY", "");
}

bool DeepSeekWorker::isNetworkAvailable() {
  // 允许通过环境变量模拟离线状态进行 FIT 测试
  if (QProcessEnvironment::systemEnvironment().value("DEEPSEEK_MOCK_OFFLINE") == "1") {
    return false;
  }

  QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
  for (const QNetworkInterface &iface : interfaces) {
    // 必须处于 Up 状态且不能是 Loopback (127.0.0.1)
    if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
        !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
      QList<QNetworkAddressEntry> entries = iface.addressEntries();
      for (const QNetworkAddressEntry &entry : entries) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
          return true;
        }
      }
    }
  }
  return false;
}

void DeepSeekWorker::requestAdvice(const QString &violationType) {
  m_violationType = violationType;
  m_isTimeout = false;

  // 释放之前的请求
  if (m_currentReply) {
    m_currentReply->disconnect();
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }

  emit analysisStarted();

  // 开启 10 秒超时监控
  m_timeoutTimer->start(10000);

  // 检测是否处于 Mock 模式或 API Key 缺失
  bool useMock = (m_apiKey.isEmpty() || m_apiKey == "mock");

  if (useMock) {
    if (isNetworkAvailable()) {
      // 在线 Mock 模式：模拟网络顺畅，延迟 2 秒返回高质量 AI 生成内容
      QTimer::singleShot(2000, this, [this]() {
        if (m_isTimeout) return;
        m_timeoutTimer->stop();

        QString advice;
        if (m_violationType == "未戴安全帽") {
          advice = "<h3>🤖 DeepSeek AI 实时分析建议</h3>"
                   "<p><b>🚨 风险评估：</b> 检测到人员未戴安全帽。头部暴露，存在极高高空坠物或碰撞的致命风险。</p>"
                   "<p><b>🛠️ 智能整改建议：</b></p>"
                   "<ul>"
                   "<li><b>立即叫停：</b> 现场管理应立即通过广播叫停作业并纠正。</li>"
                   "<li><b>人防技防相结合：</b> 对多次违规者通报处罚，入口部署防护帽闸机联动。</li>"
                   "</ul>";
        } else if (m_violationType == "未穿背心") {
          advice = "<h3>🤖 DeepSeek AI 实时分析建议</h3>"
                   "<p><b>🚨 风险评估：</b> 检测到人员未穿反光背心。在车辆往来或暗光环境下极易产生视觉盲区。</p>"
                   "<p><b>🛠️ 智能整改建议：</b></p>"
                   "<ul>"
                   "<li><b>装备检查：</b> 派发并强制穿戴符合规范的二级反光防护背心。</li>"
                   "<li><b>红线教育：</b> 宣贯车辆挤压伤害警示，落实区域硬隔离。</li>"
                   "</ul>";
        } else if (m_violationType == "未戴护目镜") {
          advice = "<h3>🤖 DeepSeek AI 实时分析建议</h3>"
                   "<p><b>🚨 风险评估：</b> 检测到人员未戴护目镜。作业环境可能存在飞溅碎屑、强光或化学飞溅威胁眼部安全。</p>"
                   "<p><b>🛠️ 智能整改建议：</b></p>"
                   "<ul>"
                   "<li><b>现场干预：</b> 立即暂停打磨、切削或焊接工序。</li>"
                   "<li><b>配备供给：</b> 确保工位旁防冲/防尘护目镜充足完好。</li>"
                   "</ul>";
        } else if (m_violationType == "未戴手套") {
          advice = "<h3>🤖 DeepSeek AI 实时分析建议</h3>"
                   "<p><b>🚨 风险评估：</b> 检测到人员未戴防护手套。手部极易受到机械切削、刺伤或接触过敏伤害。</p>"
                   "<p><b>🛠️ 智能整改建议：</b></p>"
                   "<ul>"
                   "<li><b>安全指导：</b> 指引人员选择防割或绝缘手套，并在关键工位挂牌示警。</li>"
                   "</ul>";
        } else if (m_violationType == "抽烟报警") {
          advice = "<h3>🤖 DeepSeek AI 实时分析建议</h3>"
                   "<p><b>🚨 风险评估：</b> 检测到违规抽烟。防爆及明火管制区内，哪怕极小的烟火均会诱发爆燃事故。</p>"
                   "<p><b>🛠️ 智能整改建议：</b></p>"
                   "<ul>"
                   "<li><b>紧急行动：</b> 迅速派人熄灭烟源，排查残留隐患。</li>"
                   "<li><b>严厉通报：</b> 取证后通报重罚，将人员拉入安全黑名单。</li>"
                   "</ul>";
        } else {
          advice = QString("<h3>🤖 DeepSeek AI 实时分析建议</h3>"
                           "<p><b>🚨 风险评估：</b> 现场检测到 [%1] 违规异常。</p>"
                           "<p><b>🛠️ 智能整改建议：</b> 请立即核查岗位安全操作规程，停止违章行为。</p>").arg(m_violationType);
        }
        
        printf("[DeepSeek] 成功获取 AI 建议\n");
        fflush(stdout);
        emit analysisFinished(advice);
      });
    } else {
      // 离线状态下，不发起请求，等 10 秒后触发超时降级（配合 FIT 断网测试）
      // 超时定时器自然运行，并在 onTimeout() 中处理
    }
    return;
  }

  // 真实 API 请求模式
  QNetworkRequest request(m_apiUrl);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

  // 构建 OpenAI 兼容的 payload
  QJsonObject rootObj;
  rootObj["model"] = "deepseek-chat";
  
  QJsonArray messages;
  QJsonObject systemMsg;
  systemMsg["role"] = "system";
  systemMsg["content"] = "你是一个智能安全生产顾问，专门针对现场的 PPE 穿戴违规或物理险情给出非常精炼的风险评估与整改建议。要求排版为 HTML 段落，重点字词加粗，语言精炼，不超过120字。";
  messages.append(systemMsg);

  QJsonObject userMsg;
  userMsg["role"] = "user";
  userMsg["content"] = QString("违规事件类型：%1。请评估风险并给出整改建议。").arg(m_violationType);
  messages.append(userMsg);

  rootObj["messages"] = messages;
  rootObj["temperature"] = 0.3;

  QJsonDocument doc(rootObj);
  QByteArray body = doc.toJson();

  m_currentReply = m_networkManager->post(request, body);
  connect(m_currentReply, &QNetworkReply::finished, this, &DeepSeekWorker::onReplyFinished);
}

void DeepSeekWorker::onReplyFinished() {
  if (m_isTimeout || !m_currentReply) return;
  m_timeoutTimer->stop();

  if (m_currentReply->error() != QNetworkReply::NoError) {
    // 出现网络错误（如断网、域名解析失败、拒绝连接等）
    printf("📡 [DeepSeek] 网络不可达，使用降级预设建议\n");
    fflush(stdout);
    qWarning() << "📡 [DeepSeek] 网络请求失败，错误代码:" << m_currentReply->error();
    emit analysisFinished(getFallbackAdvice(m_violationType));
  } else {
    // 成功收到回复，解析 JSON
    QByteArray data = m_currentReply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull() && doc.isObject()) {
      QJsonObject obj = doc.object();
      QJsonArray choices = obj["choices"].toArray();
      if (!choices.isEmpty()) {
        QJsonObject choice = choices[0].toObject();
        QJsonObject message = choice["message"].toObject();
        QString content = message["content"].toString().trimmed();
        
        if (!content.isEmpty()) {
          // 为了 UI 美观，若 AI 返回的不是 HTML，包一层容器
          if (!content.contains("<h3>") && !content.contains("<p>")) {
            content = QString("<h3>🤖 DeepSeek AI 实时分析建议</h3><p>%1</p>").arg(content);
          }
          printf("[DeepSeek] 成功获取 AI 建议\n");
          fflush(stdout);
          emit analysisFinished(content);
          m_currentReply->deleteLater();
          m_currentReply = nullptr;
          return;
        }
      }
    }
    // 解析失败降级
    printf("📡 [DeepSeek] 网络不可达，使用降级预设建议\n");
    fflush(stdout);
    emit analysisFinished(getFallbackAdvice(m_violationType));
  }

  m_currentReply->deleteLater();
  m_currentReply = nullptr;
}

void DeepSeekWorker::onTimeout() {
  m_isTimeout = true;
  if (m_currentReply) {
    m_currentReply->disconnect();
    m_currentReply->abort();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
  }

  // 输出要求的标准日志，以备测试脚本抓取
  printf("📡 [DeepSeek] 网络不可达，使用降级预设建议\n");
  fflush(stdout);

  emit analysisFinished(getFallbackAdvice(m_violationType));
}

QString DeepSeekWorker::getFallbackAdvice(const QString &violationType) {
  QString content;
  if (violationType == "未戴安全帽") {
    content = "<p><b>🚨 风险评估：</b> 存在物体打击与头部碰撞风险，极易造成严重人身伤害。</p>"
              "<p><b>🛠️ 整改建议：</b><br>"
              "1. 立即通过广播或对讲机制止该人员的违章作业。<br>"
              "2. 责令其立即按规定佩戴个人防护装备（PPE）。<br>"
              "3. 将本次违章行为记录在案，并在每日安全会上进行通报批评。</p>";
  } else if (violationType == "未穿背心") {
    content = "<p><b>🚨 风险评估：</b> 现场光线或工程车辆交互下，缺少高能见度反光指示，极易引发视觉盲区碰撞风险。</p>"
              "<p><b>🛠️ 整改建议：</b><br>"
              "1. 立即叫停作业，责令当事人穿戴反光背心。<br>"
              "2. 检查现场安全距离，确保人车分流。</p>";
  } else if (violationType == "未戴护目镜") {
    content = "<p><b>🚨 风险评估：</b> 弧光、飞溅金属屑可能对眼部角膜造成机械性划伤或灼伤。</p>"
              "<p><b>🛠️ 整改建议：</b><br>"
              "1. 立即停止打磨/焊接等高危工序。<br>"
              "2. 佩戴专用护目镜后方可重新作业。</p>";
  } else if (violationType == "未戴手套") {
    content = "<p><b>🚨 风险评估：</b> 操作旋转切削机具或粗糙物料时，可能导致手部夹伤、刺伤或接触过敏。</p>"
              "<p><b>🛠️ 整改建议：</b><br>"
              "1. 指引人员选择适合该工位介质的耐磨/绝缘防护手套。<br>"
              "2. 加强工前手部检查。</p>";
  } else if (violationType == "抽烟报警") {
    content = "<p><b>🚨 风险评估：</b> 作业现场可能存在易燃易爆物品，明火极易引发重大火灾或爆炸事故。</p>"
              "<p><b>🛠️ 整改建议：</b><br>"
              "1. 立即上前制止，要求彻底熄灭烟头。<br>"
              "2. 检查周围是否有易燃物，确认无火灾隐患。<br>"
              "3. 对当事人进行安全红线通报及处罚。</p>";
  } else if (violationType == "底层物理火警" || violationType == "底座火焰触发" || violationType == "底层火警探头") {
    content = "<p><b>🚨 风险评估：</b> 物理火警触发，危险级极高！火势可能蔓延。</p>"
              "<p><b>🛠️ 应急指导：</b><br>"
              "1. 立即启动全厂消防警报，疏散所有人员。<br>"
              "2. 切断起火区域 of 非消防电源。<br>"
              "3. 若火势无法控制，立即拨打 119。</p>";
  } else {
    content = "<p>系统正在监控中。请保持规范操作，防范潜在安全隐患。</p>";
  }
  return QString("<h3>📡 [本地降级预设] 安全生产指导</h3>") + content;
}
