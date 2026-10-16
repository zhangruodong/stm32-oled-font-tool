#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSizePolicy>
#include <QSlider>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringConverter>
#include <QTextStream>
#include <QVBoxLayout>

#include "fontgen.h"
#include "fontpreview.h"

/* 诊断用：读进程令牌，判断是否被 AppContainer / 低完整性限制 */
#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shlobj.h>
#endif

/*
 * OLED 汉字取模工具
 *
 * 关键约束：生成的字模必须和工程里 Hardware/OLED.c 的
 * OLED_ShowChinese() 对得上（列行式 / 逆向 / 低位在前 / 每字 32 字节）。
 * 编码逻辑在 fontgen.cpp，那份算法和 gen_oled_hz.py 是等价的，
 * 改任何一边都要同步改另一边，否则字会花屏。
 */

/*
 * 找本工具自己的 src 目录（里面有 install-cfont.cmd 等脚本）。
 * exe 在 oledfonttool\build\ 下，src 在 oledfonttool\src\，所以向上找。
 * 之所以不写死 "../src"：工具被拷到别处时仍然能找到。
 */
static QString findSrcDir()
{
	QDir dir(QCoreApplication::applicationDirPath());
	for (int up = 0; up <= 5; ++up)
	{
		const QString candidate = dir.absoluteFilePath(QStringLiteral("src"));
		if (QFile::exists(QDir(candidate).absoluteFilePath(QStringLiteral("install-cfont.cmd"))))
		{
			return candidate;
		}
		if (!dir.cdUp())
		{
			break;
		}
	}
	return QString();
}

