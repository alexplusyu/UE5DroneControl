#!/usr/bin/env python3
"""
启动UE5到PX4桥接节点
支持从配置文件读取参数
"""

import os
import sys
import yaml
import argparse
import subprocess
import time
from pathlib import Path

def load_config(config_file: str) -> dict:
    """加载配置文件"""
    config_path = Path(config_file)
    if not config_path.exists():
        print(f"错误: 配置文件不存在: {config_file}")
        sys.exit(1)

    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config = yaml.safe_load(f)
        print(f"[OK] 配置文件加载成功: {config_file}")
        return config or {}
    except Exception as e:
        print(f"错误: 无法解析配置文件: {e}")
        sys.exit(1)

def build_command(config: dict, args) -> list:
    """构建运行命令"""
    cmd = [sys.executable, "ue_to_px4_bridge.py"]

    # 从配置文件添加参数
    drone_config = config.get('drone', {})
    udp_config = config.get('udp', {})
    logging_config = config.get('logging', {})

    # 无人机ID
    if 'id' in drone_config:
        cmd.extend(['--drone-id', str(drone_config['id'])])

    # 话题前缀
    if 'topic_prefix' in drone_config:
        cmd.extend(['--topic-prefix', drone_config['topic_prefix']])

    # UDP端口
    if 'port' in udp_config:
        cmd.extend(['--udp-port', str(udp_config['port'])])

    # 日志级别
    if 'level' in logging_config:
        cmd.extend(['--log-level', logging_config['level']])

    # 命令行参数覆盖配置文件
    if args.drone_id is not None:
        # 替换无人机ID
        for i, item in enumerate(cmd):
            if item == '--drone-id' and i + 1 < len(cmd):
                cmd[i + 1] = str(args.drone_id)
                break
        else:
            cmd.extend(['--drone-id', str(args.drone_id)])

    if args.topic_prefix is not None:
        # 替换话题前缀
        for i, item in enumerate(cmd):
            if item == '--topic-prefix' and i + 1 < len(cmd):
                cmd[i + 1] = args.topic_prefix
                break
        else:
            cmd.extend(['--topic-prefix', args.topic_prefix])

    if args.udp_port is not None:
        # 替换UDP端口
        for i, item in enumerate(cmd):
            if item == '--udp-port' and i + 1 < len(cmd):
                cmd[i + 1] = str(args.udp_port)
                break
        else:
            cmd.extend(['--udp-port', str(args.udp_port)])

    if args.log_level is not None:
        # 替换日志级别
        for i, item in enumerate(cmd):
            if item == '--log-level' and i + 1 < len(cmd):
                cmd[i + 1] = args.log_level
                break
        else:
            cmd.extend(['--log-level', args.log_level])

    return cmd

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="启动UE5到PX4桥接节点")
    parser.add_argument('--config', '-c', default='ue_to_px4_config.yaml',
                       help='配置文件路径 (默认: ue_to_px4_config.yaml)')
    parser.add_argument('--drone-id', type=int,
                       help='覆盖配置文件的无人机ID')
    parser.add_argument('--topic-prefix', type=str,
                       help='覆盖配置文件的话题前缀')
    parser.add_argument('--udp-port', type=int,
                       help='覆盖配置文件的UDP端口')
    parser.add_argument('--log-level', type=str,
                       choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
                       help='覆盖配置文件的日志级别')
    parser.add_argument('--dry-run', action='store_true',
                       help='只显示命令，不实际运行')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='详细输出')

    args = parser.parse_args()

    # 检查主桥接文件是否存在
    if not Path("ue_to_px4_bridge.py").exists():
        print("错误: ue_to_px4_bridge.py 文件不存在")
        print("请确保在项目根目录运行此脚本")
        sys.exit(1)

    # 加载配置
    config = load_config(args.config)

    # 构建命令
    cmd = build_command(config, args)

    print("=" * 60)
    print("UE5到PX4桥接节点启动配置")
    print("=" * 60)

    # 显示配置摘要
    print("配置摘要:")
    print(f"  配置文件: {args.config}")
    print(f"  运行命令: {' '.join(cmd)}")
    print()

    # 显示详细配置
    if args.verbose:
        print("详细配置:")
        import json
        print(json.dumps(config, indent=2, ensure_ascii=False))
        print()

    if args.dry_run:
        print("[DRY RUN] 只显示命令，不实际运行")
        print(f"命令: {' '.join(cmd)}")
        sys.exit(0)

    # 运行节点
    print("启动桥接节点...")
    print("按 Ctrl+C 停止")
    print("-" * 60)

    try:
        # 运行命令
        process = subprocess.Popen(cmd)

        # 等待进程结束
        while True:
            try:
                process.wait(timeout=1)
                break
            except subprocess.TimeoutExpired:
                # 检查是否收到中断信号
                pass

    except KeyboardInterrupt:
        print("\n收到中断信号，正在停止节点...")
        if process:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                print("强制终止节点...")
                process.kill()

        print("节点已停止")

    except Exception as e:
        print(f"运行错误: {e}")
        if process:
            process.terminate()
        sys.exit(1)

if __name__ == "__main__":
    main()