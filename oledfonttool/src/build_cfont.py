# -*- coding: utf-8 -*-
"""生成 OLED_CFont.h —— 汉字 16x16 字模表

改字方法：把 CHARS 改成你要显示的文字，重新运行本脚本。
输出到 tools/OLED_CFont.h，复制到 MyFreeRTOS_C8T6/Hardware/ 覆盖同名文件即可。
（也可以直接改 OUT 指向工程目录，但那个目录可能没有写权限。）
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_oled_hz import glyph, bits_to_bytes, load_font, show_bits, render_header   # noqa: E402

# 想显示的汉字，重复的会自动去掉。ASCII 字符不用写进来（走原有字库）。
CHARS = (
    "嵌入式开发网中文测试"      # 取模软件 123.C 里的参考文字
    "你好世界"
    "温度湿度光照电量电压电流功率时间年月日时分秒"
    "开关机状态正常错误报警设置返回上下左右确认取消"
    "欢迎使用系统初始化完成运行中请稍候"
)

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "OLED_CFont.h")

HEAD = '''#ifndef __OLED_CFONT_H
#define __OLED_CFONT_H

/*==============================================================================
 *  汉字字模库  16x16 点阵，每个汉字 32 字节
 *
 *  本文件由 tools/build_cfont.py 自动生成，不要手工编辑。
 *  要增删汉字：改脚本里的 CHARS，重新运行脚本即可。
 *
 *  取模格式（与 OLED.c 的 OLED_ShowChinese 对应）：
 *      阴码 / 列行式 / 逆向(低位在前) / C51格式
 *      一个汉字 = 上半屏 16 字节 + 下半屏 16 字节
 *
 *  每行的排布：前 3 字节是该字的 UTF-8 编码，后面 32 字节是点阵。
 *  这里全部写成十六进制字节，汉字只放在注释里——因为把字符串字面量
 *  （"\\xNN\\xNN\\xNN"）和字节混在同一个初始化列表里，从第二行起编译器会
 *  把字符串当成指针值，报 initializer element is not computable at load time。
 *
 *  注意：本文件必须是 UTF-8 编码保存（Keil 里 Edit -> Configuration ->
 *        Encoding 选 UTF-8），否则注释会乱码（不影响编译，但不好读）。
 *============================================================================*/

'''

TAIL = '''
#endif
'''


def main():
    chars = sorted(set(CHARS), key=lambda c: c.encode("utf-8"))
    font = load_font()

    rows = []
    for ch in chars:
        bits = glyph(ch, font)
        data = bits_to_bytes(bits)
        if len(data) != 32:
            raise SystemExit("字符 %r 生成了 %d 字节, 应为 32" % (ch, len(data)))
        show_bits(bits, "=== %s ===" % ch)
        rows.append((ch, list(ch.encode("utf-8")) + data))

    path = os.path.abspath(OUT)
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(HEAD)
        f.write(render_header(rows))
        f.write("\n")
        f.write(TAIL.lstrip("\n"))

    print("\n写出 %s" % path)
    print("汉字 %d 个, 每行 35 字节, 共 %d 字节 (%d KB)"
          % (len(rows), len(rows) * 35, (len(rows) * 35 + 512) // 1024))
    print("复制到 MyFreeRTOS_C8T6/Hardware/ 覆盖同名文件, 然后 Keil 里 Rebuild")


if __name__ == "__main__":
    main()
