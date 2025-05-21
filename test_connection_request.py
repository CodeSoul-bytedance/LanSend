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

def simulate_connection_request(target_ip, target_port, auth_code):
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
        "auth_code": auth_code,
        "device_info": device_info
    }
    
    print(f"发送连接请求到 {url}")
    print(f"设备信息: {json.dumps(device_info, indent=2, ensure_ascii=False)}")
    print(f"认证码: {auth_code}")
    
    try:
        # 发送请求，忽略SSL证书验证（仅用于测试）
        response = requests.post(url, json=data, verify=False)
        
        # 打印响应
        print("\n响应状态码:", response.status_code)
        print("响应内容:", response.text)
        
        # 解析响应
        if response.status_code == 200:
            resp_data = response.json()
            if resp_data.get("success"):
                print("\n✅ 连接请求成功！")
            else:
                print(f"\n❌ 连接请求被拒绝: {resp_data.get('message', '未知原因')}")
        else:
            print(f"\n❌ 连接请求失败: HTTP {response.status_code}")
            
    except requests.exceptions.RequestException as e:
        print(f"\n❌ 请求异常: {e}")
    except json.JSONDecodeError:
        print("\n❌ 响应不是有效的JSON格式")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="模拟LanSend连接请求")
    parser.add_argument("--ip", default="127.0.0.1", help="目标设备IP地址")
    parser.add_argument("--port", type=int, default=56789, help="目标设备端口")
    parser.add_argument("--auth", required=True, help="认证码")
    
    args = parser.parse_args()
    
    simulate_connection_request(args.ip, args.port, args.auth)
