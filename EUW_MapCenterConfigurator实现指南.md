# EUW_MapCenterConfigurator 实现指南

## 1. 概述

`EUW_MapCenterConfigurator` 是一个 Editor Utility Widget，用于可视化配置地图中心坐标，简化 UE5 与 Python 桥接端的坐标系统对齐流程。

**核心功能**：
- 在 Cesium 地图中点击位置自动获取 GPS 坐标
- 手动输入 GPS 坐标
- 从 Cesium Georeference 读取坐标
- 自动配置 `DA_MapRuntimeConfig` 数据资产
- 导出 `multi_drone_config.yaml` 配置文件
- 在 Viewport 中可视化显示坐标系统

---

## 2. 创建步骤

### 2.1 创建 Editor Utility Widget

1. 在 Content Browser 中右键 → Editor Utilities → Editor Utility Widget
2. 命名为 `EUW_MapCenterConfigurator`
3. 保存到 `Content/DroneOps/EditorTools/`

### 2.2 设计 UI 布局

在 Widget Designer 中添加以下控件：

```
Canvas Panel
├─ Vertical Box (主容器)
    ├─ Text Block: "Map Center Configurator"
    ├─ Horizontal Line (分隔线)
    │
    ├─ Text Block: "【方式1：点击地图设置】"
    ├─ Button: "Enable Click Mode" (Name: Btn_EnableClickMode)
    ├─ Text Block: "提示：在Viewport中点击Cesium地图" (Name: Txt_ClickHint)
    │
    ├─ Horizontal Line
    │
    ├─ Text Block: "【方式2：手动输入GPS坐标】"
    ├─ Horizontal Box
    │   ├─ Text Block: "Latitude:"
    │   └─ Editable Text Box (Name: Input_Latitude, Text: "39.908720")
    ├─ Horizontal Box
    │   ├─ Text Block: "Longitude:"
    │   └─ Editable Text Box (Name: Input_Longitude, Text: "116.397480")
    ├─ Horizontal Box
    │   ├─ Text Block: "Altitude:"
    │   └─ Editable Text Box (Name: Input_Altitude, Text: "50.0")
    ├─ Button: "Apply GPS Coordinates" (Name: Btn_ApplyGPS)
    │
    ├─ Horizontal Line
    │
    ├─ Text Block: "【方式3：从Cesium读取】"
    ├─ Button: "Get From Cesium Georeference" (Name: Btn_GetFromCesium)
    │
    ├─ Horizontal Line
    │
    ├─ Text Block: "【当前配置状态】"
    ├─ Text Block (Name: Txt_CurrentConfig, Text: "未配置")
    │
    ├─ Horizontal Line
    │
    ├─ Text Block: "【导出配置】"
    ├─ Button: "Export to multi_drone_config.yaml" (Name: Btn_ExportYAML)
    ├─ Button: "Validate with Python Bridge" (Name: Btn_Validate)
    │
    ├─ Horizontal Line
    │
    ├─ Text Block: "【可视化】"
    ├─ Check Box: "Show Coordinate System in Viewport" (Name: Chk_ShowCoordSystem)
    ├─ Check Box: "Show Map Center Marker" (Name: Chk_ShowMapCenter)
    └─ Check Box: "Show North Direction Arrow" (Name: Chk_ShowNorthArrow)
```

---

## 3. Blueprint 逻辑实现

### 3.1 变量定义

在 Graph 中添加以下变量：

```
- CurrentLatitude (Double): 当前纬度
- CurrentLongitude (Double): 当前经度
- CurrentAltitude (Double): 当前海拔
- bClickModeEnabled (Boolean): 是否启用点击模式
- MapConfigAsset (DA_MapRuntimeConfig): 地图配置资产引用
- CesiumGeoreference (ACesiumGeoreference): Cesium地理参考Actor引用
```

### 3.2 初始化逻辑 (Event Construct)

```blueprint
Event Construct
  ↓
Load or Create MapConfigAsset
  ├─ Try Load: /Game/DroneOps/Data/DA_MapConfig_Default
  └─ If Not Found: Create New Asset
  ↓
Find CesiumGeoreference in Level
  ├─ Get All Actors of Class: ACesiumGeoreference
  └─ Store First Result
  ↓
Update UI with Current Config
  └─ Call: UpdateCurrentConfigDisplay()
```

### 3.3 方式1：点击地图模式

