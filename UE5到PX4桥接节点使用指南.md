# UE5到PX4桥接节点使用指南

## 📋 概述

`ue_to_px4_bridge.py` 是一个ROS2节点，用于将UE5仿真无人机的控制指令转发到PX4真实无人机。它实现了UE5→真实无人机的反向控制链路。

## 🏗️ 系统架构

```
UE5仿真无人机 (UE5DroneControlCharacter)
    ↓ UDP二进制数据 (端口8889, 24字节)
ue_to_px4_bridge.py (桥接节点)
    ↓ ROS2话题 /px4_i/fmu/in/...
PX4真实无人机 (Offboard模式)
```

## 📁 文件说明

### 核心文件
1. **`ue_to_px4_bridge.py`** - 主桥接节点
2. **`ue_to_px4_config.yaml`** - 配置文件
3. **`start_ue_to_px4_bridge.py`** - 启动脚本
4. **`test_ue_sender.py`** - UE5数据发送模拟器

### 依赖文件
- `px4_msgs/` - PX4 ROS2消息定义
- `requirements.txt` - Python依赖

## 🔧 安装与配置

### 1. 安装依赖
```bash
# 确保在项目根目录
cd /home/chen-jinyao/UE5DroneControl

# 安装Python依赖
pip install -r requirements.txt

# 安装px4_msgs
cd px4_msgs
pip install -e .
cd ..
```

### 2. 配置PX4消息
确保`px4_msgs`包含以下消息类型：
- `OffboardControlMode`
- `TrajectorySetpoint`
- `VehicleCommand`
- `VehicleOdometry`

如果缺少某些消息，可能需要从PX4 Firmware获取完整定义。

## 🚀 快速开始

### 方法1: 直接运行桥接节点
```bash
# 基本运行
python3 ue_to_px4_bridge.py

# 自定义参数
python3 ue_to_px4_bridge.py --drone-id 1 --topic-prefix /px4_1 --udp-port 8889 --log-level INFO
```

### 方法2: 使用启动脚本
```bash
# 使用默认配置
python3 start_ue_to_px4_bridge.py

# 自定义配置
python3 start_ue_to_px4_bridge.py --config ue_to_px4_config.yaml --drone-id 1 --verbose
```

### 方法3: 使用测试模拟器
```bash
# 交互模式
python3 test_ue_sender.py --mode interactive

# 连续发送测试
python3 test_ue_sender.py --mode continuous --trajectory circle --duration 30

# 单点测试
python3 test_ue_sender.py --mode single
```

## 📝 代码详细说明

### 1. `ue_to_px4_bridge.py` 结构

#### 主要类
- **`UEDataParser`** - UDP数据解析器
- **`CoordinateConverter`** - 坐标转换器
- **`UEToPX4Bridge`** - 主桥接节点

#### 关键函数

##### UDP数据解析
```python
def parse_udp_data(data: bytes) -> Optional[Dict[str, Any]]:
    """
    解析UE5发送的24字节二进制数据
    格式: double timestamp, float x, float y, float z, int32 mode
    """
```

##### 坐标转换
```python
def ue5_to_ned(ue_x: float, ue_y: float, ue_z: float) -> Tuple[float, float, float]:
    """
    UE5厘米 → NED米转换
    UE5: X=前, Y=右, Z=上 (厘米)
    NED: X=北, Y=东, Z=下 (米)
    转换: NED_X = UE5_X * 0.01, NED_Y = UE5_Y * 0.01, NED_Z = -UE5_Z * 0.01
    """
```

##### OFFBOARD控制流程
```python
def enter_offboard_mode(self):
    """
    仿照u1.hpp的OFFBOARD进入流程:
    1. 先发送20次空的TrajectorySetpoint
    2. 发送VEHICLE_CMD_DO_SET_MODE命令 (param1=1.0, param2=6.0)
    3. 切换到OFFBOARD模式
    """
```

##### 主控制循环
```python
def control_loop(self):
    """
    100Hz控制循环:
    1. 检查UDP数据超时
    2. 进入OFFBOARD模式 (如果需要)
    3. 发布控制模式消息
    4. 发布位置设定点
    """
```

### 2. 配置参数详解

#### 无人机配置
```yaml
drone:
  id: 1                    # 无人机ID (对应PX4参数MAV_SYS_ID)
  topic_prefix: "/px4_1"  # ROS2话题前缀
```

#### 安全限制
```yaml
safety:
  max_velocity: 5.0       # 最大速度 5米/秒
  min_altitude: 1.0       # 最小高度 1米
  max_altitude: 10.0      # 最大高度 10米
```

#### 坐标转换
```yaml
coordinate:
  ue5_to_ned_scale: 0.01   # UE5厘米→NED米比例
  ue5_x_axis: "north"      # 假设UE5 X轴指向北
```

## 🔌 UE5端修改指南

### 修改发送端口
在UE5编辑器中修改`UE5DroneControlCharacter`的`RemotePort`:
1. 打开`UE5DroneControlCharacter`蓝图
2. 在Details面板中找到`Remote Port`参数
3. 从`8888`改为`8889` (与桥接节点配置一致)

### 数据结构匹配
确保UE5发送的数据结构与桥接节点期望的一致:
```cpp
// UE5中的结构 (24字节)
struct FDroneSocketData {
    double Timestamp;  // 8字节
    float X;          // 4字节 (厘米)
    float Y;          // 4字节 (厘米)
    float Z;          // 4字节 (厘米)
    int32 Mode;       // 4字节 (0=悬停, 1=移动)
};
```

## 🔍 调试与测试

### 1. 验证UDP链路
```bash
# 方法1: 使用netcat监听
nc -ul 8889

# 方法2: 使用测试模拟器发送
python3 test_ue_sender.py --mode interactive
```

