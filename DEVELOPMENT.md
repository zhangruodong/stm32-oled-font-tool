# 开发注意事项

改这个项目之前建议先看一遍，下面每条都是实际踩出来的，附有报错现象。

## 字库生成

**1. 字库数组不能用 `{"\xE4\xB8\x8A",0x00,...}` 这种写法**

字符串字面量是"字符数组"类型，和字节元素混在同一个初始化列表里时，从第二行开始
编译器会把字符串当成指针值：

```
warning: initialization of 'unsigned char' from 'char *'
error:   initializer element is not computable at load time
```

**所以字库全部写十六进制字节，汉字只放在注释里。**

**2. 数组要声明成 `[][35]`，不是 `[][32]`**

3 字节 UTF-8 编码 + 32 字节点阵 = 35。写成 32 会报
`excess elements in struct initializer`。

**3. 十六进制不要对整串 `toUpper()`**

`QStringLiteral("0x%1").arg(...).toUpper()` 会把前缀的 `x` 也变成 `X`，生成 `0XE4`。
C++ 能编译（十六进制前缀大小写不敏感），但与约定不一致，导出前的自检会误报。

**4. 两套渲染实现必须同步改**

Qt 版（`src/fontgen.cpp`，用 `QPainter`）和 Python 版（`src/gen_oled_hz.py`，用 Pillow）
是**两套独立实现**，都按 16px 宋体渲染。两者的输出已逐行比对过、完全一致。
**改任何一边都要同步改另一边**，否则两个版本产出的字库会不同。

## 脚本

**5. `.ps1` 必须带 UTF-8 BOM**

Windows PowerShell 5.1 读**没有 BOM** 的 UTF-8 文件时按系统 ANSI 代码页（中文系统是 GBK）
解释，中文注释变成乱码后凑出非法语法，脚本**连解析都过不去**，现象是窗口一闪而过：

```
Missing type name after '['.
Missing closing '}' in statement block or type definition.
```

`build.ps1` 内置了自检自愈（启动时发现没 BOM 就自己补上再重启）。
`scripts/add_bom.py` 可以手动给文件补 BOM。

**6. `.cmd` 必须纯 ASCII，且不能写中文路径字面量**

cmd.exe 按 ANSI（GBK）解析 `.cmd`，中文字节会被解错导致脚本崩。
现有脚本一律用 `%~dp0` 推导路径，从根上避开。

**7. PowerShell 函数必须先定义再调用**

PowerShell 顺序执行，把 `Say` 这类自定义函数的调用写在定义之前，
会报 `不是 cmdlet、函数、脚本文件或可运行程序的名称` 并直接退出——现象同样是窗口一闪而过。
**语法检查查不出这种问题**（语法完全合法），改完脚本要实跑一次。

**8. 找上层目录不要手工数层数**

`Split-Path -Parent` 连写几层极易数错（`cl.exe` 到 VS 根目录要往上 6 层）。
用循环逐级上溯、遇到目标就停，见 `build.ps1` 里找 `VC\Redist\MSVC` 的写法。

## 界面

**9. 样式表里不能写全局 `color`**

Windows 深色主题下 `QMessageBox` 用深色背景，全局深色文字会让弹窗**完全看不见内容**
（看起来"只有标题没有正文"）。现在统一用 Fusion 风格 + 浅色调色板，
并给 `QMessageBox` 显式配色。

**10. 固定宽度会让分隔线拖不动**

左边面板不能写 `setFixedWidth()`，否则 `QSplitter` 无法调整。
用 `setMinimumWidth()` 加合适的尺寸策略。

**11. 标签要开 `setWordWrap(true)`**

否则文字超出面板宽度会被直接切掉，看起来像"显示不全"。

## 打包发布

**12. 别把调试版运行库打进发布包**

`windeployqt` 会连同**微软 C 运行库的调试版**（`msvcp140d.dll`、`vcruntime140d.dll` 等）
一起拷进来，这些是**不允许再分发**的。

判断规则不能简单用"以 d 结尾"——`qcertonlybackend.dll` 里的 `d` 属于 backend 这个词，
是正常发布版。`build.ps1` 里用前缀加 `d.dll` / `d_` 的组合判断。

**13. 发布包必须带 VC 运行库**

`windeployqt` 默认**不拷** `msvcp140.dll` / `vcruntime140.dll`。
不加 `--compiler-runtime` 的话，在没装过 VC++ 运行库的电脑上会报
"找不到 msvcp140.dll"起不来。

要分发就得用允许再分发的那份——在 VS 的
`VC\Redist\MSVC\<版本>\x64\Microsoft.VC*.CRT\` 目录里，**不要用 System32 里的**。

**14. 提交前确认没有二进制混入**

`build\`、`package\`、`installer\Output\`、`*.zip` 都已在 `.gitignore` 里排除。
二进制文件每次构建都变，git 为每份存一个完整副本，仓库会迅速膨胀而且清不掉。

提交前可以自查：

```bash
git diff --cached --name-only | grep -E '\.(exe|dll|zip|obj|pdb)$'
```

没有输出就是干净的。

**15. `.git` 里也可能留下大文件**

如果曾误 `git add` 过大文件（即使之后 `git rm --cached` 移出），
它在对象库里仍是未被引用的对象。用这个回收：

```bash
git reflog expire --expire=now --all
git gc --prune=now --aggressive
```

## 来源不明的现象（已查清）

**16. Keil 编译中文报 `missing closing quote`**

Keil 的 ARMCC 5.x **按系统 ANSI 代码页（简体中文 Windows 是 GBK）读源文件**。
工程里的 `.c` 是 UTF-8，中文字节被当 GBK 解码后引号配对错乱：

```
main.c(275): error: #8: missing closing quote
    OLED_ShowChineseString( 1, 1, "吉新豪纯傻逼" );
