# OLED 汉字取模工具

给 STM32 + SSD1306 工程配套的汉字取模工具。**输入文字 → 实时看点阵 → 导出
`OLED_CFont.h`**，一个字一个字地手攒字模这件事就不必再做了。

有两种用法，产出格式完全一致，按需要选：

- **图形界面版**（Qt）——适合加几个字、想边看边调字形
- **命令行版**（`src\build_cfont.py`）——适合一次加几十个字，或写进脚本

## 它解决什么

取模最容易出的两个问题，这个工具都从源头消掉了：

1. **取模参数选错**。字模有"列行式 / 逐行式 / 顺向 / 逆向"多种排列，
   和驱动的读取方式不匹配就会显示成一片散点。本工具**格式写死在代码里**，
   不需要你选任何参数。
2. **手抄 32 个十六进制数**。漏一个字节肉眼根本看不出来，上机才发现不对。
   本工具自动生成，并且**导出前逐行校验**（每行必须正好 35 字节）。

再加上点阵实时可看、Flash 占用实时可见、已有字库可导入合并。

## 目录结构

```
oledfonttool\
  README.md              <- 本文件
  BUILD.lnk              <- 双击构建的快捷方式（本机专用，不提交进仓库）
  build\                 <- 构建产物：oledfonttool.exe + Qt 运行库
  scripts\               <- 构建用的脚本（一般不用碰）
    build.cmd              构建入口，双击即可（推荐）
    build.ps1              真正的构建逻辑
    make-shortcut.ps1      重建 BUILD.lnk
    add_bom.py             给文件补 UTF-8 BOM
  src\                   <- 所有源码
    main.cpp               界面、布局、导出
    fontgen.cpp/.h         渲染 + 编码（核心）
    fontpreview.cpp/.h     点阵预览网格
    CMakeLists.txt
    build_cfont.py         【命令行主入口】改这里加字
    gen_oled_hz.py         渲染/编码底层
    verify_cfont.py        校验字库是否逐字节一致
    dump_font.py           把字节还原成点阵图
    install-cfont.cmd      把字库搬进工程（带备份）
    OLED_CFont.h           命令行版的产物
    pctest\                PC 上跑 OLED 驱动的测试台
      stm32f10x.h           桩头文件（别加进 Keil 工程）
      test_oled.c           I2C 从机模拟器，还原显存成图
```

## 构建图形界面版

需要：Visual Studio 2022（含 C++ 工具）、Qt 6（msvc 版）、CMake、Ninja。

```
双击 scripts\build.cmd
```

脚本会自动探测这些工具装在哪，并把结果打印在开头。探测不到时才需要改
`scripts\build.ps1` 开头的 `$Fallback*` 变量。

产出 `build\oledfonttool.exe`，双击运行。Qt 运行库会自动拷到旁边。

真正的构建逻辑在 `scripts\build.ps1`：失败时会停在窗口里，打印日志最后 40 行，
完整日志写到 `build-log.txt`。

也可以在 Qt Creator 里打开 `src\CMakeLists.txt`，选 MSVC 版的 Qt 6 kit，然后构建运行。

## 用

### 图形界面版

1. 左下输入要显示的汉字（可以多行，会自动去重）
2. 右边实时显示每个字的 16×16 点阵
3. 字体选**宋体**、阈值约 **110** 是默认值；字太细就把阈值调小，太粗调大
4. 点「导出 OLED_CFont.h」
5. 双击 `src\install-cfont.cmd` 搬运进工程
6. Keil 里 Rebuild

「导入已有字库」会把现成的 `OLED_CFont.h` 读回来**并合并**（不是替换），可以在它基础上加字。

**如果弹出"导出成功（需要手动搬运）"**：说明程序没有权限直接写工程目录。
按提示双击 `src\install-cfont.cmd` 即可，它会自动备份旧文件。

### 命令行版

改 `src\build_cfont.py` 顶部的 `CHARS`：

```python
CHARS = (
    "嵌入式开发网中文测试"
    "你好世界"
    ...
)
```

然后：

```powershell
python oledfonttool\src\build_cfont.py      # 生成字库，并打印每个字的点阵供核对
```

产出 `src\OLED_CFont.h`，再双击 `src\install-cfont.cmd` 搬运。

## 编码格式

```
阴码 / 列行式 / 逆向(低位在前) / C51格式
一个汉字 = 上半屏 16 字节 + 下半屏 16 字节 = 32 字节
字库每行 = 3 字节 UTF-8 编码 + 32 字节点阵 = 35 字节
```

工程里 `Hardware\OLED.c` 的 `OLED_ShowChinese()` 就是按这个格式取数据。

Qt 版实现在 `src\fontgen.cpp`，Python 版在 `src\gen_oled_hz.py`。
**两条独立实现，改任何一边都要同步改另一边。** 目前两者算出的 70 行点阵已逐行比对过，完全一致。

## 一处硬要求

`OLED_CFont.h` 必须写成 **UTF-8**，且 Keil 的
`Edit → Configuration → Encoding` 要选 **UTF-8**。工具已经按 UTF-8 写出，
但如果你用别的编辑器打开再保存，可能被转成 GBK，字库就会查不到。

## PC 上验证 OLED 驱动（不用真机）

`src\pctest\` 里有个 I2C 从机模拟器，能在电脑上把驱动源码跑一遍、把 SSD1306 显存
还原成 128×64 的 ASCII 图。改驱动代码时先在这里验一遍，能省下反复烧录的时间。

```powershell
cd oledfonttool\src\pctest
# 把 <工程> 换成你自己的工程目录
gcc -std=c99 -O1 -Wall -Wextra -I. `
    -I"<工程>\Hardware" `
    test_oled.c "<工程>\Hardware\OLED.c" -o test_oled.exe
.\test_oled.exe
```

`stm32f10x.h` 是给 PC 编译用的桩头文件（替代 STM32 标准库），**不要加进 Keil 工程**。