### 2. 验证ROS2话题
```bash
# 查看发布的话题
ros2 topic list | grep px4

# 查看特定话题
ros2 topic echo /px4_1/fmu/in/offboard_control_mode
ros2 topic echo /px4_1/fmu/in/trajectory_setpoint
```

### 3. 验证坐标转换
```python
# 手动测试坐标转换
from ue_to_px4_bridge import CoordinateConverter

# UE5厘米 → NED米
ue_x, ue_y, ue_z = 100.0, 200.0, 300.0  # 厘米
ned_x, ned_y, ned_z = CoordinateConverter.ue5_to_ned(ue_x, ue_y, ue_z)
print(f"UE5: ({ue_x}, {ue_y}, {ue_z}) cm")
print(f"NED: ({ned_x:.2f}, {ned_y:.2f}, {ned_z:.2f}) m")
```

### 4. 分步测试流程
1. **步骤1**: 只运行桥接节点，不连接PX4
   - 验证UDP接收正常
   - 验证ROS2话题发布正常

2. **步骤2**: 连接PX4但不解锁
   - 验证OFFBOARD模式切换
   - 验证控制消息格式正确

3. **步骤3**: 安全高度测试
   - 在>2米高度进行首次移动测试
   - 测试单个方向小幅度移动

4. **步骤4**: 完整功能测试
   - 测试鼠标点击控制
   - 测试连续轨迹跟踪

## ⚠️ 安全注意事项

### 1. 首次飞行安全措施
- 在安全高度 (>2米) 进行首次测试
- 限制移动范围 (例如5×5米)
- 准备紧急停机开关

### 2. 异常处理机制
- **UDP超时**: 超过0.5秒无数据自动悬停
- **位置限制**: 强制高度范围1-10米
- **速度限制**: 最大速度5米/秒

### 3. 紧急情况处理
```bash
# 快速停止桥接节点
pkill -f ue_to_px4_bridge

# 发送紧急停止命令 (如果节点仍在运行)
# 通过测试模拟器发送悬停命令
python3 test_ue_sender.py --mode interactive
# 然后输入: 0 0 150 0  (当前位置悬停)
```

## 🔄 与现有系统集成

### 与`drone_data_bridge.py`协同工作
```
# 终端1: 真实无人机→UE5数据流
python3 drone_data_bridge.py --ue-port 8888

# 终端2: UE5→真实无人机控制流
python3 ue_to_px4_bridge.py --udp-port 8889
```

### 端口分配
- `8888`: 真实无人机数据 → UE5 (`drone_data_bridge.py`)
- `8889`: UE5控制 → 真实无人机 (`ue_to_px4_bridge.py`)
- `9999`: Python模拟器 → UE5 (`main2.py`发送)

## 🐛 常见问题解决

### Q1: 无法导入PX4消息
```
错误: 无法导入PX4消息: No module named 'px4_msgs.msg'
```
**解决**:
```bash
cd px4_msgs
pip install -e .
cd ..
```

### Q2: UDP数据接收不到
**检查步骤**:
1. 确认UE5发送端口为`8889`
2. 确认防火墙未阻止UDP
3. 使用`test_ue_sender.py`测试UDP链路
4. 检查桥接节点日志是否有错误

### Q3: PX4不进入OFFBOARD模式
**可能原因**:
1. 未发送足够次数的空消息 (需要20次)
2. VehicleCommand参数不正确
3. PX4参数配置问题

**调试**:
```bash
# 查看VehicleCommand消息
ros2 topic echo /px4_1/fmu/in/vehicle_command
```

### Q4: 坐标转换错误
**症状**: 无人机飞向错误方向

**解决**:
1. 检查UE5地图的X轴方向
2. 调整`ue5_x_axis`配置参数
3. 使用`test_ue_sender.py`发送已知坐标验证

## 📈 性能优化

### 1. 控制频率
- 默认: 100Hz (0.01秒间隔)
- 可调整: 50-200Hz，根据网络状况调整

### 2. 网络优化
- 使用本地网络 (减少延迟)
- 增大UDP缓冲区
- 启用QoS配置

### 3. 日志优化
- 生产环境使用`INFO`级别
- 调试时使用`DEBUG`级别
- 定期清理日志文件

## 🔮 扩展功能

### 1. 多无人机支持
修改配置文件支持多个无人机:
```yaml
drones:
  - id: 1
    topic_prefix: "/px4_1"
    udp_port: 8889
  - id: 2
    topic_prefix: "/px4_2"
    udp_port: 8890
```

### 2. 速度控制模式
当前使用位置控制，可扩展为速度控制:
```python
# 在配置中启用
control:
  mode: "velocity"
  use_position_ctrl: false
  use_velocity_ctrl: true
```

### 3. 轨迹插值
添加平滑轨迹生成:
```python
def generate_smooth_trajectory(current_pos, target_pos):
    """生成平滑轨迹"""
    # 实现B样条或多项式轨迹
```

## 📞 技术支持

### 日志文件位置
- 桥接节点日志: `ue_to_px4_bridge.log`
- 系统日志: 查看终端输出

### 调试信息收集
```bash
# 收集所有相关信息
python3 ue_to_px4_bridge.py --log-level DEBUG 2>&1 | tee debug.log
```

### 问题报告模板
```
1. 问题描述:
2. 复现步骤:
3. 期望行为:
4. 实际行为:
5. 相关日志:
6. 环境信息:
   - OS版本:
   - Python版本:
   - ROS2版本:
   - PX4版本:
```

---

**作者**: Claude Code
**最后更新**: 2026-03-09
**版本**: 1.0.0