```

**关键前提：ARMCC 不做任何转码 —— 源文件里是什么字节，字符串字面量里就是什么字节。**
实测（文件分别存成 GBK / UTF-8+BOM / UTF-8），编译后字符串的字节是
`BC AA` / `E5 90 89` / `E5 90 89`。

所以**源码和字库必须用同一种编码**。不一致的话编译能过、程序也能跑，
但屏幕上一片空白 —— 驱动是按三字节逐字节比较的，对不上就查不到字形。

修法（两个都做，互为保险）：

| 做法 | 位置 |
|---|---|
| 加编译参数 `--no_multibyte_chars` | `Project.uvprojx` → `<Cads>` → `<VariousControls>` → `<MiscControls>` |
| 源文件存成 **UTF-8 + BOM** | `User\main.c` |

另外把 Keil 的 `Edit → Configuration → Editor → Encoding` 设成 **UTF-8**，
否则在 Keil 里编辑后按 Ctrl+S 可能把文件另存成 GBK，前功尽弃。

ARMCC 5.06 **不支持** `--source-encoding=utf-8`（报 `C3900U: Unrecognized option`），
`--locale=utf8` 也无效。

**17. 程序写不进工程目录，报"拒绝访问"**

现象很有迷惑性：**同一个目录，别的程序能写，只有本程序写不进去。**

```
Python / PowerShell  写 Hardware\  → 成功
Qt 程序              写 Hardware\  → 失败（拒绝访问）
```

排查过程与结论：

| 检查项 | 结果 |
|---|---|
| NTFS 权限 | 正常（`Authenticated Users: Modify`） |
| Defender 受控文件夹访问 | `EnableControlledFolderAccess: 0`（关闭） |
| Qt 的三种写文件 API | `QFile::open` / `QFile::copy` / `QSaveFile` **全部被拒** |
| 同一 exe 换位置运行 | 项目目录树里 → **失败**；"我的文档"或工程目录里 → **成功** |

**根因是目录上的一条 ACL 拒绝规则**，由某个 Windows 应用包（MSIX）加上：

```
Everyone : DeleteSubdirectoriesAndFiles : Deny
（同目录还有一条 S-1-4-* 的包能力 ACE，Allow）
```

`DeleteSubdirectoriesAndFiles` 同时是 `Remove-Item -Recurse` 需要的权限，
所以这条规则也会导致"删目录树失败"。

**`icacls /remove:d` 对目录会失败**（报 `Failed processing 1 files`），要用 PowerShell：

```powershell
$acl = Get-Acl 'G:\项目目录'
$acl.Access | Where-Object { $_.AccessControlType -eq 'Deny' } |
    ForEach-Object { $acl.RemoveAccessRule($_) | Out-Null }
Set-Acl -Path 'G:\项目目录' -AclObject $acl
```

工具本身**不依赖**这条规则被删掉：写入失败时会自动降级到程序自己的目录，
并提示双击 `install-cfont.cmd` 搬运。删掉规则只是省去这一步。

**18. 「另存为」要点两次才生效**

`exportHeader()` 内部在 `needAsk` 为真时会自己弹一次保存对话框；
而「另存为…」按钮已经让用户选过路径了，再调 `exportHeader()`
就会**再弹一次**，用户得点两次"保存/替换"才真正写文件。

修法：`exportHeader(bool askForPath = true)` 加参数，「另存为」传 `false`。
默认值保证信号槽直连（`&MainWindow::exportHeader`）仍能编译。

**19. 工程里"混合编码"是正常的，不要试图统一**

实际工程的文件编码很杂，实测：

```
main.c              UTF-8 + BOM     ← 有中文字符串，必须和字库同为 UTF-8
LED.c / OLED.c      UTF-8
OLED_CFont.h        UTF-8
FreeRTOSConfig.h    GBK
stm32f10x_it.c      GBK
stm32f10x.h         GBK
```

**这不影响功能**，因为只有"字符串字面量里的中文"才参与查表：

| 中文出现的位置 | 会不会影响 |
|---|---|
| 注释里（`stm32f10x_it.c` 等） | ❌ 不影响 |
| 字符串字面量里（`main.c`） | ✅ **必须和字库同编码** |

而且 `OLED_CFont.h` 里真正的数据是 **ASCII 十六进制**（`0xE5,0x90,0x89,...`），
中文只在注释里 —— 所以这个文件什么编码都能正确编译，注释乱码只影响阅读。

**判断一个文件会不会出问题**：看它有没有"含非 ASCII 字符的字符串字面量"。
`main.c` 有，所以它是唯一需要关心编码的源文件。

工具里已经内置了这个检查（`checkSourceEncoding()`）：启动时和导出后
都会扫一遍工程，发现编码不匹配就提醒。检查逻辑与上面的判据一致。

**为什么不做"输出 GBK 字库"选项**：

GBK 下一个汉字 2 字节，字库每行会从 35 变成 34 字节，
驱动的查表（`OLED_CFont_Get` 固定比较 3 字节、固定跳过 3 字节）要跟着改。
而且 Qt 6 移除了 `QTextCodec`，只剩 `QStringConverter` 的
`Utf8 / Utf16 / Utf32 / Latin1 / System` —— 拿不到真正的 GBK，
只能用 `System`（跟随系统代码页），换台非中文 Windows 就会输出错的编码。
代价大于收益，所以保持 UTF-8 单一格式。
