#include "fontgen.h"

#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

FontGen::FontGen(const QFont &font, int threshold)
	: m_font(font), m_threshold(threshold)
{
	if (m_font.family().isEmpty())
	{
		m_font = QFont(QStringLiteral("SimSun"), 16);   /* 和取模软件的"宋体 16"对应 */
	}
	m_font.setPixelSize(SIZE);
}

void FontGen::setFont(const QFont &font)
{
	m_font = font;
	m_font.setPixelSize(SIZE);
	m_cache.clear();
}

void FontGen::setThreshold(int threshold)
{
	if (threshold != m_threshold)
	{
		m_threshold = threshold;
		m_cache.clear();
	}
}

/*
 * 渲染：16x16 画布上画字 -> 取墨迹边界 -> 整体平移居中 -> 按阈值二值化
 * 平移这一步不能省：宋体的字形在字身框里天然偏左偏上，不居中的话点阵会贴边
 */
QVector<QVector<bool>> FontGen::renderBits(QChar ch) const
{
	const int S = SIZE;
	QImage img(S, S, QImage::Format_Grayscale8);
	img.fill(0);

	QPainter p(&img);
	p.setRenderHint(QPainter::TextAntialiasing, true);
	p.setFont(m_font);
	p.setPen(Qt::white);
	p.drawText(QRect(0, 0, S, S), Qt::AlignLeft | Qt::AlignTop, QString(ch));
	p.end();

	/* 墨迹边界 */
	int x0 = S, y0 = S, x1 = -1, y1 = -1;
	for (int y = 0; y < S; ++y)
	{
		const uchar *line = img.constScanLine(y);
		for (int x = 0; x < S; ++x)
		{
			if (line[x] > 0)
			{
				x0 = std::min(x0, x);
				y0 = std::min(y0, y);
				x1 = std::max(x1, x);
				y1 = std::max(y1, y);
			}
		}
	}

	int dx = 0, dy = 0;
	if (x1 >= x0 && y1 >= y0)
	{
		dx = (S - (x1 - x0 + 1)) / 2 - x0;
		dy = (S - (y1 - y0 + 1)) / 2 - y0;
	}

	QImage moved(S, S, QImage::Format_Grayscale8);
	moved.fill(0);
	QPainter p2(&moved);
	p2.drawImage(dx, dy, img);              /* 平移，超出部分自动裁掉 */
	p2.end();

	QVector<QVector<bool>> bits(S, QVector<bool>(S, false));
	for (int y = 0; y < S; ++y)
	{
		const uchar *line = moved.constScanLine(y);
		for (int x = 0; x < S; ++x)
		{
			bits[y][x] = (line[x] >= m_threshold);
		}
	}
	return bits;
}

/* 缓存可变，所以标记 mutable；对外接口仍然是 const */
QVector<QVector<bool>> FontGen::glyphBits(QChar ch) const
{
	QMap<QChar, QVector<QVector<bool>>>::const_iterator it = m_cache.constFind(ch);
	if (it != m_cache.constEnd())
	{
		return it.value();
	}
	QVector<QVector<bool>> bits = renderBits(ch);
	m_cache.insert(ch, bits);
	return bits;
}

/* 列行式 + 逆向(低位在前)：每列取 8 行，低位在上 */
QVector<quint8> FontGen::glyphBytes(QChar ch) const
{
	const QVector<QVector<bool>> bits = glyphBits(ch);
	QVector<quint8> out;
	out.reserve(BYTES_PER_GLYPH);

	for (int half = 0; half < 2; ++half)
	{
		for (int x = 0; x < SIZE; ++x)
		{
			quint8 b = 0;
			for (int i = 0; i < 8; ++i)
			{
				if (bits[half * 8 + i][x])
				{
					b = static_cast<quint8>(b | (1 << i));
				}
			}
			out.append(b);
		}
	}
	return out;
}

int FontGen::glyphCount(const QString &text)
{
	QSet<QChar> set;
	for (int i = 0; i < text.size(); ++i)
	{
		if (text.at(i).unicode() > 0x7F)         /* ASCII 走原字库，不进这个表 */
		{
			set.insert(text.at(i));
		}
	}
	return set.size();
}

qint64 FontGen::flashBytes(int count, bool withAsciiFont)
{
	/* 汉字表 count*32 字节；ASCII 8x16 字库固定 95*16 字节 */
	return static_cast<qint64>(count) * BYTES_PER_GLYPH + (withAsciiFont ? 95 * 16 : 0);
}

