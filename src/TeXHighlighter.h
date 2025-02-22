/*
	This is part of TeXworks, an environment for working with TeX documents
	Copyright (C) 2007-2020  Jonathan Kew, Stefan Löffler, Charlie Sharpsteen

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	For links to further information, or to contact the authors,
	see <http://www.tug.org/texworks/>.
*/

#ifndef TEX_HIGHLIGHTER_H
#define TEX_HIGHLIGHTER_H

#include "document/SpellChecker.h"
#include <vector>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QTimer>

namespace Tw {
namespace Document {

class TeXDocument;

} // namespace Document
} // namespace Tw

// This class implements a non-blocking syntax highlighter that is a rewrite/
// replacement of QSyntaxHighlighter. It queues all highlight requests and
// processes them in small chunks that take no longer than MAX_TIME_MSECS
// before returning control to the main event loop to keep the UI responsive.
// Inspired by http://enki-editor.org/2014/08/22/Syntax_highlighting.html
class NonblockingSyntaxHighlighter : public QObject
{
	Q_OBJECT

public:
	static constexpr int MAX_TIME_MSECS = 5;
	static constexpr int IDLE_DELAY_TIME = 40;
	NonblockingSyntaxHighlighter(QTextDocument& doc)
	    : QObject(&doc), _processingPending(false) {
	    connect(document(), &QTextDocument::contentsChange, this, &NonblockingSyntaxHighlighter::maybeRehighlightText);
	    rehighlight();
    }
	~NonblockingSyntaxHighlighter() override {
	    disconnect(document());
	    QObject::setParent(nullptr);
	}
    NonblockingSyntaxHighlighter(NonblockingSyntaxHighlighter&& rhs) {
        swap(rhs);
    }
    NonblockingSyntaxHighlighter& operator=(NonblockingSyntaxHighlighter&& rhs) {
        swap(rhs);
        return *this;
    }
	QTextDocument* document() const { return static_cast<QTextDocument*>(QObject::parent()); }
public slots:
	void rehighlight();
	void rehighlightBlock(const QTextBlock & block);

protected:
	virtual void highlightBlock(const QString & text) = 0;
	void setFormat(const int start, const int count, const QTextCharFormat & format);
	QTextBlock currentBlock() const { return _currentBlock; }
	int currentBlockState() const { return _currentBlock.userState(); }
	int previousBlockState() const { return _currentBlock.previous().userState(); }
    // use nextBlockToHighlight()
	[[deprecated]] bool hasBlocksToHighlight() const noexcept { return !_highlightRanges.empty(); }
	const QTextBlock nextBlockToHighlight() const;
	void pushHighlightBlock(const QTextBlock & block);
	void pushHighlightRange(const int from, const int to);
	void popHighlightRange(const int from, const int to);
	void blockHighlighted(const QTextBlock & block);
	void pushDirtyRange(const QTextBlock & block) { pushDirtyRange(block.position(), block.position() + block.length()); }
	void pushDirtyRange(const int from, const int length);
	void markDirtyContent();
	void sanitizeHighlightRanges();
	void swap(NonblockingSyntaxHighlighter& rhs) {
        std::swap(_processingPending, rhs._processingPending);
        std::swap(_highlightRanges, rhs._highlightRanges);
        std::swap(_dirtyRanges, rhs._dirtyRanges);
        auto block = _currentBlock; // QTextBlock is not cannot be move c/able
        _currentBlock = rhs._currentBlock;
        rhs._currentBlock = block;
        std::swap(_currentFormatRanges, _currentFormatRanges);
	}

private slots:
	void maybeRehighlightText(int position, int charsRemoved, int charsAdded);
	void process();
	void processWhenIdle();

private:
	bool _processingPending;

	struct range {
		int from, to; // character ranges
	};
	std::vector<range> _highlightRanges;
	std::vector<range> _dirtyRanges;

	QTextBlock _currentBlock;
	QVector<QTextLayout::FormatRange> _currentFormatRanges;
};

class TeXHighlighter : public NonblockingSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit TeXHighlighter(Tw::Document::TeXDocument& parent);
	void setActiveIndex(int index);

	void setSpellChecker(Tw::Document::SpellChecker::Dictionary * dictionary);
	Tw::Document::SpellChecker::Dictionary * getSpellChecker() const { return _dictionary; }

	QString getSyntaxMode() const {
		return (highlightIndex >= 0 && highlightIndex < syntaxOptions().size())
				? syntaxOptions().at(highlightIndex) : QString();
	}

	static QStringList syntaxOptions();

protected:
	void highlightBlock(const QString &text) override;

	void spellCheckRange(const QString &text, int index, int limit, const QTextCharFormat &spellFormat);

private:
	static void loadPatterns();

	struct HighlightingRule {
		QRegularExpression pattern;
		QTextCharFormat format;
		QTextCharFormat	spellFormat;
		bool spellCheck;
	};
	typedef QList<HighlightingRule> HighlightingRules;
	struct HighlightingSpec {
		QString				name;
		HighlightingRules	rules;
	};
	static QList<HighlightingSpec> *syntaxRules;

	QTextCharFormat spellFormat;

	struct TagPattern {
		QRegularExpression pattern;
		unsigned int level;
	};
	static QList<TagPattern> *tagPatterns;

	int highlightIndex;
	bool isTagging;

	Tw::Document::SpellChecker::Dictionary * _dictionary;

	Tw::Document::TeXDocument * texDoc;
};

#endif