#### 按钮点击事件 (Btn_EnableClickMode → OnClicked)

```blueprint
OnClicked (Btn_EnableClickMode)
  ↓
Toggle bClickModeEnabled
  ↓
If Enabled:
  ├─ Set Button Text: "Disable Click Mode"
  ├─ Set Txt_ClickHint Color: Green
  └─ Register Viewport Click Handler
Else:
  ├─ Set Button Text: "Enable Click Mode"
  ├─ Set Txt_ClickHint Color: Gray
  └─ Unregister Viewport Click Handler
```

#### Viewport 点击处理（需要 C++ 辅助）

**C++ 辅助类**：`UMapCenterClickHandler`

```cpp
// MapCenterClickHandler.h
UCLASS()
class UMapCenterClickHandler : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "Map Config")
    static bool GetClickedGeographicCoordinates(
        FVector WorldLocation,
        ACesiumGeoreference* Georeference,
        double& OutLatitude,
        double& OutLongitude,
        double& OutAltitude
    );
};

// MapCenterClickHandler.cpp
bool UMapCenterClickHandler::GetClickedGeographicCoordinates(
    FVector WorldLocation,
    ACesiumGeoreference* Georeference,
    double& OutLatitude,
    double& OutLongitude,
    double& OutAltitude
)
{
    if (!Georeference) return false;

    // 使用 Cesium API 转换坐标
    FVector GeographicCoords = Georeference->TransformUeToLongitudeLatitudeHeight(WorldLocation);

    OutLongitude = GeographicCoords.X;
    OutLatitude = GeographicCoords.Y;
    OutAltitude = GeographicCoords.Z;

    return true;
}
```

**Blueprint 调用**：

```blueprint
On Viewport Clicked (Custom Event)
  ├─ Input: Hit Location (FVector)
  ↓
Call: GetClickedGeographicCoordinates
  ├─ World Location: Hit Location
  ├─ Georeference: CesiumGeoreference
  └─ Output: Latitude, Longitude, Altitude
  ↓
Update Input Fields
  ├─ Set Input_Latitude Text: Latitude
  ├─ Set Input_Longitude Text: Longitude
  └─ Set Input_Altitude Text: Altitude
  ↓
Call: ApplyCoordinates()
```

### 3.4 方式2：手动输入GPS坐标

#### 按钮点击事件 (Btn_ApplyGPS → OnClicked)

```blueprint
OnClicked (Btn_ApplyGPS)
  ↓
Parse Input Fields
  ├─ CurrentLatitude = Parse Double (Input_Latitude)
  ├─ CurrentLongitude = Parse Double (Input_Longitude)
  └─ CurrentAltitude = Parse Double (Input_Altitude)
  ↓
Validate Coordinates
  ├─ Latitude: -90 to 90
  ├─ Longitude: -180 to 180
  └─ Altitude: -500 to 10000
  ↓
If Valid:
  └─ Call: ApplyCoordinates()
Else:
  └─ Show Error Message
```

### 3.5 方式3：从Cesium读取

#### 按钮点击事件 (Btn_GetFromCesium → OnClicked)

```blueprint
OnClicked (Btn_GetFromCesium)
  ↓
Check CesiumGeoreference Valid
  ↓
Get Origin Coordinates
  ├─ CurrentLatitude = Georeference.OriginLatitude
  ├─ CurrentLongitude = Georeference.OriginLongitude
  └─ CurrentAltitude = Georeference.OriginHeight
  ↓
Update Input Fields
  ↓
Call: ApplyCoordinates()
```

### 3.6 核心函数：ApplyCoordinates()

```blueprint
Function: ApplyCoordinates()
  ↓
1. Update MapConfigAsset
  ├─ MapConfigAsset.MapCenterLatitude = CurrentLatitude
  ├─ MapConfigAsset.MapCenterLongitude = CurrentLongitude
  └─ MapConfigAsset.MapCenterAltitude = CurrentAltitude
  ↓
2. Mark Asset Dirty
  └─ MapConfigAsset.MarkPackageDirty()
  ↓
3. Save Asset
  └─ UEditorAssetLibrary::SaveAsset(MapConfigAsset)
  ↓
4. Update Cesium Georeference
  ├─ CesiumGeoreference.OriginLatitude = CurrentLatitude
  ├─ CesiumGeoreference.OriginLongitude = CurrentLongitude
  └─ CesiumGeoreference.OriginHeight = CurrentAltitude
  ↓
5. Initialize DroneCoordinateService
  ├─ Get Subsystem: UDroneCoordinateService
  └─ Call: SetMapCenter(Lat, Lon, Alt)
  ↓
6. Update UI Display
  └─ Call: UpdateCurrentConfigDisplay()
  ↓
7. Show Success Message
  └─ Print String: "地图中心已配置"
```