/* 判断 s 的第 i 位开始是不是 "0x"/"0X" + 两个十六进制数字（也就是一个字节的字面量） */
static bool isHexPair(const QString &s, int i)
{
	if (i + 3 >= s.size())            return false;
	if (s.at(i) != QLatin1Char('0'))  return false;
	if (s.at(i + 1) != QLatin1Char('x') && s.at(i + 1) != QLatin1Char('X')) return false;

	for (int k = i + 2; k <= i + 3; ++k)
	{
		const QChar c = s.at(k);
		const bool hex = (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
		                 (c >= QLatin1Char('a') && c <= QLatin1Char('f')) ||
		                 (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
		if (!hex)
		{
			return false;
		}
	}
	return true;
}

static const char *kQss =
	/* 注意：这里不写全局 color。
	 * 写了之后 QMessageBox 会继承深色文字，而它在深色主题下是深色背景，
	 * 结果弹窗文字完全看不见（踩过这个坑）。颜色一律写进具体控件。 */
	"QWidget { font-family: 'Microsoft YaHei UI','Microsoft YaHei',sans-serif; font-size: 12px; }"
	"QMainWindow, QSplitter { background: #FFFFFF; }"
	"#SidePanel { background: #FAFBFC; border-right: 1px solid #E4E7EB; }"
	"#Title { color: #1F2328; font-size: 16px; font-weight: 600; }"
	"#SubTitle, #Hint { color: #6B7280; font-size: 11px; }"
	"#Count { color: #1F6FEB; font-size: 12px; font-weight: 600; }"
	"#Warn { color: #B45309; background: #FEF3C7; border: 1px solid #FDE68A;"
	"        border-radius: 4px; padding: 4px 6px; font-size: 11px; }"
	"#PathLabel { color: #6B7280; font-size: 11px; }"
	"QLabel { color: #1F2328; }"
	"QCheckBox { color: #1F2328; }"
	"QGroupBox { color: #374151; border: 1px solid #E4E7EB; border-radius: 6px; margin-top: 14px; padding: 10px 8px 8px 8px; background: #FFFFFF; }"
	"QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: #374151; font-weight: 600; }"
	"QPlainTextEdit, QComboBox, QLineEdit { color: #1F2328; border: 1px solid #D7DBE0; border-radius: 4px; padding: 4px 6px; background: #FFFFFF; }"
	"QPlainTextEdit:focus, QComboBox:focus, QLineEdit:focus { border-color: #3B82F6; }"
	"QPushButton { color: #1F2328; border: 1px solid #D7DBE0; border-radius: 4px; padding: 6px 12px; background: #FFFFFF; }"
	"QPushButton:hover { background: #F3F4F6; }"
	"QPushButton#Primary { background: #3B82F6; border-color: #3B82F6; color: #FFFFFF; font-weight: 600; }"
	"QPushButton#Primary:hover { background: #2F6FE0; }"
	"QSlider::groove:horizontal { height: 4px; background: #E4E7EB; border-radius: 2px; }"
	"QSlider::handle:horizontal { width: 14px; margin: -6px 0; background: #3B82F6; border-radius: 7px; }"
	/* 弹窗显式配色，别依赖系统主题 */
	"QMessageBox { background: #FFFFFF; }"
	"QMessageBox QLabel { color: #1F2328; min-width: 320px; }";

class MainWindow : public QMainWindow
{
public:
	MainWindow();

private:
	void refreshPreview();
	void exportHeader();
	void importHeader();

	void buildUi();
	void updateStatus();
	void checkFontExists();
	QString defaultHeaderPath() const;

	FontGen m_gen;
	QPlainTextEdit *m_text;
	QComboBox *m_fontBox;
	QLabel *m_fontWarn;
	QSlider *m_threshold;
	QLabel *m_thresholdValue;
	QCheckBox *m_asciiFont;
	FontPreview *m_preview;
	QLabel *m_status;
	QLabel *m_countLabel;
	QLabel *m_hint;
	QLabel *m_pathLabel;
	QCheckBox *m_askEveryTime;      /* 勾上则每次导出都弹对话框选位置 */
	QString m_headerPath;
};

MainWindow::MainWindow()
	: m_gen(QFont(QStringLiteral("SimSun"), 16), 110)
{
	buildUi();
	refreshPreview();
}

void MainWindow::buildUi()
{
	setWindowTitle(QStringLiteral("OLED 汉字取模工具"));
	resize(1060, 660);

	QWidget *side = new QWidget(this);
	side->setObjectName(QStringLiteral("SidePanel"));
	/* 不要用 setFixedWidth：固定宽度会让分隔线拖不动。
	 * 给个最小宽度让内容不挤，剩下的交给 QSplitter —— 用户可以自由拖动。 */
	side->setMinimumWidth(280);
	side->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

	QVBoxLayout *sideLayout = new QVBoxLayout(side);
	sideLayout->setContentsMargins(16, 16, 16, 12);
	sideLayout->setSpacing(10);

	QLabel *title = new QLabel(QStringLiteral("OLED 汉字取模工具"), side);
	title->setObjectName(QStringLiteral("Title"));
	QLabel *subTitle = new QLabel(QStringLiteral("16×16 点阵 · 列行式 · 低位在前"), side);
	subTitle->setObjectName(QStringLiteral("SubTitle"));
	sideLayout->addWidget(title);
	sideLayout->addWidget(subTitle);

	/* ---- 取模设置 ---- */
	QGroupBox *boxSet = new QGroupBox(QStringLiteral("取模设置"), side);
	QFormLayout *form = new QFormLayout(boxSet);
	form->setLabelAlignment(Qt::AlignLeft);
	form->setSpacing(8);

	m_fontBox = new QComboBox(boxSet);
	const QStringList families = QFontDatabase::families();
	const QStringList prefer = { QStringLiteral("SimSun"), QStringLiteral("宋体"),
	                             QStringLiteral("SimHei"), QStringLiteral("黑体"),
	                             QStringLiteral("Microsoft YaHei"), QStringLiteral("微软雅黑") };
	for (int i = 0; i < prefer.size(); ++i)
	{
		if (families.contains(prefer.at(i)))
		{
			m_fontBox->addItem(prefer.at(i));
		}
	}
	for (int i = 0; i < families.size(); ++i)
	{
		if (m_fontBox->findText(families.at(i)) < 0)
		{
			m_fontBox->addItem(families.at(i));
		}
	}
	form->addRow(QStringLiteral("字体"), m_fontBox);

	/* 字体名不存在时 Qt 会静默回退，字形就变了，必须提醒 */
	m_fontWarn = new QLabel(boxSet);
	m_fontWarn->setObjectName(QStringLiteral("Warn"));
	m_fontWarn->setWordWrap(true);
	m_fontWarn->setVisible(false);
	form->addRow(QString(), m_fontWarn);


	QWidget *thrRow = new QWidget(boxSet);
	QHBoxLayout *thrLayout = new QHBoxLayout(thrRow);
	thrLayout->setContentsMargins(0, 0, 0, 0);
	thrLayout->setSpacing(6);
	m_threshold = new QSlider(Qt::Horizontal, thrRow);
	m_threshold->setRange(40, 200);
	m_threshold->setValue(110);
	m_thresholdValue = new QLabel(QStringLiteral("110"), thrRow);
	m_thresholdValue->setFixedWidth(28);
	thrLayout->addWidget(m_threshold);
	thrLayout->addWidget(m_thresholdValue);
	form->addRow(QStringLiteral("阈值"), thrRow);

	sideLayout->addWidget(boxSet);

	/* ---- 显示内容 ---- */
	QGroupBox *boxText = new QGroupBox(QStringLiteral("① 要显示的文字（在这里加字）"), side);
	QVBoxLayout *textLayout = new QVBoxLayout(boxText);
	m_text = new QPlainTextEdit(boxText);
	m_text->setPlaceholderText(QStringLiteral("直接打字，或点下面的「导入已有字库」"));
	m_text->setPlainText(QStringLiteral("嵌入式开发网中文测试你好世界温度湿度"));
	m_text->setFixedHeight(110);
	textLayout->addWidget(m_text);

	/* 字数就地显示在输入框下面，改一个字立刻能看到 */
	m_countLabel = new QLabel(boxText);
	m_countLabel->setObjectName(QStringLiteral("Count"));
	textLayout->addWidget(m_countLabel);

	m_hint = new QLabel(boxText);
	m_hint->setObjectName(QStringLiteral("Hint"));
	m_hint->setWordWrap(true);
	m_hint->setText(QStringLiteral("· 直接改上面的文字就能加字，右边实时预览\n"
	                               "· 「导入已有字库」= 读回 OLED_CFont.h，与当前文字合并，不会覆盖\n"
	                               "· 字母数字走原有 ASCII 字库，不用取模\n"
	                               "· 字库没有的字会显示成两格空白，不会花屏"));
	textLayout->addWidget(m_hint);
	sideLayout->addWidget(boxText);

	/* ---- 导出 ---- */
	QGroupBox *boxOut = new QGroupBox(QStringLiteral("导出"), side);
	QVBoxLayout *outLayout = new QVBoxLayout(boxOut);

	m_asciiFont = new QCheckBox(QStringLiteral("统计时包含 ASCII 字库(1520B)"), boxOut);
	m_asciiFont->setChecked(true);
	outLayout->addWidget(m_asciiFont);

	m_pathLabel = new QLabel(boxOut);
	m_pathLabel->setObjectName(QStringLiteral("PathLabel"));
	m_pathLabel->setWordWrap(true);
	outLayout->addWidget(m_pathLabel);

	/* 默认不勾：正常情况下一键导出，不打扰。
	 * 勾上后每次导出都弹对话框，方便导出到别处。 */
	m_askEveryTime = new QCheckBox(QStringLiteral("每次导出都询问保存位置"), boxOut);
	m_askEveryTime->setChecked(false);
	outLayout->addWidget(m_askEveryTime);

	QPushButton *exportBtn = new QPushButton(QStringLiteral("导出 OLED_CFont.h"), boxOut);
	exportBtn->setObjectName(QStringLiteral("Primary"));
	QPushButton *importBtn = new QPushButton(QStringLiteral("导入已有字库"), boxOut);
	QPushButton *saveAsBtn = new QPushButton(QStringLiteral("另存为…"), boxOut);

	outLayout->addWidget(exportBtn);
	QHBoxLayout *btnRow = new QHBoxLayout();
	btnRow->addWidget(importBtn);
	btnRow->addWidget(saveAsBtn);
	outLayout->addLayout(btnRow);

	sideLayout->addWidget(boxOut);
	sideLayout->addStretch(1);

	m_status = new QLabel(side);
	m_status->setObjectName(QStringLiteral("Hint"));
	/* 必须开换行：状态文字比较长（例如"已导入 17 个汉字并与原文合并（原有 17 个，现共 17 个）"），
	 * 不开的话超出面板宽度的部分会被直接切掉，看起来像显示不全。 */
	m_status->setWordWrap(true);
	m_status->setMinimumWidth(1);      /* 允许被压缩，配合上面的换行才不会撑开面板 */
	sideLayout->addWidget(m_status);

	/* ---- 右侧预览：套一层滚动区域，字多了 / 窗口缩小了都能滚 ---- */
	m_preview = new FontPreview(this);
	m_preview->setGenerator(&m_gen);

	QScrollArea *scroll = new QScrollArea(this);
	scroll->setWidget(m_preview);
	scroll->setWidgetResizable(true);          /* 宽度跟随窗口，高度由 heightForWidth 决定 */
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setFrameShape(QFrame::NoFrame);

	QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(side);
	splitter->addWidget(scroll);
	splitter->setStretchFactor(0, 0);
	splitter->setStretchFactor(1, 1);
	splitter->setChildrenCollapsible(false);
	splitter->setSizes(QList<int>() << 320 << 740);
	setCentralWidget(splitter);

	m_headerPath = defaultHeaderPath();
	if (m_headerPath.isEmpty())
	{
		m_pathLabel->setText(QStringLiteral(
			"未自动找到 Keil 工程目录。\n"
			"导出时会先弹出对话框让你选保存位置。"));
	}
	else
	{
		m_pathLabel->setText(QStringLiteral("默认导出到：\n%1").arg(m_headerPath));
	}

	connect(m_text, &QPlainTextEdit::textChanged, this, &MainWindow::refreshPreview);
	connect(m_fontBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
	        this, &MainWindow::refreshPreview);
	connect(m_threshold, &QSlider::valueChanged, this, [this](int v) {
		m_thresholdValue->setText(QString::number(v));
		refreshPreview();
	});
	connect(m_asciiFont, &QCheckBox::toggled, this, &MainWindow::updateStatus);
	connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportHeader);
	connect(importBtn, &QPushButton::clicked, this, &MainWindow::importHeader);
	connect(saveAsBtn, &QPushButton::clicked, this, [this]() {
		const QString path = QFileDialog::getSaveFileName(
			this, QStringLiteral("另存为"), m_headerPath,
			QStringLiteral("C 头文件 (*.h)"));
		if (!path.isEmpty())
		{
			m_headerPath = path;
			exportHeader();
		}
	});
}

/*
 * 找工程里的 OLED_CFont.h。
 *
 * 这个工具可以放在任何地方（单独 clone 下来、拷到别的盘都行），
 * 所以这里按优先级依次尝试，都不写死某个人的机器路径：
 *   1. 从 exe 所在目录开始向上找，看哪一层下面有 MyFreeRTOS_C8T6\Hardware
 *   2. 环境变量 OLED_FONT_PROJECT 指定的工程根目录（可选，方便把工具放别处）
 *   3. 都找不到 → 返回空字符串，导出时弹对话框让用户自己选
 *
 * 找不到是正常情况：导入/导出都有文件对话框，用户可以手动选任意路径。
 */
QString MainWindow::defaultHeaderPath() const
{
	/* 1) 从 exe 所在目录向上找。
	 *    exe 在 <工程>\oledfonttool\build\ 下，向上两级就是工程根目录；
	 *    如果工具被单独拷走，这里会一直找到盘根，自然失败。 */
	QDir dir(QCoreApplication::applicationDirPath());
	for (int up = 0; up <= 5; ++up)
	{
		const QString candidate = dir.absoluteFilePath(QStringLiteral("MyFreeRTOS_C8T6/Hardware"));
		if (QDir(candidate).exists())
		{
			return QDir(candidate).absoluteFilePath(QStringLiteral("OLED_CFont.h"));
		}
		if (!dir.cdUp())
		{
			break;
		}
	}

	/* 2) 可选：允许用环境变量指定工程位置。
	 *    适合“工具装在 C 盘、工程在 D 盘”这种情况，用户自己设一个就够了。 */
	const QByteArray envRoot = qgetenv("OLED_FONT_PROJECT");
	if (!envRoot.isEmpty())
	{
		const QString candidate = QDir(QString::fromLocal8Bit(envRoot))
			.absoluteFilePath(QStringLiteral("MyFreeRTOS_C8T6/Hardware"));
		if (QDir(candidate).exists())
		{
			return QDir(candidate).absoluteFilePath(QStringLiteral("OLED_CFont.h"));
		}
	}

	/* 3) 找不到就返回空字符串 —— 表示"猜不准"。
	 *    调用方（构造函数和导出）会据此弹对话框让用户自己选，
	 *    而不是默默写到某个用户不知道的地方。 */
	return QString();
}

void MainWindow::refreshPreview()
{
	QFont f(m_fontBox->currentText());
	f.setPixelSize(FontGen::SIZE);
	m_gen.setFont(f);
	m_gen.setThreshold(m_threshold->value());

	checkFontExists();
	m_preview->setText(m_text->toPlainText());
	updateStatus();
}

/* QFont 在字体名不存在时会静默回退到默认字体，那样字形就不是你要的那个了 */
void MainWindow::checkFontExists()
{
	const QFont f(m_fontBox->currentText());
	QFontInfo info(f);
	const bool exact = info.family().compare(m_fontBox->currentText(), Qt::CaseInsensitive) == 0;
	if (exact)
	{
		m_fontWarn->setVisible(false);
	}
	else
	{
		m_fontWarn->setText(QStringLiteral("⚠ 找不到「%1」，实际用的是「%2」，字形会不一样")
		                    .arg(m_fontBox->currentText(), info.family()));
		m_fontWarn->setVisible(true);
	}
}

void MainWindow::updateStatus()
{
	const int n = m_preview->glyphCount();
	const qint64 bytes = FontGen::flashBytes(n, m_asciiFont->isChecked());

	m_countLabel->setText(QStringLiteral("当前 %1 个汉字 · 字库 %2 字节（%3 KB）")
	                      .arg(n)
	                      .arg(bytes)
	                      .arg(QString::number(bytes / 1024.0, 'f', 1)));

	m_status->setText(QStringLiteral("汉字 %1 个 · 字库 %2 字节（%3 KB）\nSTM32F103C8T6 共 64KB Flash")
	                  .arg(n)
	                  .arg(bytes)
	                  .arg(QString::number(bytes / 1024.0, 'f', 1)));
}

void MainWindow::exportHeader()
{
	const QString srcText = m_text->toPlainText();
	QString err;
	const QString content = m_gen.generateHeader(srcText, &err);

	/* 诊断：每次导出都把中间结果落盘。
	 * 生成失败时靠猜没有意义，直接留下证据（文字、生成内容、逐行统计）。 */
	{
		const QString diagPath = QDir(QCoreApplication::applicationDirPath())
			.absoluteFilePath(QStringLiteral("export-debug.txt"));
		QFile dbg(diagPath);
		if (dbg.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			QTextStream ts(&dbg);
			ts.setEncoding(QStringConverter::Utf8);
			ts << "=== 输入文字 ===\n";
			ts << "字符数: " << srcText.size() << "\n";
			ts << "内容: [" << srcText << "]\n\n";
			ts << "=== 汉字统计 ===\n";
			ts << "FontGen::glyphCount: " << FontGen::glyphCount(srcText) << "\n";
			ts << "content 是否为空: " << (content.isEmpty() ? "是" : "否") << "\n";
			ts << "content 长度: " << content.size() << "\n\n";
			ts << "=== 生成内容逐行统计 ===\n";

			const QStringList lines = content.split(QLatin1Char('\n'));
			ts << "总行数: " << lines.size() << "\n";
			for (int i = 0; i < lines.size(); ++i)
			{
				const QString tr = lines.at(i).trimmed();
				ts << QStringLiteral("[%1] 前6字符=<%2> 以0x开头=%3 长度=%4\n")
					.arg(i)
					.arg(tr.left(6))
					.arg(tr.startsWith(QLatin1String("0x")) ? "是" : "否")
					.arg(tr.size());
			}
			ts << "\n=== 原始内容(前 2000 字符) ===\n";
			ts << content.left(2000) << "\n";
			dbg.close();
		}
	}

	if (content.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("无法导出"),
			err + QStringLiteral("\n\n(诊断信息已写入 export-debug.txt)"));
		return;
	}

	/* 写出前先自检：这个格式一旦写错，贴进 Keil 就是编译报错或者花屏，
	 * 而且肉眼很难看出来（我们已经在字库格式上栽过一次）。
	 *
	 * 这里不依赖 QString::count(QRegularExpression) 的语义——不同 Qt 版本对
	 * "数匹配个数"还是"数匹配到的字符总数"有歧义，曾经导致误报。
	 * 改成手动逐段扫："0x" + 两位十六进制 算一个字节。 */
	{
		const QStringList lines = content.split(QLatin1Char('\n'));
		int rows = 0;
		for (int i = 0; i < lines.size(); ++i)
		{
			const QString line = lines.at(i).trimmed();
			/* 注意：只以长度 + 前两字符判断，大小写都认（0x / 0X）。
			 * 曾经因为生成端写成 0X 而这里严格比 "0x"，导致误报"没有数据行"。 */
			if (line.size() < 5 ||
			    line.at(0) != QLatin1Char('0') ||
			    (line.at(1) != QLatin1Char('x') && line.at(1) != QLatin1Char('X')))
			{
				continue;
			}
			++rows;

			int bytes = 0;      /* 数 "0xNN" 的个数 */
			for (int k = 0; k + 3 < line.size(); )
			{
				if (isHexPair(line, k))
				{
					++bytes;
					k += 4;
				}
				else
				{
					++k;
				}
			}

			if (bytes != 35)
			{
				QString preview = line;
				if (preview.size() > 80)
				{
					preview = preview.left(80) + QStringLiteral(" ...");
				}
				QMessageBox::critical(this, QStringLiteral("导出前自检失败"),
					QStringLiteral("第 %1 行数出 %2 个字节，期望 35 个。\n\n该行开头：\n%3")
					.arg(i + 1).arg(bytes).arg(preview));
				return;
			}
		}
		if (rows == 0)
		{
			QMessageBox::critical(this, QStringLiteral("导出前自检失败"),
				QStringLiteral("没有生成任何字模数据行。\n"
				               "当前文字里似乎没有汉字（只有 ASCII 字符？）\n\n"
				               "诊断信息已写入程序目录下的 export-debug.txt，请把它发给我。"));
			return;
		}
	}

	/* 什么时候先问用户保存位置：
	 *   1. 用户勾了"每次导出都询问"
	 *   2. 没自动找到工程目录（猜不准，绝不能默默写到别处）
	 * 否则直接写到自动推断出来的路径，保持一键导出的便利。 */
	const bool needAsk = m_askEveryTime->isChecked() || m_headerPath.isEmpty();
	if (needAsk)
	{
		QString start = m_headerPath;
		if (start.isEmpty())
		{
			/* 用"文档"目录当对话框的起始位置，比空路径友好 */
			start = QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
				.absoluteFilePath(QStringLiteral("OLED_CFont.h"));
		}
		const QString picked = QFileDialog::getSaveFileName(
			this,
			m_headerPath.isEmpty() ? QStringLiteral("导出到（未自动找到工程目录，请选择位置）")
			                       : QStringLiteral("导出到"),
			start,
			QStringLiteral("C 头文件 (*.h)"));
		if (picked.isEmpty())
		{
			return;     /* 用户取消了 */
		}
		m_headerPath = picked;
		m_pathLabel->setText(QStringLiteral("导出到：\n%1").arg(m_headerPath));
	}

	QFile file(m_headerPath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		/* 先判断到底是什么原因：Windows 把"文件被占用"和"权限不足"
		 * 都报成"拒绝访问"，光看错误信息分不出来。
		 * 这里的办法是同目录下建一个临时文件试试：
		 *   能建 → 目录可写，问题在那个文件本身（多半被 Keil/编辑器打开了）
		 *   不能建 → 目录不可写（权限问题） */
		const QString dirPath = QFileInfo(m_headerPath).absolutePath();
		const QString probePath = QDir(dirPath).absoluteFilePath(QStringLiteral("write-probe.tmp"));
		bool dirWritable = false;
		{
			QFile probe(probePath);
			if (probe.open(QIODevice::WriteOnly))
			{
				dirWritable = true;
				probe.close();
				probe.remove();
			}
		}

		/* 这个程序进程可能被限制了写入范围（实测：连系统临时目录都写不进去，
		 * 只有程序自己的目录可以）。所以失败后不再弹"另存为"，
		 * 而是自动重试到确定可写的位置，再告诉用户怎么搬进工程。 */
		const QString srcDir = findSrcDir();
		QStringList fallbacks;
		fallbacks << QDir(QCoreApplication::applicationDirPath())
		             .absoluteFilePath(QStringLiteral("OLED_CFont.h"));
		if (!srcDir.isEmpty())
		{
			fallbacks << QDir(srcDir).absoluteFilePath(QStringLiteral("OLED_CFont.h"));
		}

		for (int i = 0; i < fallbacks.size(); ++i)
		{
			QFile alt(fallbacks.at(i));
			if (!alt.open(QIODevice::WriteOnly | QIODevice::Text))
			{
				continue;
			}
			QTextStream altTs(&alt);
			altTs.setEncoding(QStringConverter::Utf8);
			altTs << content;
			const QString savedTo = fallbacks.at(i);
			alt.close();

			m_headerPath = savedTo;
			m_pathLabel->setText(QStringLiteral("导出到：\n%1").arg(savedTo));

			const QString installer = srcDir.isEmpty()
				? QStringLiteral("install-cfont.cmd")
				: QDir(srcDir).absoluteFilePath(QStringLiteral("install-cfont.cmd"));

			QMessageBox::information(this, QStringLiteral("导出成功（需要手动搬运）"),
				QStringLiteral("程序没有权限直接写入工程目录，已改为导出到：\n%1\n\n"
				               "下一步：双击\n%2\n"
				               "把字库搬进工程的 Hardware 目录（会自动备份旧文件）。\n\n"
				               "汉字 %3 个，%4 字节。")
				.arg(savedTo)
				.arg(installer)
				.arg(m_preview->glyphCount())
				.arg(FontGen::flashBytes(m_preview->glyphCount(), false)));
			return;
		}

		QString reason;
		if (dirWritable)
		{
			reason = QStringLiteral(
				"目录本身可以写，但目标文件打不开。\n"
				"最常见的原因是它正被别的程序占用——\n"
				"请关掉 Keil / VSCode / 记事本里打开的 OLED_CFont.h 再试。");
		}
		else
		{
			reason = QStringLiteral(
				"这个程序进程不能写那个目录。\n"
				"（实测连系统临时目录都写不进去，只有程序自己的目录可以，\n"
				"  属于进程被限制了写入范围，不是文件权限问题。）");
		}

		/* 诊断：把程序自己的运行环境和逐项测试结果落盘。
		 * 用户用 cmd 测试同一个目录是成功的，只有本程序失败，
		 * 所以必须看清楚程序进程到底在什么环境下跑。 */
		{
			const QString diagPath = QDir(QCoreApplication::applicationDirPath())
				.absoluteFilePath(QStringLiteral("export-debug.txt"));
			QFile dbg(diagPath);
			if (dbg.open(QIODevice::WriteOnly | QIODevice::Text))
			{
				QTextStream ts(&dbg);
				ts.setEncoding(QStringConverter::Utf8);
				ts << "=== 导出失败诊断 ===\n";
				ts << "时间: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
				ts << "--- 运行环境 ---\n";
				ts << "用户名: " << qgetenv("USERNAME") << "\n";
				ts << "计算机名: " << qgetenv("COMPUTERNAME") << "\n";
				ts << "工作目录: " << QDir::currentPath() << "\n";
				ts << "程序目录: " << QCoreApplication::applicationDirPath() << "\n";
				ts << "TEMP 环境变量: " << qgetenv("TEMP") << "\n";
				ts << "TMP  环境变量: " << qgetenv("TMP") << "\n";
				ts << "Qt 认为的临时目录: " << QDir::tempPath() << "\n";
				ts << "是否以管理员运行: "
				   << (IsUserAnAdmin() ? "是" : "否") << "\n";

				/* 打印进程令牌里的组和完整性级别：
				 * AppContainer / 受限令牌会在这里露出特征（低完整性、AppContainer SID）。 */
				HANDLE hTok = nullptr;
				if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hTok))
				{
					DWORD len = 0;
					GetTokenInformation(hTok, TokenGroups, nullptr, 0, &len);
					if (len)
					{
						QByteArray buf(static_cast<int>(len), 0);
						if (GetTokenInformation(hTok, TokenGroups, buf.data(), len, &len))
						{
							TOKEN_GROUPS *groups = reinterpret_cast<TOKEN_GROUPS *>(buf.data());
							ts << "令牌组数: " << static_cast<int>(groups->GroupCount) << "\n";
							for (DWORD g = 0; g < groups->GroupCount; ++g)
							{
								WCHAR name[256] = {0};
								WCHAR domain[256] = {0};
								DWORD nLen = 256, dLen = 256;
								SID_NAME_USE use;
								if (LookupAccountSidW(nullptr, groups->Groups[g].Sid,
								                      name, &nLen, domain, &dLen, &use))
								{
									ts << "  " << QString::fromWCharArray(domain) << "\\"
									   << QString::fromWCharArray(name)
									   << (groups->Groups[g].Attributes & SE_GROUP_USE_FOR_DENY_ONLY
									           ? "  [仅用于拒绝]" : "") << "\n";
								}
							}
						}
					}

					DWORD ilen = 0;
					GetTokenInformation(hTok, TokenIntegrityLevel, nullptr, 0, &ilen);
					if (ilen)
					{
						QByteArray ibuf(static_cast<int>(ilen), 0);
						if (GetTokenInformation(hTok, TokenIntegrityLevel, ibuf.data(), ilen, &ilen))
						{
							TOKEN_MANDATORY_LABEL *label =
								reinterpret_cast<TOKEN_MANDATORY_LABEL *>(ibuf.data());
							const DWORD rid = *GetSidSubAuthority(
								label->Label.Sid,
								*GetSidSubAuthorityCount(label->Label.Sid) - 1);
							ts << "完整性级别 RID: " << static_cast<int>(rid) << "  (";
							if (rid < SECURITY_MANDATORY_MEDIUM_RID)      ts << "低于 Medium —— 受限！";
							else if (rid == SECURITY_MANDATORY_MEDIUM_RID) ts << "Medium（正常）";
							else                                           ts << "高于 Medium";
							ts << ")\n";
						}
					}
					CloseHandle(hTok);
				}
				ts << "\n";
				ts << "--- 目标 ---\n";
				ts << "m_headerPath: [" << m_headerPath << "]\n";
				ts << "absolutePath: [" << dirPath << "]\n";
				ts << "目录存在: " << (QDir(dirPath).exists() ? "是" : "否") << "\n";
				ts << "文件存在: " << (QFile::exists(m_headerPath) ? "是" : "否") << "\n";
				ts << "文件大小: " << QFileInfo(m_headerPath).size() << "\n\n";
				ts << "--- 逐项测试 ---\n";
				ts << "1) QFile 打开目标文件(WriteOnly): " << file.errorString() << "\n";

				QFile t1(probePath);
				ts << "2) QFile 在目标目录建 probe.tmp: "
				   << (t1.open(QIODevice::WriteOnly) ? "成功" : ("失败 - " + t1.errorString())) << "\n";
				if (t1.isOpen()) { t1.close(); t1.remove(); }

				QFile t2(QDir::temp().absoluteFilePath(QStringLiteral("oled-probe.tmp")));
				ts << "3) QFile 在系统临时目录建文件: "
				   << (t2.open(QIODevice::WriteOnly) ? "成功" : ("失败 - " + t2.errorString())) << "\n";
				if (t2.isOpen()) { t2.close(); t2.remove(); }

				QFile t3(QDir(QCoreApplication::applicationDirPath())
					.absoluteFilePath(QStringLiteral("oled-probe.tmp")));
				ts << "4) QFile 在程序目录建文件: "
				   << (t3.open(QIODevice::WriteOnly) ? "成功" : ("失败 - " + t3.errorString())) << "\n";
				if (t3.isOpen()) { t3.close(); t3.remove(); }

				QFile t4(QDir(QDir::currentPath()).absoluteFilePath(QStringLiteral("oled-probe.tmp")));
				ts << "5) QFile 在工作目录建文件: "
				   << (t4.open(QIODevice::WriteOnly) ? "成功" : ("失败 - " + t4.errorString())) << "\n";
				if (t4.isOpen()) { t4.close(); t4.remove(); }

				dbg.close();
			}
		}

		QMessageBox::critical(this, QStringLiteral("导出失败"),
			QStringLiteral("无法写入：\n%1\n\n%2\n\n系统报错：%3\n\n"
			               "诊断已写入 export-debug.txt（在程序目录下）。")
			.arg(m_headerPath, reason, file.errorString()));
		return;
	}

	QTextStream ts(&file);
	ts.setEncoding(QStringConverter::Utf8);        /* 必须是 UTF-8，Keil 才不会读花 */
	ts << content;
	file.close();

	/* 写完把路径显示同步一下，避免提示里写着 A、标签上还显示 B */
	m_pathLabel->setText(QStringLiteral("导出到：\n%1").arg(m_headerPath));

	QMessageBox::information(this, QStringLiteral("导出成功"),
	                         QStringLiteral("已写入：\n%1\n\n汉字 %2 个，%3 字节\n在 Keil 里 Rebuild 即可生效。")
	                         .arg(m_headerPath)
	                         .arg(m_preview->glyphCount())
	                         .arg(FontGen::flashBytes(m_preview->glyphCount(), false)));
}

