# UE5到PX4桥接节点 - 快速参考

## 🎯 目标
实现UE5仿真无人机 → PX4真实无人机的反向控制链路

## 📦 新增文件清单

### 核心文件
1. **`ue_to_px4_bridge.py`** - 主桥接节点 (500+行)
   - UDP接收UE5数据 (24字节二进制)
   - 坐标转换: UE5厘米 → NED米
   - ROS2话题发布: OffboardControlMode, TrajectorySetpoint, VehicleCommand
   - OFFBOARD控制逻辑 (仿照u1.hpp)

2. **`ue_to_px4_config.yaml`** - 配置文件
   - 无人机ID、话题前缀、UDP端口
   - 安全限制、控制参数、坐标转换设置

3. **`start_ue_to_px4_bridge.py`** - 启动脚本
   - 从配置文件加载参数
   - 支持命令行参数覆盖

4. **`test_ue_sender.py`** - UE5数据模拟器
   - 模拟UE5发送UDP数据
   - 支持交互、连续、单点测试模式

### 文档
5. **`UE5到PX4桥接节点使用指南.md`** - 详细使用说明
6. **`UE5_PX4_BRIDGE_README.md`** - 本快速参考文件

## 🚀 快速启动

### 1. 安装依赖
```bash
pip install -r requirements.txt
cd px4_msgs && pip install -e . && cd ..
```

### 2. 测试UDP链路
```bash
# 终端1: 启动桥接节点
python3 ue_to_px4_bridge.py

# 终端2: 发送测试数据
python3 test_ue_sender.py --mode interactive
```

### 3. 连接PX4无人机
```bash
# 使用配置文件启动
python3 start_ue_to_px4_bridge.py --config ue_to_px4_config.yaml --verbose
```

## 🔧 UE5端修改

### 必须修改
1. 修改`UE5DroneControlCharacter`的`RemotePort`从`8888`改为`8889`

### 数据结构验证
确保UE5发送的数据格式为24字节:
- `double timestamp` (8字节)
- `float x, y, z` (各4字节，厘米)
- `int32 mode` (4字节，0=悬停，1=移动)

## ⚙️ 关键配置参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `udp.port` | 8889 | 接收UE5数据的端口 |
| `drone.id` | 1 | 无人机ID |
| `drone.topic_prefix` | `/px4_1` | ROS2话题前缀 |
| `control.rate_hz` | 100 | 控制频率 |
| `safety.min_altitude` | 1.0 | 最小高度(米) |

## 🔍 调试命令

```bash
# 查看UDP数据
nc -ul 8889

# 查看ROS2话题
ros2 topic list | grep px4
ros2 topic echo /px4_1/fmu/in/trajectory_setpoint

# 测试坐标转换
python3 -c "
from ue_to_px4_bridge import CoordinateConverter
ned = CoordinateConverter.ue5_to_ned(100, 200, 300)
print(f'NED: {ned}')
"
```

## ⚠️ 安全第一

1. **首次测试**: 在>2米安全高度进行
2. **范围限制**: 限制移动范围5×5米
3. **紧急停止**: 准备好遥控器紧急开关
4. **超时保护**: UDP超时0.5秒自动悬停

## 📞 故障排除

### 常见问题
1. **无法导入PX4消息**: `cd px4_msgs && pip install -e .`
2. **UDP数据收不到**: 检查防火墙和端口配置
3. **PX4不进入OFFBOARD**: 检查VehicleCommand参数
4. **飞向错误方向**: 验证坐标转换公式

### 日志位置
- 终端输出 (实时)
- `ue_to_px4_bridge.log` (文件日志)

## 🔄 与现有系统集成

### 同时运行两个数据流
```bash
# 终端1: 真实无人机→UE5 (端口8888)
python3 drone_data_bridge.py

# 终端2: UE5→真实无人机 (端口8889)
python3 ue_to_px4_bridge.py
```

### 端口分配表
| 端口 | 用途 | 方向 |
|------|------|------|
| 8888 | 真实无人机数据 → UE5 | 接收 |
| 8889 | UE5控制 → 真实无人机 | 发送 |
| 9999 | Python模拟器 → UE5 | 发送 |

## 🎮 测试流程建议

1. **阶段1**: 不连接PX4，只测试UDP链路
2. **阶段2**: 连接PX4但不解锁，测试OFFBOARD切换
3. **阶段3**: 安全高度小幅度移动测试
4. **阶段4**: 完整功能集成测试

## 📚 详细文档
- 完整使用指南: `UE5到PX4桥接节点使用指南.md`
- 代码注释: 所有Python文件都有详细注释
- 示例配置: `ue_to_px4_config.yaml`

---

**状态**: ✅ 已完成所有代码编写
**下一步**: 1) 安装依赖 2) 测试UDP链路 3) 修改UE5端口