### 3.7 导出YAML配置

#### 按钮点击事件 (Btn_ExportYAML → OnClicked)

```blueprint
OnClicked (Btn_ExportYAML)
  ↓
Build YAML Content (String)
  ↓
"""
# 地图中心配置（由 EUW_MapCenterConfigurator 自动生成）
map_center:
  latitude: {CurrentLatitude}
  longitude: {CurrentLongitude}
  altitude: {CurrentAltitude}

unified_control:
  ue_send_port: 8899

# 注意：此配置必须与 UE5 的 DA_MapRuntimeConfig 保持一致
"""
  ↓
Get Project Directory
  └─ FPaths::ProjectDir()
  ↓
Build File Path
  └─ ProjectDir + "Config/multi_drone_config.yaml"
  ↓
Save to File
  └─ FFileHelper::SaveStringToFile(Content, FilePath)
  ↓
Show Success Message
  └─ Print String: "配置已导出到: {FilePath}"
```

### 3.8 可视化显示

#### 复选框事件 (Chk_ShowCoordSystem → OnCheckStateChanged)

```blueprint
OnCheckStateChanged (Chk_ShowCoordSystem)
  ↓
If Checked:
  └─ Call: DrawCoordinateSystemDebug()
Else:
  └─ Call: ClearDebugDrawings()
```

**C++ 辅助函数**：

```cpp
UFUNCTION(BlueprintCallable, Category = "Map Config")
static void DrawCoordinateSystemDebug(UWorld* World, bool bShowMapCenter, bool bShowNorthArrow)
{
    if (!World) return;

    // 1. 绘制地图中心（红色球体）
    if (bShowMapCenter)
    {
        DrawDebugSphere(World, FVector::ZeroVector, 500.0f, 16, FColor::Red, true, -1.0f, 0, 10.0f);
        DrawDebugString(World, FVector(0, 0, 1000), TEXT("Map Center (0,0,0)"), nullptr, FColor::White, -1.0f, true);
    }

    // 2. 绘制北向箭头（蓝色）
    if (bShowNorthArrow)
    {
        FVector NorthDir(10000.0f, 0, 0);  // +X = 北
        DrawDebugDirectionalArrow(World, FVector::ZeroVector, NorthDir, 500.0f, FColor::Blue, true, -1.0f, 0, 10.0f);
        DrawDebugString(World, FVector(10000, 0, 1000), TEXT("North (+X)"), nullptr, FColor::Blue, -1.0f, true);
    }

    // 3. 绘制坐标轴
    DrawDebugCoordinateSystem(World, FVector::ZeroVector, FRotator::ZeroRotator, 5000.0f, true, -1.0f, 0, 10.0f);
}
```

---

## 4. C++ 辅助类完整实现

### 4.1 头文件：MapCenterEditorLibrary.h

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CesiumGeoreference.h"
#include "MapCenterEditorLibrary.generated.h"

UCLASS()
class UE5DRONECONTROL_API UMapCenterEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 从世界坐标获取地理坐标
    UFUNCTION(BlueprintCallable, Category = "Map Config")
    static bool GetGeographicCoordinatesFromWorld(
        FVector WorldLocation,
        ACesiumGeoreference* Georeference,
        double& OutLatitude,
        double& OutLongitude,
        double& OutAltitude
    );

    // 绘制坐标系统调试信息
    UFUNCTION(BlueprintCallable, Category = "Map Config")
    static void DrawCoordinateSystemDebug(
        UWorld* World,
        bool bShowMapCenter,
        bool bShowNorthArrow,
        bool bShowCoordinateAxes
    );

    // 清除调试绘制
    UFUNCTION(BlueprintCallable, Category = "Map Config")
    static void ClearDebugDrawings(UWorld* World);

    // 导出YAML配置
    UFUNCTION(BlueprintCallable, Category = "Map Config")
    static bool ExportMapCenterToYAML(
        double Latitude,
        double Longitude,
        double Altitude,
        const FString& FilePath
    );

    // 查找场景中的 Cesium Georeference
    UFUNCTION(BlueprintCallable, Category = "Map Config")
    static ACesiumGeoreference* FindCesiumGeoreference(UWorld* World);
};
```

### 4.2 实现文件：MapCenterEditorLibrary.cpp

```cpp
#include "MapCenterEditorLibrary.h"
#include "DrawDebugHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

