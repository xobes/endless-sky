/* Font.cpp
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

#include "Font.h"

#include "Alignment.h"
#include "../Color.h"
#include "DisplayText.h"
#include "../shader/FillShader.h"
#include "../text/Format.h"
#include "../GameData.h"
#include "../image/ImageBuffer.h"
#include "../Logger.h"
#include "../Point.h"
#include "../Preferences.h"
#include "../Rectangle.h"
#include "../Screen.h"
#include "../image/Sprite.h"
#include "../shader/SpriteShader.h"
#include "../text/TextRun.h"
#include "Truncate.h"
#include "Utf8String.h"
#include "../ZipFile.h"

#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace std;

namespace {
	bool showUnderlines = false;

	/// Shared VAO and VBO quad (0,0) -> (1,1)
	GLuint vao = 0;
	GLuint vbo = 0;
}



void Font::Load(const filesystem::path &path, double size)
{
	// TODO: consider variable initialized with std::this_thread::get_id() to allow asserting that later
	//  calls to Font are on the same thread (for much clearer debugging when that happens not to be the
	//  case, as SDL_ttf fonts only exist/work on the thread they are initialized in)
	Init();
	string fontKey = path.string();
	if(ranges::find(loadedFonts, fontKey) == loadedFonts.end())
	{
		filesystem::path fontFileUnzipped = path;
		// string zipFilePath;
		string pathString = path.string();
		size_t zip = pathString.find(".zip", 0);
		if(zip != std::string::npos)
			fontFileUnzipped = ZipFile(pathString.substr(0, zip + 4)).ExtractTempFile(path);

		auto font = TTF_OpenFont(fontFileUnzipped.c_str(), size);
		if(!font)
		{
			Logger::Log("Unable to load font: " + fontFileUnzipped.string(), Logger::Level::WARNING);
			return;
		}
		TTF_SetFontHinting(font, TTF_HINTING_MONO);
		fontList.emplace_back(font);
		loadedFonts.emplace_back(fontKey);
		Logger::Log("Loaded font/size: " + fontFileUnzipped.string() + " size " +
			std::to_string(static_cast<int>(size)), Logger::Level::INFO);
	}
	if(!height)
	{
		fontSize = size;
		height = size + 2;
		space = WidthRawString("-");
	}
}



void Font::Draw(const DisplayText &text, const Point &point, const Color &color) const
{
	DisplayText copy(text.GetText(), text.GetLayout());
	DrawAliased(copy, round(point.X()), round(point.Y()), color);
}



void Font::Draw(const Utf8String &str, const Point &point, const Color &color) const
{
	DisplayText text(str, Layout(Alignment::LEFT));
	DrawAliased(text, round(point.X()), round(point.Y()), color);
}



void Font::DrawAliased(const Utf8String &str, double x, double y, const Color &color) const
{
	DisplayText text(str, Layout(Alignment::LEFT));
	DrawAliased(text, x, y, color);
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



int Font::Width(const Utf8String &str) const
{
	return WidthRawString(str.c_str());
}



int Font::FormattedWidth(const DisplayText &text) const
{
	int width = -1;
	const Utf8String truncText = TruncateText(text, width);
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



void Font::ShowUnderlines(bool show) noexcept
{
	showUnderlines = show || Preferences::Has("Always underline shortcuts");
}



const Font::TextureHandle &Font::GetTextureForText(const string &str, int fontIndex) const
{
	// Mark as used and return cached texture if available:
	auto texHandle = make_pair(str, fontIndex);
	auto it = textureCache.find(texHandle);
	if(it != textureCache.end())
	{
		textureUsedThisFrame[texHandle] = true;
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

	auto font = fontList[fontIndex];
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
	textureUsedThisFrame[texHandle] = true;
	return textureCache[texHandle] = TextureHandle(texI, columns, rows);
}



void Font::MarkTexturesUnused() const noexcept
{
	for(auto &entry : textureUsedThisFrame)
		entry.second = false;
}



void Font::ClearUnusedTextures() const
{
	vector<pair<string, int>> toRemove;
	for(const auto &[texHandle, used] : textureUsedThisFrame)
		if(!used)
			toRemove.push_back(texHandle);

	for(const auto &texHandle : toRemove)
	{
		textureCache.erase(texHandle);
		textureUsedThisFrame.erase(texHandle);
	}
}



void Font::MarkTextRunsUnused() const noexcept
{
	for(auto &entry : textRunsUsedThisFrame)
		entry.second = false;
}



void Font::ClearUnusedTextRuns() const
{
	vector<string> toRemove;
	for(const auto &[str, used] : textRunsUsedThisFrame)
		if(!used)
			toRemove.push_back(str);

	for(const auto &str : toRemove)
	{
		textRunsCache.erase(str);
		textRunsUsedThisFrame.erase(str);
	}
}



void Font::CachedTTFSizeUTF8(TTF_Font *font, const std::string &text, size_t fontIndex, int &w) const
{
	auto sizeKey = std::make_pair(text, fontIndex);
	auto sizeIt = sizeCache.find(sizeKey);
	if(sizeIt != sizeCache.end())
		w = sizeIt->second.first;
	else
	{
		int h;
		TTF_SizeUTF8(font, text.c_str(), &w, &h);
		sizeCache[sizeKey] = std::make_pair(w, h);
	}
	sizeUsedThisFrame[sizeKey] = true;
}



int Font::WidthRawString(const char *str) const noexcept
{
	DisplayText text(str, Alignment::LEFT);
	return WidthRawString(text);
}



int Font::WidthRawString(DisplayText &text) const noexcept
{
	text.UpdateSpriteReferences();

	if(text.GetText().empty())
		return 0;

	int width = 0;
	int spriteNum = 0;

	for(const TextRun &d : GenerateDirectionalRuns(text.GetText().to_string()))
	{
		for(const TextRun &r : GenerateGlyphRuns(d.text, fontList, d.isRTL))
		{
			// If this textRun is a placeholder for a SPRITE, save room for the sprite.
			if(r.text.c_str()[0] == DisplayText::SPRITE_PLACEHOLDER && r.text.length() == 1)
			{
				auto spriteData = text.inlineSprites[spriteNum++];
				width += std::get<0>(spriteData)->Width();
				continue;
			}

			int w;
			CachedTTFSizeUTF8(fontList[r.fontIndex], r.text, r.fontIndex, w);
			width += w;
		}
	}

	return width;
}



// Param width will be set to the width of the return value, unless the layout width is negative.
Utf8String Font::TruncateText(const DisplayText &text, int &width) const
{
	width = -1;
	const auto &layout = text.GetLayout();
	const Utf8String &str = text.GetText();
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



Utf8String Font::TruncateBack(const Utf8String &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const Utf8String &str, int charCount) {
			return str.substr(0, charCount) + "...";
		});
}



Utf8String Font::TruncateFront(const Utf8String &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const Utf8String &str, int charCount) {
			return Utf8String("...") + str.substr(str.size() - charCount);
		});
}



Utf8String Font::TruncateMiddle(const Utf8String &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const Utf8String &str, int charCount) {
			return str.substr(0, (charCount + 1) / 2) + "..." + str.substr(str.length() - charCount / 2);
		});
}



Utf8String Font::TruncateEndsOrMiddle(const Utf8String &str, int &width,
	const function<Utf8String(const Utf8String &, int)>& getResultString) const
{
	int firstWidth = WidthRawString(str.c_str());
	if(firstWidth <= width)
	{
		width = firstWidth;
		return str;
	}

	int workingChars = 0;
	int workingWidth = 0;

	int low = 0, high = str.length() - 1;
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
		}
		else
			high = nextChars - 1;
	}
	width = workingWidth;
	return getResultString(str, workingChars);
}



// FriBidi handles "flipping" the RTL words so they appear in the right place.
// HarfBuzz (inside SDL2_ttf) handles "connecting" the letters once it has a correctly ordered chunk.
void Font::DrawAliased(DisplayText &text, double x, double y, const Color &color) const
{
	text.UpdateSpriteReferences();
	int spriteNum = 0;

	auto font = fontList[0];
	int baseY = TTF_FontAscent(font);
	int offset = -0.5 * (TTF_FontHeight(font) - height);

	int w = 0;
	int width = -1;
	const DisplayText truncText(TruncateText(text, width), text.GetLayout());
	const auto &layout = text.GetLayout();
	if(width >= 0)
	{
		if(layout.align == Alignment::CENTER)
			x += (layout.width - width) / 2;
		else if(layout.align == Alignment::RIGHT)
			x += layout.width - width;
	}

	x -= 1.;

	string str = truncText.GetText().to_string();
	if(str.empty())
		return;

	// Check TextRuns cache
	auto it = textRunsCache.find(str);
	if(it == textRunsCache.end())
	{
		// Add a new value to the cache
		auto &cached = textRunsCache[str];
		for(const TextRun &d : GenerateDirectionalRuns(str))
		{
			auto glyphRuns = GenerateGlyphRuns(d.text, fontList, d.isRTL);
			cached.insert(cached.end(), glyphRuns.begin(), glyphRuns.end());
		}
		it = textRunsCache.find(str);
	}
	textRunsUsedThisFrame[str] = true;

	// Render text runs
	for(TextRun &r : it->second)
	{
		// if this textRun is a placeholder for a SPRITE, draw the sprite
		if(r.text.c_str()[0] == DisplayText::SPRITE_PLACEHOLDER && r.text.length() == 1)
		{
			auto spriteData = &text.inlineSprites[spriteNum++];
			double w = std::get<0>(*spriteData)->Width();
			// Set sprite center point.
			std::get<2>(*spriteData) = Point(x + .5 * w, y + .5 * height);
			x += w;
			continue;
		}

		auto font = fontList[r.fontIndex];
		// Note: fribidi alrady swapped LTR/RTL for us
		// TTF_SetFontDirection(font, r.isRTL ? TTF_DIRECTION_RTL: TTF_DIRECTION_LTR);

		if(r.width == 0)
			CachedTTFSizeUTF8(font, r.text, r.fontIndex, r.width);
		w = r.width;

		double dY = baseY - TTF_FontAscent(font);
		RenderString(r.text, r.fontIndex, x, y + offset + dY, color);

		x += w;

		if(showUnderlines)
			for(auto[ux, uw] : r.underlines)
				FillShader::Fill(Rectangle::FromCorner(
					{x + ux, y + offset + dY + height + 2}, {1. * uw, 1}), color);
	}

	// TODO: a sprite is just it's own portion of a textRun.
	DrawInlineSprites(text, color);
}



void Font::RenderString(const string &str, int fontIndex, double x, double y, const Color &color) const
{
	if(str.empty())
		return;

	const auto texture = &GetTextureForText(str, fontIndex);

	// Bind.
	glUseProgram(shader->Object());
	glBindVertexArray(vao);
	glBindTexture(GL_TEXTURE_2D, texture->GetTexture());
	GLfloat scale[2] = {2.f / Screen::Width(), -2.f / Screen::Height()};
	glUniform2fv(shader->Uniform("scale"), 1, scale);
	GLfloat sizeV[2] = {static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())};
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



void Font::DrawInlineSprites(const DisplayText &text, const Color &color) const
{
	for(const auto &[sprite, embossedStr, center] : text.inlineSprites)
	{
		// center += Point(0,20);
		SpriteShader::Draw(sprite, center);
		if(embossedStr != "")
		{
			Point textPoint = center + Point(-.5 * sprite->Width(), -.5 * height);
			DisplayText embossedText(embossedStr, Layout(sprite->Width(), Alignment::CENTER));
			DrawAliased(embossedText, textPoint.X(), textPoint.Y(), color);
		}
	}
}
