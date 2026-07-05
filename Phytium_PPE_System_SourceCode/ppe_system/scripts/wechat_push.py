import os
import sys
import glob
import json
import base64
import hashlib
import requests

# 【在此处替换为您真实的 Webhook 地址】
# 您提供的链接是机器人的资料页，通常真正的 Webhook 地址看起来像下面这样（包含 key=...）
# 如果下面的推送失败，请进入飞书/企业微信机器人设置，复制真正的 Webhook 地址替换这里。
WEBHOOK_URL = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=24637ff8b2636e6482981868467e4cf8d2"
VIOLATIONS_DIR = "/home/user/ppe/ppe_system/violations_data"

def push_to_wechat(advice_text):
    print("[WeChat Push] 正在寻找最新的违规抓拍图片...")
    
    # 1. 寻找最新的违规截图
    list_of_files = glob.glob(f'{VIOLATIONS_DIR}/*.jpg')
    if not list_of_files:
        print("[WeChat Push] 未找到任何违规图片。")
        return
        
    latest_file = max(list_of_files, key=os.path.getctime)
    print(f"[WeChat Push] 找到最新图片: {latest_file}")
    
    # 2. 读取图片、转 Base64 并计算 MD5
    with open(latest_file, "rb") as f:
        img_data = f.read()
    
    b64_data = base64.b64encode(img_data).decode('utf-8')
    md5_hash = hashlib.md5(img_data).hexdigest()
    
    # 3. 构造企业微信机器人的发送载荷 (图文格式)
    # 企业微信要求先发图片，再发文字，或者一起发。这里我们分开发送，确保都能看到。
    
    # 发送文字 (Markdown 格式)
    # 将 HTML 标签简单清理一下，让企业微信显示更美观
    clean_text = advice_text.replace('<h3>', '## ').replace('</h3>', '\n').replace('<p>', '').replace('</p>', '\n').replace('<br>', '\n')
    
    text_payload = {
        "msgtype": "markdown",
        "markdown": {
            "content": f"🚨 **边缘节点违规警报** 🚨\n> **抓拍节点**: Phytium Pi Edge-01\n> **AI 分析结果**:\n\n{clean_text}"
        }
    }
    
    img_payload = {
        "msgtype": "image",
        "image": {
            "base64": b64_data,
            "md5": md5_hash
        }
    }
    
    # 4. 执行 HTTP 请求
    try:
        # 先推图片
        resp_img = requests.post(WEBHOOK_URL, json=img_payload, timeout=5)
        print(f"[WeChat Push] 图片推送结果: {resp_img.text}")
        
        # 再推文字
        resp_txt = requests.post(WEBHOOK_URL, json=text_payload, timeout=5)
        print(f"[WeChat Push] 文本推送结果: {resp_txt.text}")
    except Exception as e:
        print(f"[WeChat Push] 请求发送失败: {str(e)}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        # sys.argv[1] 是 C++ 传过来的 DeepSeek 文字建议
        advice = sys.argv[1]
        push_to_wechat(advice)
    else:
        print("[WeChat Push] 未接收到文字参数。")
