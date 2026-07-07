import os
import sys
import requests

# 您的飞书 Webhook 地址
WEBHOOK_URL = "https://open.feishu.cn/open-apis/bot/v2/hook/9d769430-8059-4dd3-b1b3-230bf93cc1db"

def push_to_feishu(advice_text):
    print("[Feishu Push] 正在推送警报信息到飞书...")
    
    # 清理 C++ 传过来的 HTML 标签，转换成飞书支持的 Markdown 语法
    clean_text = advice_text.replace('<h3>', '**').replace('</h3>', '**\n').replace('<p>', '').replace('</p>', '\n').replace('<br>', '\n')
    
    # 构造飞书卡片格式的 JSON (极其精美)
    payload = {
        "msg_type": "interactive",
        "card": {
            "config": {
                "wide_screen_mode": True
            },
            "header": {
                "title": {
                    "tag": "plain_text",
                    "content": "🚨 边缘安全监控警报"
                },
                "template": "red"
            },
            "elements": [
                {
                    "tag": "div",
                    "text": {
                        "tag": "lark_md",
                        "content": f"**监控设备**: Phytium Pi Edge-01\n**警报状态**: 违规行为已阻断\n\n💬 **AI 智能顾问建议**:\n{clean_text}"
                    }
                },
                {
                    "tag": "note",
                    "elements": [
                        {
                            "tag": "plain_text",
                            "content": "💡 提示：详细违规抓拍图片已自动保存至飞腾派本地 violations_data 目录。"
                        }
                    ]
                }
            ]
        }
    }
    
    try:
        headers = {"Content-Type": "application/json"}
        resp = requests.post(WEBHOOK_URL, json=payload, headers=headers, timeout=5)
        print(f"[Feishu Push] 推送结果: {resp.status_code} {resp.text}")
    except Exception as e:
        print(f"[Feishu Push] 推送失败: {str(e)}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        advice = sys.argv[1]
        push_to_feishu(advice)
    else:
        print("[Feishu Push] 未接收到文字参数。")
