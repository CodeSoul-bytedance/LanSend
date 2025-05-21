#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import requests
import argparse
import socket
import uuid
import platform
import ssl

# 禁用SSL证书验证（仅用于测试）
requests.packages.urllib3.disable_warnings()

def get_local_ip():
    """获取本机IP地址"""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # 不需要真正连接
        s.connect(('10.255.255.255', 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = '127.0.0.1'
    finally:
        s.close()
    return IP

def simulate_connection_request(target_ip, target_port, pin_code):
    """模拟发送连接请求"""
    url = f"https://{target_ip}:{target_port}/connect"
    
    # 创建模拟设备信息
    device_info = {
        "device_id": str(uuid.uuid4()),
        "alias": "测试设备",
        "hostname": socket.gethostname(),
        "ip_address": get_local_ip(),
        "port": 56789,  # 模拟端口
        "operating_system": platform.system(),
        "device_model": "PC",
        "device_type": "desktop"
    }
    
    # 创建请求数据
    data = {
        "pin_code": pin_code,
        "device_info": device_info
    }
    
    print(f"发送连接请求到 {url}")
    print(f"设备信息: {json.dumps(device_info, indent=2, ensure_ascii=False)}")
    print(f"认证码: {pin_code}")
    
    try:
        # 发送请求，忽略SSL证书验证（仅用于测试）
        response = requests.post(url, json=data, verify=False)
        
        # 打印响应
        print("\n响应状态码:", response.status_code)
        response_text = response.text # 获取一次响应文本
        if response.status_code == 200:
            if response_text == "Connection accepted":
                print("\n✅ 连接请求成功！")
            else:
                # 如果响应不是预期的纯文本 "Connection accepted"
                # 尝试按JSON解析，以获取可能的错误信息结构
                try:
                    resp_data = response.json()
                    # 假设拒绝时可能返回如 {"message": "错误原因"} 的JSON
                    print(f"\n❌ 连接请求被拒绝: {resp_data.get('message', 'JSON响应但无message字段或非预期格式')}")
                except requests.exceptions.JSONDecodeError:
                    # 如果无法解析为JSON，则将原始文本作为拒绝信息
                    rejection_message = response_text.strip() if response_text.strip() else '响应为空或非JSON格式'
                    print(f"\n❌ 连接请求被拒绝: {rejection_message}")
        else:
            print(f"\n❌ 连接请求失败: HTTP {response.status_code}")
            
    except requests.exceptions.RequestException as e:
        print(f"\n❌ 请求异常: {e}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="模拟LanSend连接请求")
    parser.add_argument("--ip", default="127.0.0.1", help="目标设备IP地址")
    parser.add_argument("--port", type=int, default=56789, help="目标设备端口")
    parser.add_argument("--pin", required=True, help="认证码")
    
    args = parser.parse_args()
    
    simulate_connection_request(args.ip, args.port, args.pin)
