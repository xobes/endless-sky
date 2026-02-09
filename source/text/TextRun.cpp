/* TextRun.cpp
Copyright (c) 2026 by xobes

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/
#include "TextRun.h"

#include "Utf8.h"

#include <fribidi.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <vector>

using namespace std;

vector<TextRun> GenerateDirectionalRuns(const string &text)
{
	vector<FriBidiChar> logical_unicode_str(text.size());
	FriBidiStrIndex len = fribidi_charset_to_unicode(
		FRIBIDI_CHAR_SET_UTF8,
		text.c_str(),
		text.size(),
		logical_unicode_str.data()
	);
	vector<FriBidiChar> visual_unicode_str(len);
	vector<FriBidiLevel> levels(len);
	FriBidiParType baseDirection = FRIBIDI_PAR_LTR;

	// Convert from logical order to visual order:
	fribidi_log2vis(logical_unicode_str.data(), len, &baseDirection, visual_unicode_str.data(),
		nullptr, nullptr, levels.data());

	vector<TextRun> runs;
	string currentRunText;

	FriBidiLevel currentLevel = levels[0];
	FriBidiLevel level;

	for(int i = 0; i < len; ++i)
	{
		level = levels[i];

		// If the font changed, save the previous run and start a new one
		if(level != currentLevel && !currentRunText.empty())
		{
			runs.push_back({currentRunText, 0, baseDirection, currentLevel});
			currentRunText.clear();
		}
		currentLevel = level;

		currentRunText.append(Utf8::UTF32ToUTF8(visual_unicode_str[i]));
	}

	if (!currentRunText.empty())
		runs.push_back({currentRunText, 0, baseDirection, currentLevel});
	return runs;
}



// This function will parse out the underlines and provide a set of underlines in their stead.
vector<TextRun> GenerateGlyphRuns(
	const string &text, const vector<TTF_Font *> &fontList, bool isRTL)
{
	vector<TextRun> runs;
	string currentRunText;
	std::vector<std::pair<double, int>> underlines;
	size_t currentFontIndex = 0;
	size_t newFontIndex = 0;

	size_t end = text.length();
	size_t pos = 0;
	double x = 0.;
	int h = 0;
	int w = 0;

	// If the first character is the UTF8 byte order mark (BOM), skip it.
	if(!Utf8::IsBOM(Utf8::DecodeCodePoint(text, pos)))
		pos = 0;

	bool underlineChar = false;
	while(pos < end)
	{
		size_t start = pos;

		// Note: pos skips to the next unicode code point after pos in utf8,
		// or is set string::npos when there are no more code points.
		char32_t codepoint = Utf8::DecodeCodePoint(text, pos);

		// Former behavior preserved: underlines are control characters.
		if(codepoint == '\t')
		{
			currentRunText.append("    ");
			continue;
		}
		if(codepoint == '_')
		{
			underlineChar = true;
			continue;
		}
		string utf8codepoint = text.substr(start, pos - start);

		// See which of our fonts/fallback fonts are needed to handle this codepoint:
		for(size_t i = 0; i < fontList.size(); ++i)
		{
			bool fontHasGlyph = TTF_GlyphIsProvided32(fontList[i], codepoint);
			if(fontHasGlyph)
			{
				newFontIndex = i;
				break;
			}
		}

		// If the font changed, save the previous run and start a new one
		if(newFontIndex != currentFontIndex && !currentRunText.empty())
		{
			runs.push_back({currentRunText, currentFontIndex});
			currentRunText.clear();
			underlines.clear();
		}

		currentFontIndex = newFontIndex;

		// Append the raw UTF-8 bytes for this codepoint to the current run
		currentRunText.append(text, start, pos - start);

		// Calculate the width of the this codepoint so that we can draw a proper underline
		TTF_SizeUTF8(fontList[newFontIndex], utf8codepoint.c_str(), &w, &h);
		if(underlineChar)
		{
			underlines.emplace_back(make_pair(x, w));
			underlineChar = false;
		}
		x += isRTL ? w : -w;
	}

	if(!currentRunText.empty())
		runs.push_back({currentRunText, currentFontIndex, 0, 0, underlines});

	return runs;
}
