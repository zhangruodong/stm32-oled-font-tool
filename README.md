# STM32 OLED 汉字取模工具

给 **STM32 + SSD1306 OLED** 用的点阵取模工具。输入汉字，实时预览 16×16 点阵，
一键生成可以直接编进工程的 C 字库文件。

**替代 PCtoLCD2002 那套流程**，并且把它的两个大坑堵掉了：

| PCtoLCD2002 的问题 | 这个工具的做法 |
|---|---|
| 取模方式要选"列行式/逐行式/顺向/逆向"，**选错就花屏**，而且错了看不出来 | **格式写死在代码里**，不可能选错 |
| 手动复制 32 个十六进制字节，**漏抄一个就花屏** | 自动生成 + **导出前逐行校验**（每行必须正好 35 字节） |
| 汉字要用 GB2312 区位索引或另建字库 | 直接按 **UTF-8 编码**查表，源码里写 `"温度:25℃"` 就能用 |
| 没有体积概念 | 实时显示字库占多少 Flash |

## 快速开始（不想编译的话）

到 [Releases](https://github.com/zhangruodong/stm32-oled-font-tool/releases/latest) 下载，两种版本内容相同，任选一个：

| 版本 | 用法 | 特点 |
|---|---|---|
| `oledfonttool-1.0-setup.exe` | 双击按向导安装 | 可自选安装路径，创建开始菜单/桌面快捷方式，可在控制面板卸载 |
| `oledfonttool-1.0-portable.zip` | 解压后双击 `oledfonttool.exe` | 免安装（绿色版），不用了就删文件夹 |

**两种都不需要安装 Qt、Visual Studio、或任何运行库** —— 都在包里了。

用法：

1. 左边输入框写你要显示的汉字（多行也行，会自动去重）
2. 右边实时显示每个字的点阵，核对字形
3. 点「导出 OLED_CFont.h」
4. 把生成的文件放进 Keil 工程的 `Hardware\` 目录，Rebuild

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

配套的驱动函数（`Hardware\OLED.c`）：

```c
void OLED_ShowChinese(uint8_t Line, uint8_t Column, char *Chinese);      // 单个汉字
void OLED_ShowChineseString(uint8_t Line, uint8_t Column, char *String); // 中英混排
```

用法：

```c
OLED_ShowChineseString(1, 1, "嵌入式开发网");   // 汉字占 2 个 ASCII 字符宽
OLED_ShowChineseString(3, 1, "温度:25℃");      // 中英文自动混排
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
installer\              制作 Windows 安装包（Inno Setup）
  oledfonttool.iss        安装脚本
  app.ico                 程序图标
```

**两种用法产出的格式完全一致**，随便挑：

- **图形界面版** —— 适合加几个字、想边看边调字形
- **命令行版** —— 适合一次加几十个字

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
python oledfonttool/src/verify_cfont.py     # 校验格式
```

### 验证外来字模

从别处抄来的 32 字节字模，可以还原成点阵图看看到底对不对：

```bash
python oledfonttool/src/dump_font.py 0x00,0x40,0xFE,...
```

它会**同时按"列行式"和"逐行式"两种解释**画图，并统计孤立点数 ——
**散点少的那个才是正确解释**。这个办法当初就是用来证明某个网上下载的
字模文件是"逐行式"、和驱动不匹配的。

## 不用真机验证驱动

`src\pctest\` 里有个 I2C 从机模拟器：它把 `OLED.c` 跑一遍，接收软件 I2C 时序，
还原出 SSD1306 的 128×64 显存，打印成图。改驱动代码时先在这里验一遍，能省下反复烧录。

```bash
cd oledfonttool/src/pctest
gcc -std=c99 -O1 -Wall -I. -I../../MyFreeRTOS_C8T6/Hardware \
    test_oled.c ../../MyFreeRTOS_C8T6/Hardware/OLED.c -o test_oled.exe
./test_oled.exe
```

## 踩过的坑（改这个项目前先看）

这些都是实际踩出来的，每条都有明确的报错现象：

**1. 字库数组不能用 `{"\xE4\xB8\x8A",0x00,...}` 这种写法**

字符串字面量是"字符数组"类型，和字节元素混在同一个初始化列表里时，从第二行开始
编译器会把字符串当成指针值：

```
warning: initialization of 'unsigned char' from 'char *'
error:   initializer element is not computable at load time
```

**所以字库全部写十六进制字节，汉字只放在注释里。**

**2. 数组要声明成 `[][35]`，不是 `[][32]`**

3 字节 UTF-8 编码 + 32 字节点阵 = 35。写成 32 会报 `excess elements in struct initializer`。

**3. 十六进制不要对整串 `toUpper()`**

`QStringLiteral("0x%1").arg(...).toUpper()` 会把前缀的 `x` 也变成 `X`，生成 `0XE4`。
C++ 能编译（十六进制前缀大小写不敏感），但与约定不一致，自检会误报。

**4. `.ps1` 脚本必须带 UTF-8 BOM**

Windows PowerShell 5.1 读**没有 BOM** 的 UTF-8 文件时按系统 ANSI 代码页（中文系统是 GBK）
解释，中文注释变成乱码后凑出非法语法，脚本**连解析都过不去**，现象是窗口一闪而过：

```
Missing type name after '['.
Missing closing '}' in statement block or type definition.
```

`build.ps1` 内置了自检自愈（启动时发现没 BOM 就自己补上再重启）。

**5. `.cmd` 必须纯 ASCII，且不能写中文路径字面量**

cmd.exe 按 ANSI（GBK）解析 `.cmd`，中文字节会被解错导致脚本崩。
现有脚本一律用 `%~dp0` 推导路径，从根上避开。

**6. 样式表里不能写全局 `color`**

Windows 深色主题下 `QMessageBox` 用深色背景，全局深色文字会让弹窗**完全看不见内容**
（看起来"只有标题没有正文"）。现在统一用 Fusion 风格 + 浅色调色板，并给 `QMessageBox` 显式配色。

**7. 打包发布时别把调试版运行库带进去**

`windeployqt` 会连同 **微软 C 运行库的调试版**（`msvcp140d.dll`、`vcruntime140d.dll` 等）
一起拷进来，这些是**不允许再分发**的。判断规则不能简单用"以 d 结尾"——
`qcertonlybackend.dll` 里的 `d` 属于 backend 这个词，是正常发布版。

**8. 发布包要带 VC 运行库**

`windeployqt` 默认**不拷** `msvcp140.dll` / `vcruntime140.dll`。
不加 `--compiler-runtime` 的话，在没装过 VC++ 运行库的电脑上会报"找不到 msvcp140.dll"起不来。

## 关于字模渲染

Qt 版（`QPainter`）和 Python 版（`Pillow`）是**两套独立实现**，都按 16px 宋体渲染。
两者的输出已逐行比对过，**完全一致**。改任何一边都要同步改另一边。

## 许可

本仓库的工具代码采用 **MIT 协议**（见 [LICENSE](LICENSE)）—— 可以自由使用、修改、分发，包括商用，保留版权声明即可。

**注意**：如果你把本工具配合 STM32 标准外设库一起使用，那部分代码的版权归
**STMicroelectronics**（见各源文件头部的版权声明）；FreeRTOS 遵循 **MIT** 许可。
这些第三方组件不在本仓库内，其许可各自独立。
