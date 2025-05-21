#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
import socket
import uuid
import platform
import time
import argparse
import sys

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

def simulate_discoverable_device(port=56789, broadcast_interval=3):
    """
    模拟一个可被LanSend发现的设备
    
    参数:
    - port: 模拟设备监听的端口
    - broadcast_interval: 广播间隔（秒）
    """
    # 创建UDP套接字用于广播
    broadcast_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    broadcast_socket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    
    # 设置广播地址
    broadcast_address = ('255.255.255.255', 56789)  # LanSend默认使用的端口
    
    # 生成设备ID
    device_id = f"{int(time.time() * 1000)}_{uuid.uuid4().hex[:8]}"
    
    # 创建设备信息
    device_info = {
        "device_id": device_id,
        "alias": "模拟测试设备",
        "hostname": socket.gethostname(),
        "operating_system": platform.system(),
        "ip_address": get_local_ip(),
        "port": port,
        "device_type": "desktop",
        "device_model": "PC"
    }
    
    print(f"开始模拟设备广播，设备信息:")
    print(json.dumps(device_info, indent=2, ensure_ascii=False))
    print(f"广播间隔: {broadcast_interval}秒")
    print("按Ctrl+C停止广播...")
    
    try:
        while True:
            # 将设备信息转换为JSON字符串
            data = json.dumps(device_info).encode('utf-8')
            
            # 广播设备信息
            broadcast_socket.sendto(data, broadcast_address)
            print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] 已广播设备信息")
            
            # 等待指定的时间间隔
            time.sleep(broadcast_interval)
    except KeyboardInterrupt:
        print("\n停止广播，退出程序")
    finally:
        broadcast_socket.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="模拟一个可被LanSend发现的设备")
    parser.add_argument("--port", type=int, default=56789, help="模拟设备监听的端口")
    parser.add_argument("--interval", type=int, default=3, help="广播间隔（秒）")
    
    args = parser.parse_args()
    
    simulate_discoverable_device(args.port, args.interval)
