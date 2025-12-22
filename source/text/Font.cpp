/* Font.cpp
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

#include "Font.h"

#include "../Color.h"
#include "DisplayText.h"
#include "../text/Format.h"
#include "../GameData.h"
#include "../Logger.h"
#include "../Point.h"
#include "../Screen.h"

#include <SDL2/SDL_ttf.h>

using namespace std;

namespace {
	bool showUnderlines = false;

	/// Shared VAO and VBO quad (0,0) -> (1,1)
	GLuint vao = 0;
	GLuint vbo = 0;
}



Font::~Font()
{
}



void Font::Load(const filesystem::path &path, double size)
{
	Init();
	fontSize = size;
	font = TTF_OpenFont(path.c_str(), size);
	TTF_SetFontHinting(font, TTF_HINTING_MONO);
	if(size == 14)
		height = 16;
	else
		height = size;
	space = WidthRawString("-");
}



void Font::Draw(const DisplayText &text, const Point &point, const Color &color) const
{
	DrawAliased(text, round(point.X()), round(point.Y()), color);
}



void Font::DrawAliased(const DisplayText &text, double x, double y, const Color &color) const
{
	int width = -1;
	const string truncText = TruncateText(text, width);
	const auto &layout = text.GetLayout();
	if(width >= 0)
	{
		if(layout.align == Alignment::CENTER)
			x += (layout.width - width) / 2;
		else if(layout.align == Alignment::RIGHT)
			x += layout.width - width;
	}
	DrawAliased(truncText, x, y, color);
}



void Font::Draw(const string &str, const Point &point, const Color &color) const
{
	DrawAliased(str, round(point.X()), round(point.Y()), color);
}



void Font::DrawAliased(const string &str, double x, double y, const Color &color) const
{
	if(str.empty())
		return;

	DrawAliased(GetTextureForText(str), x, y, color);
}



void Font::Init()
{
	shader = GameData::Shaders().Get("ttfont");
	if(!shader->Object())
		throw std::runtime_error("Could not find ttfont shader!");

	// Initialize the shared parameters only once
	if(!vao)
	{
		glUseProgram(shader->Object());
		glUniform1i(shader->Uniform("tex"), 0);
		glUseProgram(0);

		// Generate the vertex data for drawing sprites.
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		GLfloat vertexData[] = {
			0.f, 0.f,
			1.f, 0.f,
			0.f, 1.f,
			1.f, 1.f
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

		GLuint vertI = shader->Attrib("vert");
		GLsizei stride = 2 * sizeof(float);
		glEnableVertexAttribArray(vertI);
		glVertexAttribPointer(vertI, 2, GL_FLOAT, GL_FALSE, stride, nullptr);

		// Unbind the VBO and VAO.
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
}



int Font::Width(const string &str) const
{
	return WidthRawString(str.c_str());
}



int Font::FormattedWidth(const DisplayText &text) const
{
	int width = -1;
	const string truncText = TruncateText(text, width);
	return width < 0 ? WidthRawString(truncText.c_str()) : width;
}



int Font::Height() const noexcept
{
	return height;
}



int Font::Space() const noexcept
{
	return space;
}



const Font::TextureHandle& Font::GetTextureForText(const string &str) const
{
	// Mark as used and return cached texture if available:
	auto it = textureCache.find(str);
	if(it != textureCache.end())
	{
		textureUsedThisFrame[str] = true;
		return it->second;
	}

	// Otherwise, create a texture for this text string (and cache it).
	GLuint texI;
	glGenTextures(1, &texI);
	glBindTexture(GL_TEXTURE_2D, texI);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SDL_Color sdlColor(255, 255, 255, 0);

	SDL_Surface *surface = TTF_RenderUTF8_Blended(font, str.c_str(), sdlColor);

	if(surface == nullptr)
	{
		Logger::Log(string("Attempt to create surface resulted in TTF_GetError:") + TTF_GetError(),
			Logger::Level::ERROR);
		glDeleteTextures(1, &texI);
		return TextureHandle::Invalid();
	}

	int columns = surface->pitch / surface->format->BytesPerPixel;
	int rows = surface->h;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, columns, rows, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, surface->pixels);

	SDL_FreeSurface(surface);
	textureUsedThisFrame[str] = true;
	return textureCache[str] = TextureHandle(texI, columns, rows);
}



void Font::DrawAliased(const TextureHandle &texture, double x, double y, const Color &color) const
{
	// Bind.
	glUseProgram(shader->Object());
	glBindVertexArray(vao);
	glBindTexture(GL_TEXTURE_2D, texture.GetTexture());

	GLfloat scale[2] = {2.f / Screen::Width(), -2.f / Screen::Height()};
	glUniform2fv(shader->Uniform("scale"), 1, scale);

	GLfloat sizeV[2] = {static_cast<float>(texture.GetWidth()), static_cast<float>(texture.GetHeight())};
	glUniform2fv(shader->Uniform("size"), 1, sizeV);

	float position[2]{static_cast<float>(x), static_cast<float>(y - 1)};
	glUniform2fv(shader->Uniform("position"), 1, position);
	glUniform4fv(shader->Uniform("color"), 1, color.Get());

	// Draw the rectangle of triangles.
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// Clean up.
	glBindVertexArray(0);
	glUseProgram(0);
}



void Font::MarkTexturesUnused() const noexcept
{
	for(auto &entry : textureUsedThisFrame)
		entry.second = false;
}



void Font::ClearUnusedTextures() const
{
	vector<string> toRemove;
	for(const auto &entry : textureUsedThisFrame)
		if(!entry.second)
			toRemove.push_back(entry.first);

	for(const auto &str : toRemove)
	{
		textureCache.erase(str);
		textureUsedThisFrame.erase(str);
	}
}



void Font::ShowUnderlines(bool show) noexcept
{
	// TODO: showUnderlines = show || Preferences::Has("Always underline shortcuts");
}



int Font::WidthRawString(const char *str) const noexcept
{
	int width = 0;
	int h = 0;
	TTF_SizeUTF8(font, str, &width, &h);

	return width;
}



// Param width will be set to the width of the return value, unless the layout width is negative.
string Font::TruncateText(const DisplayText &text, int &width) const
{
	width = -1;
	const auto &layout = text.GetLayout();
	const string &str = text.GetText();
	if(layout.width < 0 || (layout.align == Alignment::LEFT && layout.truncate == Truncate::NONE))
		return str;
	width = layout.width;
	switch(layout.truncate)
	{
		case Truncate::NONE:
			width = WidthRawString(str.c_str());
			return str;
		case Truncate::FRONT:
			return TruncateFront(str, width);
		case Truncate::MIDDLE:
			return TruncateMiddle(str, width);
		case Truncate::BACK:
		default:
			return TruncateBack(str, width);
	}
}



string Font::TruncateBack(const string &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const string &str, int charCount) {
			return str.substr(0, charCount) + "...";
		});
}



string Font::TruncateFront(const string &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const string &str, int charCount) {
			return "..." + str.substr(str.size() - charCount);
		});
}



string Font::TruncateMiddle(const string &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const string &str, int charCount) {
			return str.substr(0, (charCount + 1) / 2) + "..." + str.substr(str.size() - charCount / 2);
		});
}



string Font::TruncateEndsOrMiddle(const string &str, int &width,
	function<string(const string &, int)> getResultString) const
{
	int firstWidth = WidthRawString(str.c_str());
	if(firstWidth <= width)
	{
		width = firstWidth;
		return str;
	}

	int workingChars = 0;
	int workingWidth = 0;

	int low = 0, high = str.size() - 1;
	while(low <= high)
	{
		// Think "how many chars to take from both ends, omitting in the middle".
		int nextChars = (low + high) / 2;
		int nextWidth = WidthRawString(getResultString(str, nextChars).c_str());
		if(nextWidth <= width)
		{
			if(nextChars > workingChars)
			{
				workingChars = nextChars;
				workingWidth = nextWidth;
			}
			low = nextChars + (nextChars == low);
		} else
			high = nextChars - 1;
	}
	width = workingWidth;
	return getResultString(str, workingChars);
}
