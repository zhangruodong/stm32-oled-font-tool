# -*- coding: utf-8 -*-
"""
生成 OLED 16x16 汉字字模（SSD1306 / 江科大驱动 列行式 逆向 阴码 格式）

用法:
    python gen_oled_hz.py 你好世界 温度 湿度

输出:
    1. Hardware/OLED_Font.h 里的 C 数组文本（打印到屏幕，也可 -o 写文件）
    2. 逐字符 32 字节校验（每个汉字必须正好 32 字节，否则报错）

为什么用脚本而不是手抄:
    手抄字模容易出现字节漏抄、错位，脚本生成可以保证
    "一个汉字 == 32 字节 == 上半屏 16 字节 + 下半屏 16 字节"。
"""
import sys

from PIL import Image, ImageDraw, ImageFont

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\simsun.ttc",
    r"C:\Windows\Fonts\simhei.ttf",
    r"C:\Windows\Fonts\msyh.ttc",
]

SIZE = 16          # 点阵 16x16
THRESHOLD = 110    # 灰度阈值，越小字越粗


def load_font():
    for path in FONT_CANDIDATES:
        try:
            return ImageFont.truetype(path, SIZE)
        except OSError:
            continue
    raise SystemExit("找不到中文字体，请检查 FONT_CANDIDATES 里的路径")


def glyph(ch, font):
    """
    把一个汉字渲染成 16x16 的 0/1 点阵（1 = 点亮）

    字体按 16 像素加载，文字直接画在 16x16 画布上（和取模软件"宋体 16"一致），
    再按实际墨迹把字形在格子里居中，避免整体偏左或贴边。
    """
    canvas = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(canvas).text((0, 0), ch, fill=255, font=font)

    box = canvas.getbbox()
    if box is None:
        raise SystemExit("字符 %r 渲染为空，可能不是汉字或字体不支持" % ch)

    dx = ((SIZE - (box[2] - box[0])) // 2) - box[0]
    dy = ((SIZE - (box[3] - box[1])) // 2) - box[1]
    if dx or dy:
        shifted = Image.new("L", (SIZE, SIZE), 0)
        shifted.paste(canvas, (dx, dy))
        canvas = shifted

    px = canvas.load()
    return [[1 if px[x, y] >= THRESHOLD else 0 for x in range(SIZE)]
            for y in range(SIZE)]


def bits_to_bytes(bits):
    """
    列行式 + 逆向(低位在前):
    每次取一列 8 行，低位在上；上半屏 16 字节，下半屏 16 字节。
    """
    out = []
    for half in (0, 1):
        y0 = half * 8
        for x in range(SIZE):
            byte = 0
            for i in range(8):
                if bits[y0 + i][x]:
                    byte |= (1 << i)          # 低位在前 => 上面的点进低位
            out.append(byte)
    return out


def bytes_to_bits(data):
    """上面的逆运算：把 32 字节还原成 16x16 点阵，用来逐个核对"""
    bits = [[0] * SIZE for _ in range(SIZE)]
    for half in (0, 1):
        y0 = half * 8
        for x in range(SIZE):
            byte = data[half * SIZE + x]
            for i in range(8):
                if byte & (1 << i):
                    bits[y0 + i][x] = 1
    return bits


def show_bits(bits, title=""):
    """用 ASCII 把点阵打印出来，肉眼核对字形"""
    if title:
        print(title)
    for row in bits:
        print("  " + "".join("#" if v else "." for v in row))


def render_header(rows):
    """
    生成 C 数组文本。rows = [(汉字, [35 个字节]), ...]

    格式说明（很重要，别改成 {"\\xNN",0x..} 那种写法）：
        字符串字面量是"字符数组"类型，和字节元素混在同一个初始化列表里时，
        编译器会把后续行的字符串当成指针值，报：
            error: initializer element is not computable at load time
        所以这里把整行都写成十六进制字节，汉字只放在注释里。
    """
    lines = ["/* 汉字码表：每行 35 字节 = UTF-8 三字节编码 + 16x16 点阵(32 字节) */",
             "const uint8_t OLED_CF16x16[][35] =",
             "{"]
    for ch, vals in rows:
        assert len(vals) == 35, ch
        lines.append("\t/* %s */" % ch)
        lines.append("\t%s" % (",".join("0x%02X" % b for b in vals) + ","))
    lines.append("};")
    lines.append("")
    lines.append("/* 汉字个数 */")
    lines.append("#define OLED_CF16x16_NUM   (sizeof(OLED_CF16x16) / sizeof(OLED_CF16x16[0]))")
    return "\n".join(lines)


def dump_hex(text):
    """把十六进制字节串还原成点阵并打印，用于核对取模软件的输出"""
    data = [int(v, 16) for v in text.replace("0x", " ").replace(",", " ").split()]
    if len(data) != 32:
        raise SystemExit("一个汉字必须是 32 字节, 现在是 %d 字节" % len(data))
    show_bits(bytes_to_bits(data))


def main():
    argv = sys.argv[1:]
    if argv and argv[0] == "--dump":
        dump_hex(" ".join(argv[1:]))
        return

    argv = [a for a in argv if not a.startswith("-")]
    text = "".join(argv)
    if not text:
        raise SystemExit("用法: python gen_oled_hz.py 你好世界")

    font = load_font()
    chars = sorted(set(text), key=lambda c: c.encode("utf-8"))   # 按 UTF-8 排序
    rows = []
    for ch in chars:
        bits = glyph(ch, font)
        data = bits_to_bytes(bits)
        if len(data) != 32:
            raise SystemExit("字符 %r 生成了 %d 字节, 应为 32" % (ch, len(data)))
        show_bits(bits, "=== %s ===" % ch)
        rows.append((ch, list(ch.encode("utf-8")) + data))

    print()
    print(render_header(rows))
    print()
    print("/* 共 %d 字, 每字 = 3 字节编码 + 32 字节点阵 */" % len(rows))


if __name__ == "__main__":
    main()
