# OLED 汉字取模工具

给 `MyFreeRTOS_C8T6` 这个 STM32 工程配套的汉字取模工具。**输入文字 → 实时看点阵 → 导出
`OLED_CFont.h`**,替代 PCtoLCD2002 一个字一个字点、再复制 32 个十六进制数的做法。

有两种用法，产出格式完全一致，随便挑：

- **图形界面版**（Qt，双击 `BUILD.lnk` 构建）——适合加几个字、想边看边调字形
- **命令行版**（`src\build_cfont.py`）——适合一次加几十个字，或写进脚本

## 为什么要有它

PCtoLCD2002 能用，但有两个坑：

1. **取模方式选错就花屏**。你的驱动是**列行式**，软件默认/容易点成**逐行式**，
   导出的字模贴进去就是一片散点（`文字取模软件(赠送)\123.C` 就是这么来的）。
2. **手抄 32 个数字容易错**。漏一个字节肉眼看不出来，上机才发现花屏。

这个工具把这两件事都消掉了：格式写死在代码里，点阵实时可看，导出即用。

## 目录结构

```
oledfonttool\
  README.md              <- 本文件
  BUILD.lnk              <- 双击这个构建（不依赖文件关联）
  build\                 <- 构建产物：oledfonttool.exe + Qt 运行库
  scripts\               <- 构建用的脚本（一般不用碰）
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
    dump_font.py           验证外来字模
    install-cfont.cmd      把字库搬进工程（带备份）
    OLED_CFont.h           命令行版的产物
    pctest\                PC 上跑 OLED 驱动的测试台
      stm32f10x.h           桩头文件（别加进 Keil 工程）
      test_oled.c           I2C 从机模拟器，还原显存成图
```

## 构建图形界面版

需要：Visual Studio 2022（MSVC）、Qt 6.11.2（msvc2022_64）、CMake、Ninja。
你机器上都齐了，直接双击：

```
BUILD.lnk
```

产出 `build\oledfonttool.exe`，双击运行。Qt 运行库会自动拷到旁边。

`BUILD.lnk` 把"启动 powershell.exe + 参数"写死在快捷方式里，不依赖文件关联。
真正的构建逻辑在 `scripts\build.ps1`：失败时会停在窗口里，打印日志最后 40 行，
完整日志写到 `build-log.txt`。

也可以在 Qt Creator 里打开 `src\CMakeLists.txt`，选 **Desktop Qt 6.11.2 MSVC2022 64bit**
这个 kit，然后构建运行。

> 路径和我假设的不一样的话，改 `scripts\build.ps1` 开头那四个变量。

## 用

### 图形界面版

1. 左下输入要显示的汉字（可以多行，会自动去重）
2. 右边实时显示每个字的 16×16 点阵
3. 字体选**宋体**、阈值约 **110** 是默认值；字太细就把阈值调小，太粗调大
4. 点「导出 OLED_CFont.h」
5. 双击 `src\install-cfont.cmd` 搬运进工程
6. Keil 里 Rebuild

「导入已有字库」会把现成的 `OLED_CFont.h` 读回来**并合并**（不是替换），可以在它基础上加字。

