# STM32 OLED 汉字取模工具

给 **STM32 + SSD1306 OLED** 用的点阵取模工具。输入汉字，实时预览 16×16 点阵，
一键生成可以直接编进工程的 C 字库文件。

## 特点

| | 说明 |
|---|---|
| **格式写死在代码里** | 阴码 / 列行式 / 逆向，不需要你选任何取模参数，也就不可能选错 |
| **导出前逐行自检** | 每行必须正好 35 字节，不合格就拒绝导出并指出是第几行 |
| **按 UTF-8 编码查表** | 源码里直接写 `"温度:25"` 就能显示，不用算区位码 |
| **实时预览点阵** | 左边打字右边出图，字形对不对当场就能看出来 |
| **实时算 Flash 占用** | 64KB 的 STM32F103C8T6 心里有数 |
| **导入并合并** | 读回已有字库往里加字，不是覆盖重建 |
| **两种用法同格式** | 图形界面版和命令行版产出的文件完全一致 |
| **不用真机验证驱动** | 附带 PC 端 I2C 从机模拟器，改驱动不用反复烧录 |

## 快速开始（不想编译的话）

到 [Releases](https://github.com/zhangruodong/stm32-oled-font-tool/releases/latest) 下载，两种版本内容相同，任选一个：

| 版本 | 用法 | 特点 |
|---|---|---|
| `...-setup.exe` | 双击按向导安装 | 可自选安装路径，创建开始菜单/桌面快捷方式，可在控制面板卸载 |
| `...-portable.zip` | 解压后双击 `oledfonttool.exe` | 免安装（绿色版），不用了就删文件夹 |

**两种都不需要安装 Qt、Visual Studio、或任何运行库** —— 都在包里了。

用法：

1. 左边输入框写你要显示的汉字（多行也行，会自动去重）
2. 右边实时显示每个字的点阵，核对字形
3. 点「导出 OLED_CFont.h」
4. 把生成的文件放进 STM32 工程的 `Hardware\` 目录，Rebuild

> 「导入已有字库」可以把现成的 `OLED_CFont.h` 读回来**并合并**（不是替换），
> 方便在已有字库基础上加字。

## 字库格式（核心约定）

这个工具存在的意义就是保证输出格式正确。格式定义只有一份：

```
阴码 / 列行式 / 逆向(低位在前) / C51格式
一个汉字 = 上半屏 16 字节 + 下半屏 16 字节 = 32 字节
字库每行 = 3 字节 UTF-8 编码 + 32 字节点阵 = 35 字节
```

生成的数组长这样：

```c
const uint8_t OLED_CF16x16[][35] =
{
    /* 上 */
    0xE4,0xB8,0x8A,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x40,...,
    /* 下 */
    0xE4,0xB8,0x8B,0x01,0x01,0x01,0x01,0x01,0x01,0xFF,...,
};

#define OLED_CF16x16_NUM   (sizeof(OLED_CF16x16) / sizeof(OLED_CF16x16[0]))
```

配套的显示函数：

```c
void OLED_ShowChinese(uint8_t Line, uint8_t Column, char *Chinese);      // 单个汉字
void OLED_ShowChineseString(uint8_t Line, uint8_t Column, char *String); // 中英混排
```

用法：

```c
OLED_ShowChineseString(1, 1, "嵌入式开发网");   // 汉字占 2 个 ASCII 字符宽
OLED_ShowChineseString(3, 1, "温度:25");       // 中英文自动混排
```

## 目录结构

```
oledfonttool\
  BUILD.lnk              双击构建（Windows，需自备 MSVC + Qt）
  build\                 构建产物
  scripts\
    build.cmd            可移植的构建入口（推荐，不依赖 .lnk）
    build.ps1            真正的构建逻辑
    add_bom.py           给脚本补 UTF-8 BOM
    make-shortcut.ps1    重建 BUILD.lnk
  src\
    main.cpp             界面、布局、导出            ┐
    fontgen.cpp/.h       渲染 + 编码（核心）          │ Qt 图形界面版
    fontpreview.cpp/.h   点阵预览网格                ┘
    CMakeLists.txt
    build_cfont.py       【命令行主入口】改这里加字    ┐
    gen_oled_hz.py       渲染/编码底层                │
    verify_cfont.py      校验字库格式与一致性          │ Python 命令行版
    dump_font.py         把字节还原成点阵图            ┘
    install-cfont.cmd    把字库搬进 Keil 工程
    OLED_CFont.h         命令行版的产物
    pctest\              PC 上验证 OLED 驱动，不用真机
      stm32f10x.h          桩头文件（别加进 Keil 工程）
      test_oled.c          I2C 从机模拟器，还原显存成 ASCII 图
installer\
  oledfonttool.iss        制作 Windows 安装包的脚本（Inno Setup）
  app.ico                 程序图标
```

## 从源码构建

### 图形界面版

需要 **Visual Studio 2022**（含 C++ 工具）+ **Qt 6**（msvc 版）+ CMake + Ninja。

```
双击 oledfonttool\scripts\build.cmd
```

构建脚本会自动探测工具链位置（环境变量 → 常见安装路径 → `vswhere`），
找到什么会在开头打印出来。都找不到时再改 `build.ps1` 里的 `$Fallback*` 变量。

产物：

```
build\oledfonttool.exe      开发用
package\                    分发用（已含全部运行库，可整个拷走）
```

### 制作分发包

**免安装 zip**：把 `package\` 压缩即可。

**安装包（可选）**：装了 [Inno Setup](https://jrsoftware.org/isinfo.php) 之后

```
# 1. 先把 package\ 放到仓库根目录（和 installer\ 同级）
# 2. 编译
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\oledfonttool.iss
# 产出 installer\Output\oledfonttool-<版本>-setup.exe
```

安装脚本支持自选安装路径、开始菜单与桌面快捷方式、控制面板卸载。

### 命令行版

改 `src\build_cfont.py` 顶部的 `CHARS`：

```python
CHARS = (
    "嵌入式开发网中文测试"
    "你好世界"
    "温度湿度光照电量电压电流功率时间年月日时分秒"
)
```

然后：

```bash
python oledfonttool/src/build_cfont.py      # 生成字库，并打印每个字的点阵供核对
```

## 不用真机验证驱动

`src\pctest\` 里有个 I2C 从机模拟器：它把驱动源码跑一遍，接收软件 I2C 时序，
还原出 SSD1306 的 128×64 显存，打印成图。改驱动代码时先在这里验一遍，能省下反复烧录。

```bash
cd oledfonttool/src/pctest
gcc -std=c99 -O1 -Wall -I. -I<你的工程>/Hardware \
    test_oled.c <你的工程>/Hardware/OLED.c -o test_oled.exe
./test_oled.exe
```

## 改这个项目之前

字库格式、脚本编码、界面、打包发布这几个方面都踩过坑，每条都记录了报错现象。
动手改之前建议看一眼 **[DEVELOPMENT.md](DEVELOPMENT.md)**。

## 许可

本仓库的工具代码采用 **MIT 协议**（见 [LICENSE](LICENSE)）—— 可以自由使用、修改、分发，包括商用，保留版权声明即可。

注意：如果你的 STM32 工程里用了 ST 标准外设库或 FreeRTOS，那部分代码的版权和许可
归各自作者所有，与本仓库无关。
