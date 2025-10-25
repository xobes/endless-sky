/* Font.h
Copyright (c) 2025 by xobes

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

#include "../shader/Shader.h"

#include <SDL_ttf.h>

#include <filesystem>

class Color;
class DisplayText;
class Point;




// Class for drawing text in OpenGL. Each font is based on a single image with
// glyphs for each character in ASCII order (not counting control characters).
// The kerning between characters is automatically adjusted to look good. At the
// moment only plain ASCII characters are supported, not Unicode.
class TrueTypeFont {
public:
	//Creates image from font string
	// bool loadFromRenderedText( std::string textureText, Color textColor );

	TrueTypeFont() noexcept = default;
	// explicit TrueTypeFont(int size);

	~TrueTypeFont();

	void Load(const std::filesystem::path &fontPath, double size);

	void Example() const;
	// void Draw(const DisplayText &text, const Point &point, const Color &color) const;
	void Draw(const DisplayText &text, const Point &point, const Color &color, double dx = 0., double dy=0.) const;


private:
	void Init();


private:
	const Shader *shader;
	TTF_Font *font;
	int fontSize;

	mutable GLfloat scale[2]{0.f, 0.f};
};
