#ifndef FONTGEN_H
#define FONTGEN_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QFont>

/*
 * 把汉字渲染成 16x16 点阵，并编码成 OLED 驱动要的字节格式。
 *
 * 编码格式（和同目录的 gen_oled_hz.py、生成的 OLED_CFont.h 完全一致）：
 *   阴码 / 列行式 / 逆向(低位在前)
 *   一个汉字 = 上半屏 16 字节 + 下半屏 16 字节 = 32 字节
 *
 * 这里的算法必须和 Python 脚本逐字节一致，改一边就要同步改另一边。
 */
class FontGen
{
public:
	static const int SIZE = 16;          // 点阵边长
	static const int BYTES_PER_GLYPH = 32;

	explicit FontGen(const QFont &font = QFont(), int threshold = 110);

	void setFont(const QFont &font);
	void setThreshold(int threshold);

	QFont font() const { return m_font; }
	int threshold() const { return m_threshold; }

	/* 取一个字的点阵；调过阈值/字体后会自动重算 */
	QVector<QVector<bool>> glyphBits(QChar ch) const;

	/* 一个字的 32 字节 */
	QVector<quint8> glyphBytes(QChar ch) const;

	/* 整个字符串的 C 头文件内容 */
	QString generateHeader(const QString &text, QString *errorOut = nullptr) const;

	/* 统计 */
	static int glyphCount(const QString &text);
	static qint64 flashBytes(int glyphCount, bool withAsciiFont = true);

	/* 从已有的头文件里把汉字编码读回来（导入用） */
	static QString parseHeader(const QString &headerContent);

private:
	QVector<QVector<bool>> renderBits(QChar ch) const;

	QFont m_font;
	int m_threshold;
	mutable QMap<QChar, QVector<QVector<bool>>> m_cache;   /* const 成员函数里也能写缓存 */
};

#endif /* FONTGEN_H */
