#!/usr/bin/env python3
"""
UE5数据发送模拟器
用于测试ue_to_px4_bridge.py节点
"""

import socket
import struct
import time
import sys
import math
import argparse
from datetime import datetime

# UE5发送的数据结构 (24字节)
# struct FDroneSocketData {
#     double Timestamp;  // 8 bytes
#     float X;          // 4 bytes (厘米)
#     float Y;          // 4 bytes (厘米)
#     float Z;          // 4 bytes (厘米)
#     int32 Mode;       // 4 bytes (0=悬停, 1=移动)
# };

STRUCT_FORMAT = "dfffI"  # double, float, float, float, unsigned int
DATA_SIZE = struct.calcsize(STRUCT_FORMAT)  # 24字节

class UESender:
    """UE5数据发送模拟器"""

    def __init__(self, host: str = "127.0.0.1", port: int = 8889):
        self.host = host
        self.port = port
        self.sock = None
        self.sequence = 0

    def connect(self):
        """连接UDP套接字"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            print(f"[OK] UDP发送器初始化完成，目标: {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"[ERROR] 无法创建UDP套接字: {e}")
            return False

    def send_data(self, x: float, y: float, z: float, mode: int = 1):
        """发送数据到桥接节点

        Args:
            x: X坐标 (厘米)
            y: Y坐标 (厘米)
            z: Z坐标 (厘米)
            mode: 控制模式 (0=悬停, 1=移动)
        """
        if not self.sock:
            print("[ERROR] 套接字未连接")
            return False

        try:
            # 创建数据包
            timestamp = time.time()
            data = struct.pack(STRUCT_FORMAT, timestamp, x, y, z, mode)

            # 发送数据
            self.sock.sendto(data, (self.host, self.port))

            self.sequence += 1
            if self.sequence % 10 == 0:  # 每10个包打印一次
                print(f"[发送#{self.sequence}] "
                      f"位置: ({x:.1f}, {y:.1f}, {z:.1f})cm, "
                      f"模式: {mode}, 大小: {len(data)}字节")

            return True

        except Exception as e:
            print(f"[ERROR] 发送数据失败: {e}")
            return False

    def close(self):
        """关闭连接"""
        if self.sock:
            self.sock.close()
            self.sock = None
            print("[OK] UDP连接已关闭")

def test_single_point():
    """测试单点发送"""
    print("=" * 60)
    print("UE5数据发送模拟器 - 单点测试")
    print("=" * 60)

    sender = UESender()
    if not sender.connect():
        return

    try:
        # 测试点1: 原点悬停
        print("\n[测试1] 原点悬停 (模式0)")
        for i in range(5):
            sender.send_data(0, 0, 100, 0)  # 高度100厘米，悬停
            time.sleep(0.1)

        # 测试点2: 向前移动
        print("\n[测试2] 向前移动 (模式1)")
        for i in range(10):
            x = i * 50  # 每次移动50厘米
            sender.send_data(x, 0, 150, 1)
            time.sleep(0.1)

        # 测试点3: 正方形轨迹
        print("\n[测试3] 正方形轨迹")
        points = [
            (0, 0, 200),    # 起点
            (200, 0, 200),  # 向右
            (200, 200, 200), # 向前
            (0, 200, 200),  # 向左
            (0, 0, 200)     # 向后
        ]

        for point in points:
            print(f"  移动到: {point}")
            for _ in range(3):  # 每个点发送3次
                sender.send_data(point[0], point[1], point[2], 1)
                time.sleep(0.1)
            time.sleep(1)  # 在每个点停留1秒

        print("\n[完成] 所有测试点发送完成")

    except KeyboardInterrupt:
        print("\n[中断] 测试被用户中断")

    finally:
        sender.close()

def test_interactive():
    """交互式测试"""
    print("=" * 60)
    print("UE5数据发送模拟器 - 交互模式")
    print("=" * 60)
    print("命令:")
    print("  x y z [mode] - 发送位置 (x,y,z厘米, 模式0/1)")
    print("  hover        - 发送悬停命令 (当前位置)")
    print("  circle       - 发送圆形轨迹")
    print("  square       - 发送正方形轨迹")
    print("  quit         - 退出")
    print("=" * 60)

    sender = UESender()
    if not sender.connect():
        return

    current_pos = [0.0, 0.0, 100.0]  # 当前位置 (厘米)
    current_mode = 1  # 当前模式

    try:
        while True:
            try:
                cmd = input(">> ").strip()
                if not cmd:
                    continue

                if cmd.lower() == 'quit' or cmd.lower() == 'exit':
                    break

                elif cmd.lower() == 'hover':
                    # 悬停命令
                    print(f"悬停: 位置=({current_pos[0]:.1f}, {current_pos[1]:.1f}, {current_pos[2]:.1f})cm")
                    for _ in range(5):
                        sender.send_data(current_pos[0], current_pos[1], current_pos[2], 0)
                        time.sleep(0.1)

                elif cmd.lower() == 'circle':
                    # 圆形轨迹
                    print("发送圆形轨迹...")
                    radius = 100  # 半径100厘米
                    height = 200  # 高度200厘米
                    for angle in range(0, 360, 10):
                        rad = math.radians(angle)
                        x = radius * math.cos(rad)
                        y = radius * math.sin(rad)
                        sender.send_data(x, y, height, 1)
                        time.sleep(0.05)
                    print("圆形轨迹完成")

                elif cmd.lower() == 'square':
                    # 正方形轨迹
                    print("发送正方形轨迹...")
                    size = 200  # 边长200厘米
                    height = 200
                    points = [
                        (0, 0, height),
                        (size, 0, height),
                        (size, size, height),
                        (0, size, height),
                        (0, 0, height)
                    ]
                    for point in points:
                        print(f"  移动到: {point}")
                        for _ in range(3):
                            sender.send_data(point[0], point[1], point[2], 1)
                            time.sleep(0.1)
                        time.sleep(0.5)

                else:
                    # 解析坐标命令
                    parts = cmd.split()
                    if len(parts) >= 3:
                        try:
                            x = float(parts[0])
                            y = float(parts[1])
                            z = float(parts[2])
                            mode = int(parts[3]) if len(parts) > 3 else 1

                            # 更新当前位置
                            current_pos = [x, y, z]
                            current_mode = mode

                            print(f"发送: ({x:.1f}, {y:.1f}, {z:.1f})cm, 模式={mode}")
                            sender.send_data(x, y, z, mode)

                        except ValueError as e:
                            print(f"错误: 无效的数字格式 - {e}")
                    else:
                        print("错误: 命令格式应为 'x y z [mode]'")

            except KeyboardInterrupt:
                print("\n输入中断")
                break
            except Exception as e:
                print(f"错误: {e}")

    finally:
        sender.close()
        print("\n[退出] 交互模式结束")

def test_continuous():
    """连续发送测试"""
    parser = argparse.ArgumentParser(description="连续发送测试")
    parser.add_argument('--host', default='127.0.0.1', help='目标主机')
    parser.add_argument('--port', type=int, default=8889, help='目标端口')
    parser.add_argument('--rate', type=float, default=10.0, help='发送频率 (Hz)')
    parser.add_argument('--duration', type=float, default=30.0, help='测试时长 (秒)')
    parser.add_argument('--trajectory', choices=['static', 'line', 'circle'],
                       default='static', help='轨迹类型')

    args = parser.parse_args()

    print("=" * 60)
    print("UE5数据发送模拟器 - 连续测试")
    print("=" * 60)
    print(f"目标: {args.host}:{args.port}")
    print(f"频率: {args.rate}Hz")
    print(f"时长: {args.duration}秒")
    print(f"轨迹: {args.trajectory}")
    print("=" * 60)

    sender = UESender(args.host, args.port)
    if not sender.connect():
        return

    start_time = time.time()
    interval = 1.0 / args.rate
    count = 0

    try:
        while time.time() - start_time < args.duration:
            # 根据轨迹类型生成位置
            if args.trajectory == 'static':
                x, y, z = 0.0, 0.0, 100.0  # 静态点

            elif args.trajectory == 'line':
                # 直线运动
                t = (time.time() - start_time) / 10.0  # 10秒周期
                x = 200.0 * math.sin(t * 2 * math.pi)
                y = 0.0
                z = 150.0

            elif args.trajectory == 'circle':
                # 圆形运动
                t = (time.time() - start_time) / 10.0  # 10秒周期
                radius = 100.0
                x = radius * math.cos(t * 2 * math.pi)
                y = radius * math.sin(t * 2 * math.pi)
                z = 200.0

            # 发送数据
            sender.send_data(x, y, z, 1)
            count += 1

            # 等待
            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n[中断] 测试被用户中断")

    finally:
        sender.close()

        elapsed = time.time() - start_time
        print(f"\n[完成] 测试结束")
        print(f"  发送数量: {count} 个数据包")
        print(f"  实际时长: {elapsed:.1f} 秒")
        print(f"  平均频率: {count/elapsed:.1f} Hz")

if __name__ == "__main__":
    # 命令行接口
    parser = argparse.ArgumentParser(description="UE5数据发送模拟器")
    parser.add_argument('--mode', choices=['single', 'interactive', 'continuous'],
                       default='interactive', help='测试模式')
    parser.add_argument('--host', default='127.0.0.1', help='目标主机')
    parser.add_argument('--port', type=int, default=8889, help='目标端口')

    args = parser.parse_args()

    if args.mode == 'single':
        test_single_point()
    elif args.mode == 'interactive':
        test_interactive()
    elif args.mode == 'continuous':
        test_continuous()