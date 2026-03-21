/* TextRun.h
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

#pragma once

#include <SDL2/SDL_ttf.h>

#include <string>
#include <utility>
#include <vector>


struct TextRun {
    std::string text;
    size_t fontIndex = 0;
    bool isRTL = false;
    std::vector<std::pair<double, int>> underlines;
    int width = 0;

    TextRun(std::string t, size_t f, bool rtl, const std::vector<std::pair<double, int>> &u)
        : text(std::move(t)), fontIndex(f), isRTL(rtl), underlines(u) {}
};

std::vector<TextRun> GenerateDirectionalRuns(const std::string &text);
std::vector<TextRun> GenerateGlyphRuns(const std::string &text, const std::vector<TTF_Font *> &fontList, bool isRTL,
    bool measureUnderlines = false);
