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

#include "../opengl.h"

#include <SDL_ttf.h>

#include <filesystem>
#include <functional>
#include <map>

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
	static void ShowUnderlines(bool show) noexcept;


public:
	Font() noexcept = default;
	~Font();

	void Load(const std::filesystem::path &fontPath, double size);

	// Draw a text string, subject to the given layout and truncation strategy.
	void Draw(const DisplayText &text, const Point &point, const Color &color) const;
	void DrawAliased(const DisplayText &text, double x, double y, const Color &color) const;
	// Draw the given text string, e.g. post-formatting (or without regard to formatting).
	void Draw(const std::string &str, const Point &point, const Color &color) const;
	void DrawAliased(const std::string &str, double x, double y, const Color &color) const;

	int Width(const std::string &str) const;
	int FormattedWidth(const DisplayText &text) const;
	int Height() const noexcept;
	int Space() const noexcept;

	[[nodiscard]] const TextureHandle &GetTextureForText(const std::string &str) const;
	void DrawAliased(const TextureHandle &texture, double x, double y, const Color &color) const;


private:
	void Init();
	int WidthRawString(const char *str) const noexcept;
	std::string TruncateText(const DisplayText &text, int &width) const;
	std::string TruncateBack(const std::string &str, int &width) const;
	std::string TruncateFront(const std::string &str, int &width) const;
	std::string TruncateMiddle(const std::string &str, int &width) const;
	std::string TruncateEndsOrMiddle(const std::string &str, int &width,
	std::function<std::string(const std::string &, int)> getResultString) const;

private:
	const Shader *shader;
	int fontSize;
	int height;
	int space;

	mutable GLfloat scale[2]{0.f, 0.f};
	TTF_Font *font;
	mutable std::map<std::string, TextureHandle> textureCache;
};
