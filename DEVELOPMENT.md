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
