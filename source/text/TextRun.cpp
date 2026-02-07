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

#include <SDL2/SDL_ttf.h>

#include <string>
#include <vector>

std::vector<TextRun> GenerateRuns(const std::string &text, const std::vector<TTF_Font *> &fontList)
{
	std::vector<TextRun> runs;
	std::string currentRunText;
	size_t currentFontIndex = 0;
	size_t newFontIndex = 0;

	size_t end = text.length();
	size_t pos = 0;

	// If the first character is the UTF8 byte order mark (BOM), skip it.
	if(!Utf8::IsBOM(Utf8::DecodeCodePoint(text, pos)))
		pos = 0;

	while(pos < end)
	{
		size_t start = pos;

		// Note: pos skips to the next unicode code point after pos in utf8,
		// or is set string::npos when there are no more code points.
		char32_t codepoint = Utf8::DecodeCodePoint(text, pos);

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
		}

		currentFontIndex = newFontIndex;

		// Append the raw UTF-8 bytes for this codepoint to the current run
		currentRunText.append(text, start, pos - start);
	}

	if (!currentRunText.empty()) {
		runs.push_back({currentRunText, currentFontIndex});
	}
	return runs;
}