void MainWindow::importHeader()
{
	const QString path = QFileDialog::getOpenFileName(
		this, QStringLiteral("选择已有的 OLED_CFont.h"), m_headerPath,
		QStringLiteral("C 头文件 (*.h)"));
	if (path.isEmpty())
	{
		return;
	}

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::critical(this, QStringLiteral("读取失败"), file.errorString());
		return;
	}
	const QString text = FontGen::parseHeader(QString::fromUtf8(file.readAll()));
	file.close();

	if (text.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("没读到汉字"),
		                     QStringLiteral("这个文件里没有可识别的字模行。"));
		return;
	}

	/* 合并而不是替换：导入已有字库通常是为了"在它的基础上加字"，
	 * 直接替换会把原来几十个字全丢掉。 */
	const QString current = m_text->toPlainText();
	QString merged;
	QSet<QChar> seen;
	for (int pass = 0; pass < 2; ++pass)
	{
		const QString &src = (pass == 0) ? current : text;
		for (int i = 0; i < src.size(); ++i)
		{
			const QChar c = src.at(i);
			if (!seen.contains(c))
			{
				seen.insert(c);
				merged += c;
			}
		}
	}

	m_text->setPlainText(merged);
	m_pathLabel->setText(QStringLiteral("导出到：\n%1").arg(path));
	m_headerPath = path;

	const int before = FontGen::glyphCount(current);
	const int after = FontGen::glyphCount(merged);
	const int added = FontGen::glyphCount(text);

	if (after < before)
	{
		/* 合并逻辑理论上只增不减；真出现减少说明用户清空了输入框之类，
		 * 提醒一句，别把好不容易攒的字丢了 */
		QMessageBox::information(this, QStringLiteral("注意字数变化"),
		    QStringLiteral("合并后是 %1 个汉字，比原来的 %2 个少。\n"
		                   "如果这不是你要的，请先撤销（Ctrl+Z）。").arg(after).arg(before));
	}

	m_status->setText(QStringLiteral("已导入 %1 字，合并后共 %2 字")
	                  .arg(added)
	                  .arg(after));
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setApplicationName(QStringLiteral("OLED 汉字取模工具"));

	/* 强制用 Fusion 风格 + 浅色调色板。
	 *
	 * 原因是踩过坑：Windows 深色主题下，QMessageBox 会用深色背景，
	 * 而样式表里的深色文字在深色背景上完全看不见——弹窗看起来"只有标题、
	 * 没有内容"，实际是文字隐形了，白折腾好几轮。
	 * 固定成浅色以后，弹出框、菜单这些非自绘控件都不会再出现隐形文字。 */
	app.setStyle(QStringLiteral("Fusion"));

	QPalette pal;
	pal.setColor(QPalette::Window, QColor(0xF0, 0xF0, 0xF0));
	pal.setColor(QPalette::WindowText, QColor(0x1F, 0x23, 0x28));
	pal.setColor(QPalette::Base, Qt::white);
	pal.setColor(QPalette::AlternateBase, QColor(0xF7, 0xF8, 0xFA));
	pal.setColor(QPalette::Text, QColor(0x1F, 0x23, 0x28));
	pal.setColor(QPalette::Button, QColor(0xF0, 0xF0, 0xF0));
	pal.setColor(QPalette::ButtonText, QColor(0x1F, 0x23, 0x28));
	pal.setColor(QPalette::ToolTipBase, Qt::white);
	pal.setColor(QPalette::ToolTipText, QColor(0x1F, 0x23, 0x28));
	pal.setColor(QPalette::Highlight, QColor(0x3B, 0x82, 0xF6));
	pal.setColor(QPalette::HighlightedText, Qt::white);
	pal.setColor(QPalette::PlaceholderText, QColor(0x9A, 0xA0, 0xA6));
	app.setPalette(pal);

	/* 不要设置全局 color：那会让 QMessageBox 这类系统控件继承到深色文字，
	 * 在深色背景下看不见。 */
	app.setStyleSheet(QString::fromUtf8(kQss));

	MainWindow w;
	w.show();
	return app.exec();
}