bool UMapCenterEditorLibrary::GetGeographicCoordinatesFromWorld(
    FVector WorldLocation,
    ACesiumGeoreference* Georeference,
    double& OutLatitude,
    double& OutLongitude,
    double& OutAltitude)
{
    if (!Georeference)
    {
        UE_LOG(LogTemp, Error, TEXT("Cesium Georeference is null"));
        return false;
    }

    // 使用 Cesium API 转换坐标
    FVector GeographicCoords = Georeference->TransformUeToLongitudeLatitudeHeight(WorldLocation);

    OutLongitude = GeographicCoords.X;
    OutLatitude = GeographicCoords.Y;
    OutAltitude = GeographicCoords.Z;

    UE_LOG(LogTemp, Log, TEXT("World Location: %s → GPS: (%.6f, %.6f, %.1f)"),
        *WorldLocation.ToString(), OutLatitude, OutLongitude, OutAltitude);

    return true;
}

void UMapCenterEditorLibrary::DrawCoordinateSystemDebug(
    UWorld* World,
    bool bShowMapCenter,
    bool bShowNorthArrow,
    bool bShowCoordinateAxes)
{
    if (!World) return;

    // 1. 绘制地图中心（红色球体）
    if (bShowMapCenter)
    {
        DrawDebugSphere(World, FVector::ZeroVector, 500.0f, 16, FColor::Red, true, -1.0f, 0, 10.0f);
        DrawDebugString(World, FVector(0, 0, 1000), TEXT("Map Center (0,0,0)"),
            nullptr, FColor::White, -1.0f, true, 2.0f);
    }

    // 2. 绘制北向箭头（蓝色）
    if (bShowNorthArrow)
    {
        FVector NorthDir(10000.0f, 0, 0);  // +X = 北
        DrawDebugDirectionalArrow(World, FVector::ZeroVector, NorthDir,
            500.0f, FColor::Blue, true, -1.0f, 0, 10.0f);
        DrawDebugString(World, FVector(10000, 0, 1000), TEXT("North (+X)"),
            nullptr, FColor::Blue, -1.0f, true, 2.0f);
    }

    // 3. 绘制坐标轴
    if (bShowCoordinateAxes)
    {
        DrawDebugCoordinateSystem(World, FVector::ZeroVector,
            FRotator::ZeroRotator, 5000.0f, true, -1.0f, 0, 10.0f);

        // 添加轴标签
        DrawDebugString(World, FVector(5000, 0, 0), TEXT("X (North)"),
            nullptr, FColor::Red, -1.0f, true, 2.0f);
        DrawDebugString(World, FVector(0, 5000, 0), TEXT("Y (East)"),
            nullptr, FColor::Green, -1.0f, true, 2.0f);
        DrawDebugString(World, FVector(0, 0, 5000), TEXT("Z (Up)"),
            nullptr, FColor::Blue, -1.0f, true, 2.0f);
    }
}

void UMapCenterEditorLibrary::ClearDebugDrawings(UWorld* World)
{
    if (!World) return;
    FlushPersistentDebugLines(World);
}

bool UMapCenterEditorLibrary::ExportMapCenterToYAML(
    double Latitude,
    double Longitude,
    double Altitude,
    const FString& FilePath)
{
    FString YAMLContent = FString::Printf(TEXT(
        "# 地图中心配置（由 EUW_MapCenterConfigurator 自动生成）\n"
        "# 生成时间: %s\n"
        "\n"
        "map_center:\n"
        "  latitude: %.8f\n"
        "  longitude: %.8f\n"
        "  altitude: %.2f\n"
        "\n"
        "unified_control:\n"
        "  ue_send_port: 8899\n"
        "\n"
        "# 注意：此配置必须与 UE5 的 DA_MapRuntimeConfig 保持一致\n"
        "# UE5 世界原点 (0,0,0) = 地图中心 GPS 坐标\n"
    ), *FDateTime::Now().ToString(), Latitude, Longitude, Altitude);

    bool bSuccess = FFileHelper::SaveStringToFile(YAMLContent, *FilePath);

    if (bSuccess)
    {
        UE_LOG(LogTemp, Log, TEXT("配置已导出到: %s"), *FilePath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("导出失败: %s"), *FilePath);
    }

    return bSuccess;
}