**如果弹出"导出成功（需要手动搬运）"**：说明程序没有权限直接写工程目录
（原因见"踩过的坑"第 6 条）。按提示双击 `src\install-cfont.cmd` 即可，它会自动备份旧文件。

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
python oledfonttool\src\verify_cfont.py     # 校验是否逐字节一致
```

产出 `src\OLED_CFont.h`，再双击 `src\install-cfont.cmd` 搬运。

### 验证外来字模

从 PCtoLCD2002 手取了字模、或从网上抄来一段字节，喂给它就能看到点阵图：

```powershell
python oledfonttool\src\dump_font.py 0x00,0x40,0xFE,...
```

它会**同时按"列行式"和"逐行式"两种解释**画出图，并报孤立点数——散点少的那个才是对的。

## 编码格式（唯一权威定义）

```
阴码 / 列行式 / 逆向(低位在前) / C51格式
一个汉字 = 上半屏 16 字节 + 下半屏 16 字节 = 32 字节
字库每行 = 3 字节 UTF-8 编码 + 32 字节点阵 = 35 字节
```

`MyFreeRTOS_C8T6\Hardware\OLED.c` 的 `OLED_ShowChinese()` 就是按这个格式取数据。

Qt 版实现在 `src\fontgen.cpp`，Python 版在 `src\gen_oled_hz.py`。
**两条独立实现，改任何一边都要同步改另一边。** 目前两者算出的 70 行点阵已逐行比对过，完全一致。

## 一处硬要求

`OLED_CFont.h` 必须写成 **UTF-8**，且 Keil 的
`Edit → Configuration → Encoding` 要选 **UTF-8**。工具已经按 UTF-8 写出，
但如果你用别的编辑器打开再保存，可能被转成 GBK，字库就会查不到。

## 踩过的坑（改这个工具前先看）

**1. `.ps1` 必须带 UTF-8 BOM。**
Windows PowerShell 5.1 读没有 BOM 的 UTF-8 文件时按系统 ANSI 代码页（中文系统是 GBK）解释，
中文注释会变成乱码并凑出非法语法，脚本连解析都过不去，表现就是"窗口一闪而过"。
`build.ps1` 已内置自检自愈（启动时发现没 BOM 就自己补上再重启），
但编辑工具保存时经常去掉 BOM，**改完用 `scripts\add_bom.py` 补一下**。

**2. `.cmd` 必须纯 ASCII，且不能写中文路径字面量。**
cmd.exe 按 ANSI（GBK）解析 `.cmd`，中文字节会被解错导致脚本直接崩。
现有脚本一律用 `%~dp0` 推导路径，从根上避开。

**3. 样式表里不能写全局 `color`。**
Windows 深色主题下 `QMessageBox` 用深色背景，而全局深色文字会让弹窗**完全看不见内容**
（看起来"只有标题没有正文"）。现在统一用 Fusion 风格 + 浅色调色板，并给 `QMessageBox`
显式配色。踩这个坑时，我误以为是自检逻辑有 bug，白查了好几轮。

**4. 十六进制不要对整串 `toUpper()`。**
`QStringLiteral("0x%1").arg(...).toUpper()` 会把前缀的 `x` 变成 `X`，
生成 `0XE4` 这种数据。C++ 能编译，但与字库约定的大小写不一致，
自检若用 `startsWith("0x")` 会误报。现在只把两位数值转大写。

**5. 字库数组不能用 `{"\xE4\xB8\x8A",0x00,...}` 这种写法。**
字符串字面量是"字符数组"类型，和字节混在同一个初始化列表里时，从第二行开始编译器
会把字符串当成指针值：

```
warning: initialization of 'unsigned char' from 'char *'
error:   initializer element is not computable at load time
```

所以字库里**全部写十六进制字节**，汉字只放在注释里。另外数组要声明成 `[][35]`
（3 字节编码 + 32 字节点阵），写成 `[][32]` 会报 `excess elements in struct initializer`。

**6. 导出路径不要写死，也不要被"另存为"永久改掉。**
早期版本遇到写不进去会弹"另请选择位置"，用户一旦选到工程根目录，
本次运行后续导出就都写错地方了。现在每次启动都从 exe 所在目录**向上逐级查找**
`MyFreeRTOS_C8T6/Hardware`。

**7. 这个 exe 进程不能写工程目录（环境限制，不是代码问题）。**
在用户这台机器上，程序往 `MyFreeRTOS_C8T6\Hardware\` 和**系统临时目录**都写不进去
（拒绝访问），只有程序自己的目录可以；而同一个账号下的 cmd 写这两个目录都没问题。
连 `%TEMP%` 都不行，说明是**进程被限制了写入范围**（安全软件对新建未签名程序的沙箱处理），
不是 NTFS 权限问题。

处理办法是**自动降级**：导出失败时改写到程序目录，再双击 `src\install-cfont.cmd` 搬运。

## PC 上验证 OLED 驱动（不用真机）

`src\pctest\` 里有个 I2C 从机模拟器，能在电脑上把 `OLED.c` 跑一遍、把 SSD1306 显存
还原成 128×64 的 ASCII 图。改驱动代码时先在这里验一遍，能省下反复烧录的时间。

```powershell
cd oledfonttool\src\pctest
$env:PATH = "D:\Qt\Tools\mingw1310_64\bin;$env:PATH"   # 编译器要在 PATH 里找到自己的 DLL
gcc -std=c99 -O1 -Wall -Wextra -I. `
    -I"..\..\..\MyFreeRTOS_C8T6\Hardware" `
    test_oled.c "..\..\..\MyFreeRTOS_C8T6\Hardware\OLED.c" -o test_oled.exe
.\test_oled.exe
```

`stm32f10x.h` 是给 PC 编译用的桩头文件（替代 STM32 标准库），**不要加进 Keil 工程**。
