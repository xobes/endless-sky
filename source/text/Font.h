/* Font.h
Copyright (c) 2014-2020 by Michael Zahniser

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

#include "../opengl.h"

#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>

class Color;
class DisplayText;
class Point;



// Class for drawing text in OpenGL. Each font is based on a single image with
// glyphs for each character in ASCII order (not counting control characters).
// The kerning between characters is automatically adjusted to look good. At the
// moment only plain ASCII characters are supported, not Unicode.
class Font {
	class TextureHandle {
	public:
		static constexpr GLuint INVALID_TEXTURE = static_cast<GLuint>(-1);
		static const TextureHandle &Invalid()
		{
			static const TextureHandle invalid(INVALID_TEXTURE);
			return invalid;
		}

		TextureHandle() : texID(INVALID_TEXTURE), width(0), height(0) {}
		explicit TextureHandle(const GLuint _texID, int _width = 0, int _height = 0)
			: texID(_texID), width(_width), height(_height) {}
		~TextureHandle() {
			if(texID != INVALID_TEXTURE)
				glDeleteTextures(1, &texID);
		}

		TextureHandle(const TextureHandle &other) = delete;
		TextureHandle(TextureHandle &&other) noexcept
		{
			this->texID = other.texID;
			this->width = other.width;
			this->height = other.height;
			other.texID = INVALID_TEXTURE;
		}
		TextureHandle &operator=(const TextureHandle &other) = delete;
		TextureHandle &operator=(TextureHandle &&other) noexcept
		{
			this->texID = other.texID;
			this->width = other.width;
			this->height = other.height;
			other.texID = INVALID_TEXTURE;
			return *this;
		}

		[[nodiscard]] GLuint GetTexture() const { return texID; }
		int GetWidth() const { return width; }
		int GetHeight() const { return height; }

	private:
		GLuint texID;
		int width;
		int height;
	};


public:
	Font() noexcept = default;
 	~Font();

	void Load(const std::filesystem::path &fontPath, double size);

	// Draw a text string, subject to the given layout and truncation strategy.
	void Draw(const DisplayText &text, const Point &point, const Color &color) const;
	// Draw the given text string, e.g. post-formatting (or without regard to formatting).
	void Draw(const std::string &str, const Point &point, const Color &color) const;
	// Special use of DrawAliased only used for drawing planet labels.
	void DrawAliased(const std::string &str, double x, double y, const Color &color) const;

	// Determine the string's width, without considering formatting.
	int Width(const std::string &str) const;
	// Get the width of the text while accounting for the desired layout and truncation strategy.
	int FormattedWidth(const DisplayText &text) const;
	int Height() const noexcept;
	int Space() const noexcept;

	static void ShowUnderlines(bool show) noexcept;

private:
	[[nodiscard]] const TextureHandle &GetTextureForText(const std::string &str, int fontIndex) const;

	void MarkTexturesUnused() const noexcept;
	void ClearUnusedTextures() const;

	void Init();
	int WidthRawString(const char *str) const noexcept;
	int WidthRawString(DisplayText &text) const noexcept;
	std::string TruncateText(const DisplayText &text, int &width) const;
	std::string TruncateBack(const std::string &str, int &width) const;
	std::string TruncateFront(const std::string &str, int &width) const;
	std::string TruncateMiddle(const std::string &str, int &width) const;
	std::string TruncateEndsOrMiddle(const std::string &str, int &width,
		std::function<std::string(const std::string &, int)> getResultString) const;
	void DrawAliased(DisplayText &text, double x, double y, const Color &color) const;
	void RenderString(const std::string &str, int fontIndex, double x, double y, const Color &color) const;
	void DrawInlineSprites(const DisplayText &text, const Color &color) const;


private:
	const Shader *shader;
	int fontSize = 0;
	int height = 0;
	int space;

	mutable GLfloat scale[2]{0.f, 0.f};
	std::vector<TTF_Font *> fontList;
	mutable std::map<std::pair<std::string, int>, TextureHandle> textureCache;
	mutable std::map<std::pair<std::string, int>, bool> textureUsedThisFrame;

	friend class FontSet;
};