ACesiumGeoreference* UMapCenterEditorLibrary::FindCesiumGeoreference(UWorld* World)
{
    if (!World) return nullptr;

    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        return *It;
    }

    UE_LOG(LogTemp, Warning, TEXT("未找到 Cesium Georeference Actor"));
    return nullptr;
}
```

---

## 5. 使用流程

### 5.1 首次配置

1. 打开 UE5 编辑器
2. 打开 `LVL_DroneOps_Cesium` 地图
3. 在 Content Browser 中双击 `EUW_MapCenterConfigurator`
4. 工具窗口打开

**方式A：点击地图**
1. 点击 "Enable Click Mode" 按钮
2. 在 Viewport 中点击 Cesium 地图上的目标位置
3. GPS 坐标自动填充到输入框
4. 自动应用配置

**方式B：手动输入**
1. 在输入框中填写 GPS 坐标
2. 点击 "Apply GPS Coordinates"

**方式C：从Cesium读取**
1. 点击 "Get From Cesium Georeference"
2. 自动读取当前 Cesium 设置

### 5.2 导出配置

1. 点击 "Export to multi_drone_config.yaml"
2. 配置文件生成到 `Config/multi_drone_config.yaml`
3. 将此文件复制到 Python 脚本目录

### 5.3 可视化验证

1. 勾选 "Show Coordinate System in Viewport"
2. 在 Viewport 中查看：
   - 红色球体 = 地图中心 (0,0,0)
   - 蓝色箭头 = 北向 (+X)
   - 坐标轴 = X(北), Y(东), Z(上)

---

## 6. 注意事项

1. **Cesium 插件依赖**：确保项目已启用 `CesiumForUnreal` 插件
2. **编辑器模式**：此工具仅在编辑器中可用，不会打包到游戏中
3. **配置一致性**：导出的 YAML 配置必须与 Python 桥接端使用的配置一致
4. **坐标精度**：GPS 坐标保留 6-8 位小数（约 0.1-1 米精度）
5. **调试绘制**：调试线条会持续显示，关闭复选框可清除

---

## 7. 故障排除

### 问题1：点击地图无反应

**原因**：Viewport 点击事件未正确注册

**解决**：
- 检查 `bClickModeEnabled` 是否为 true
- 确认 Cesium Georeference 存在于场景中
- 查看 Output Log 是否有错误信息

### 问题2：导出的 YAML 文件为空

**原因**：文件路径不正确或权限不足

**解决**：
- 检查 `Config/` 目录是否存在
- 手动创建目录
- 检查文件写入权限

### 问题3：坐标转换不准确

**原因**：Cesium Georeference 配置错误

**解决**：
- 检查 Cesium Georeference 的 Origin 设置
- 确认地图瓦片正确加载
- 验证 GPS 坐标范围是否合理

---

## 8. 扩展建议

### 8.1 预设地图中心

添加常用地图中心的快速选择：

```
Dropdown: "预设地图中心"
  ├─ 北京天安门 (39.908720, 116.397480)
  ├─ 测试场地A (40.123456, 116.654321)
  └─ 自定义...
```

### 8.2 配置验证

添加与 Python 桥接端的配置对比：

```
Button: "Validate with Python Bridge"
  ↓
Send UDP Request to Python Bridge
  ↓
Compare Configurations
  ↓
Show Diff Report
```

### 8.3 历史记录

保存最近使用的地图中心配置：

```
List: "最近使用"
  ├─ 2026-03-28 14:30 - 北京天安门
  ├─ 2026-03-27 10:15 - 测试场地A
  └─ ...
```

---

## 9. 总结

`EUW_MapCenterConfigurator` 是 FR-01 的关键工具，它：

✅ 简化了地图中心配置流程（从手动编辑到可视化点击）
✅ 确保了 UE5 和 Python 配置的一致性（自动导出 YAML）
✅ 提供了直观的可视化验证（调试绘制）
✅ 降低了配置错误的风险（自动验证和提示）

建议在项目初始化时首先使用此工具配置地图中心，然后再进行其他开发工作。