QString FontGen::generateHeader(const QString &text, QString *errorOut) const
{
	QChar *chars = nullptr;
	int n = 0;
	{
		QSet<QChar> set;
		QVector<QChar> list;
		for (int i = 0; i < text.size(); ++i)
		{
			const QChar c = text.at(i);
			if (c.unicode() > 0x7F && !set.contains(c))
			{
				set.insert(c);
				list.append(c);
			}
		}
		/* 按 UTF-8 编码排序，和 Python 脚本产出一致，方便 diff */
		std::sort(list.begin(), list.end(), [](const QChar &a, const QChar &b) {
			return QString(a).toUtf8() < QString(b).toUtf8();
		});
		n = list.size();
		chars = new QChar[n];
		for (int i = 0; i < n; ++i)
		{
			chars[i] = list.at(i);
		}
	}

	if (n == 0)
	{
		delete[] chars;
		if (errorOut)
		{
			*errorOut = QStringLiteral("没有可生成的汉字（ASCII 字符走原有字库，不需要取模）");
		}
		return QString();
	}

	QString out;
	out += QStringLiteral(
		"#ifndef __OLED_CFONT_H\n"
		"#define __OLED_CFONT_H\n"
		"\n"
		"/*==============================================================================\n"
		" *  汉字字模库  16x16 点阵，每个汉字 32 字节\n"
		" *\n"
		" *  取模格式（与 OLED.c 的 OLED_ShowChinese 对应）：\n"
		" *      阴码 / 列行式 / 逆向(低位在前) / C51格式\n"
		" *      一个汉字 = 上半屏 16 字节 + 下半屏 16 字节\n"
		" *\n"
		" *  每行的排布：前 3 字节是该字的 UTF-8 编码，后面 32 字节是点阵。\n"
		" *  这里全部写成十六进制字节，汉字只放在注释里——因为把字符串字面量\n"
		" *  (\"\\xNN\\xNN\\xNN\") 和字节混在同一个初始化列表里，从第二行起编译器会\n"
		" *  把字符串当成指针值，报 initializer element is not computable at load time。\n"
		" *\n"
		" *  注意：本文件必须是 UTF-8 编码保存（Keil 里 Edit -> Configuration ->\n"
		" *        Encoding 选 UTF-8）。\n"
		" *============================================================================*/\n"
		"\n"
		"/* 汉字码表：每行 35 字节 = UTF-8 三字节编码 + 16x16 点阵(32 字节) */\n"
		"const uint8_t OLED_CF16x16[][35] =\n"
		"{\n");

	for (int i = 0; i < n; ++i)
	{
		const QVector<quint8> data = glyphBytes(chars[i]);

		/* 前 3 字节是 UTF-8 编码，后面 32 字节是点阵 */
		QVector<quint8> vals;
		const QByteArray utf8 = QString(chars[i]).toUtf8();
		for (int k = 0; k < utf8.size(); ++k)
		{
			vals.append(static_cast<quint8>(utf8.at(k)));
		}
		vals += data;

		QString hex;
		for (int k = 0; k < vals.size(); ++k)
		{
			if (k)
			{
				hex += QLatin1Char(',');
			}
			/* 注意：不能对 "0x%1" 整串调 toUpper()，那会把前缀里的 x 也变成 X，
			 * 数据会写成 0XE4 这种形式——C++ 能编译，但和字库约定的大小写不一致，
			 * 自检也会因为 startsWith("0x") 匹配不上而误报。
			 * 这里只把两位数值转成大写。 */
			const quint8 v = vals.at(k);
			const char digits[] = "0123456789ABCDEF";
			hex += QLatin1Char('0');
			hex += QLatin1Char('x');
			hex += QLatin1Char(digits[(v >> 4) & 0x0F]);
			hex += QLatin1Char(digits[v & 0x0F]);
		}

		out += QStringLiteral("\t/* %1 */\n\t%2,\n").arg(QString(chars[i]), hex);
	}

	out += QStringLiteral(
		"};\n"
		"\n"
		"/* 汉字个数 */\n"
		"#define OLED_CF16x16_NUM   (sizeof(OLED_CF16x16) / sizeof(OLED_CF16x16[0]))\n"
		"\n"
		"#endif\n");

	delete[] chars;
	return out;
}

/*
 * 导入：把字库文件里的汉字读回来，拼成字符串，供用户继续编辑。
 * 只认字模数据行，也就是以 0x 开头的那种。
 *
 * 注意：字库每一行是 35 个字节 —— 前 3 个是该字的 UTF-8 编码，后面 32 个是点阵，
 * 所以取这一行的前 3 个字节就能还原出这个汉字。
 *
 * 历史原因：早期版本写成 {"\xNN\xNN\xNN",0x..}，那种写法编译器过不去
 * （字符串和字节混在一个初始化列表里），已经全部改成纯字节表。
 * 这里顺带保留对旧格式的兼容，免得遇到老文件读不出来。
 */
QString FontGen::parseHeader(const QString &headerContent)
{
	QString result;
	QSet<QChar> seen;
	const QStringList lines = headerContent.split(QLatin1Char('\n'));

	for (int i = 0; i < lines.size(); ++i)
	{
		const QString line = lines.at(i).trimmed();

		QByteArray code;

		if (line.startsWith(QLatin1String("0x")))
		{
			/* 新格式：纯十六进制字节表，取前 3 个字节 */
			static const QRegularExpression re(QStringLiteral("0x([0-9A-Fa-f]{2})"));
			QRegularExpressionMatchIterator it = re.globalMatch(line);
			while (it.hasNext() && code.size() < 3)
			{
				code.append(static_cast<char>(it.next().captured(1).toInt(nullptr, 16)));
			}
		}
		else if (line.startsWith(QLatin1String("{\"\\x")))
		{
			/* 旧格式：{"\xE4\xB8\x8A",0x..} */
			for (int k = 0; k + 3 < line.size() && code.size() < 3; ++k)
			{
				if (line.at(k) == QLatin1Char('\\') && line.at(k + 1) == QLatin1Char('x'))
				{
					bool ok = false;
					const int v = line.mid(k + 2, 2).toInt(&ok, 16);
					if (ok)
					{
						code.append(static_cast<char>(v));
					}
				}
			}
		}
		else
		{
			continue;
		}

		if (code.size() != 3)
		{
			continue;
		}

		const QString ch = QString::fromUtf8(code);
		if (ch.isEmpty())
		{
			continue;
		}
		if (!seen.contains(ch.at(0)))
		{
			seen.insert(ch.at(0));
			result += ch.at(0);
		}
	}
	return result;
}
