/* TrueTypeFont.cpp
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

	GLuint texI = 0;
}



Font::~Font()
{
	TTF_CloseFont(font);  // TODO: all fonts use the same file, should only close it once...
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
	// widthEllipses = WidthRawString("...");
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

	// Bind.
	glUseProgram(shader->Object());
	glBindVertexArray(vao);

	// Reallocate buffer for texture id texI.
	if(texI)
		glDeleteTextures(1, &texI);

	texI = shader->Uniform("tex");
	glGenTextures(1, &texI);

	// Upload the new texture "image".
	GLint texture = 0;
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// ----------------------------------------------------------------------------------------
	SDL_Color sdlColor(255, 255, 255, 0);

#define USE_BLENDED
#ifdef USE_BLENDED
	// Note: returns ARGB, sorted out in the shader;
	SDL_Surface *surface = TTF_RenderUTF8_Blended(font, str.c_str(), sdlColor);  // ARGB, 32-bit/pixel
#else
	// Note: returns a single channel.
	// SDL_Surface *surface = TTF_RenderUTF8_Shaded(font, text.GetText().c_str(), sdlColor, bgColor);  // A, 8-bit/pixel
	SDL_Color bgColor(0, 0, 0, 0);
	SDL_Surface *surface = TTF_RenderUTF8_Shaded(font, text.GetText().c_str(), sdlColor, bgColor);  // A, 8-bit/pixel
#endif

	if(surface == nullptr)
	{
		Logger::LogError(string("Attempt to create surface resulted in TTF_GetError:") + TTF_GetError());
		return;
	}

	int columns = surface->pitch / surface->format->BytesPerPixel;
	// convert 8 bpp up to 32bpp like the current shaders expect
#ifdef USE_BLENDED
	// DebugPrint2(surface->pixels, surface->h, columns);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, columns, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
#else
	int count = surface->h * columns;
	auto *pixels = static_cast<unsigned int *>(malloc(count * sizeof(unsigned int)));
	if(pixels == nullptr)
	{
		Logger::LogError("malloc failed");
		return;
	}

	for(int y = 0; y < count; y += 4)
	{
		unsigned int b = static_cast<unsigned int *>(surface->pixels)[y/4];
		pixels[y + 3] = 0x00000000 | (0xFF000000 & (b << (0 * 8)));
		pixels[y + 2] = 0x00000000 | (0xFF000000 & (b << (1 * 8)));
		pixels[y + 1] = 0x00000000 | (0xFF000000 & (b << (2 * 8)));
		pixels[y + 0] = 0x00000000 | (0xFF000000 & (b << (3 * 8)));
	}

	// DebugPrint(pixels, surface->h, columns);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, columns, surface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	// ----------------------------------------------------------------------------------------
	free(pixels);
#endif

	GLfloat scale[2] = {2.f / Screen::Width(), -2.f / Screen::Height()};
	glUniform2fv(shader->Uniform("scale"), 1, scale);

	GLfloat sizeV[2] = {static_cast<float>(columns),
						static_cast<float>(surface->h)};
	SDL_FreeSurface(surface);
	glUniform2fv(shader->Uniform("size"), 1, sizeV);

	float position[2]{static_cast<float>(x), static_cast<float>(y - 1)};
	// float position[2]{static_cast<float>(x + 2.), static_cast<float>(y - 1)};
	glUniform2fv(shader->Uniform("position"), 1, position);
	// glUniform4fv(shader->Uniform("color"), 1, Color::Multiply(1.3, color).Get());
	glUniform4fv(shader->Uniform("color"), 1, color.Get());

	// Draw the rectangle of triangles.
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// Clean up.
	glBindVertexArray(0);
	// glBindTexture(GL_TEXTURE_2D, 0);

	// Unuse the shader program
	glUseProgram(0);
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



void Font::ShowUnderlines(bool show) noexcept
{
	//TODO: showUnderlines = show || Preferences::Has("Always underline shortcuts");
}



// // int Font::Glyph(char c, bool isAfterSpace) noexcept
// // {
// // 	// Curly quotes.
// // 	if(c == '\'' && isAfterSpace)
// // 		return 96;
// // 	if(c == '"' && isAfterSpace)
// // 		return 97;
// //
// // 	return max(0, min(GLYPHS - 3, c - 32));
// // }
// //
// //
// // void Font::LoadTexture(ImageBuffer &image)
// // {
// // 	glGenTextures(1, &texture);
// // 	glBindTexture(GL_TEXTURE_2D, texture);
// //
// // 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// // 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// // 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
// // 	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
// //
// // 	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, image.Width(), image.Height(), 0,
// // 		GL_RGBA, GL_UNSIGNED_BYTE, image.Pixels());
// // }
// //
// //
// // void Font::CalculateAdvances(ImageBuffer &image)
// // {
// // 	// Get the format and size of the surface.
// // 	int width = image.Width() / GLYPHS;
// // 	height = image.Height();
// // 	unsigned mask = 0xFF000000;
// // 	unsigned half = 0xC0000000;
// // 	int pitch = image.Width();
// //
// // 	// advance[previous * GLYPHS + next] is the x advance for each glyph pair.
// // 	// There is no advance if the previous value is 0, i.e. we are at the very
// // 	// start of a string.
// // 	memset(advance, 0, GLYPHS * sizeof(advance[0]));
// // 	for(int previous = 1; previous < GLYPHS; ++previous)
// // 		for(int next = 0; next < GLYPHS; ++next)
// // 		{
// // 			int maxD = 0;
// // 			int glyphWidth = 0;
// // 			uint32_t *begin = image.Pixels();
// // 			for(int y = 0; y < height; ++y)
// // 			{
// // 				// Find the last non-empty pixel in the previous glyph.
// // 				uint32_t *pend = begin + previous * width;
// // 				uint32_t *pit = pend + width;
// // 				while(pit != pend && (*--pit & mask) < half)
// // 				{
// // 				}
// // 				int distance = (pit - pend) + 1;
// // 				glyphWidth = max(distance, glyphWidth);
// //
// // 				// Special case: if "next" is zero (i.e. end of line of text),
// // 				// calculate the full width of this character. Otherwise:
// // 				if(next)
// // 				{
// // 					// Find the first non-empty pixel in this glyph.
// // 					uint32_t *nit = begin + next * width;
// // 					uint32_t *nend = nit + width;
// // 					while(nit != nend && (*nit++ & mask) < half)
// // 					{
// // 					}
// //
// // 					// How far apart do you want these glyphs drawn? If drawn at
// // 					// an advance of "width", there would be:
// // 					// pend + width - pit   <- pixels after the previous glyph.
// // 					// nit - (nend - width) <- pixels before the next glyph.
// // 					// So for zero kerning distance, you would want:
// // 					distance += 1 - (nit - (nend - width));
// // 				}
// // 				maxD = max(maxD, distance);
// //
// // 				// Update the pointer to point to the beginning of the next row.
// // 				begin += pitch;
// // 			}
// // 			// This is a fudge factor to avoid over-kerning, especially for the
// // 			// underscore and for glyph combinations like AV.
// // 			advance[previous * GLYPHS + next] = max(maxD, glyphWidth - 4) / 2;
// // 		}
// //
// // 	// Set the space size based on the character width.
// // 	width /= 2;
// // 	height /= 2;
// // 	space = (width + 3) / 6 + 1;
// // }
// //
// //
// // void Font::SetUpShader(float glyphW, float glyphH)
// // {
// // 	glyphWidth = glyphW * .5f;
// // 	glyphHeight = glyphH * .5f;
// //
// // 	shader = GameData::Shaders().Get("font");
// // 	// Initialize the shared parameters only once
// // 	if(!vao)
// // 	{
// // 		glUseProgram(shader->Object());
// // 		glUniform1i(shader->Uniform("tex"), 0);
// // 		glUseProgram(0);
// //
// // 		// Create the VAO and VBO.
// // 		glGenVertexArrays(1, &vao);
// // 		glBindVertexArray(vao);
// //
// // 		glGenBuffers(1, &vbo);
// // 		glBindBuffer(GL_ARRAY_BUFFER, vbo);
// //
// // 		GLfloat vertices[] = {
// // 			0.f, 0.f, 0.f, 0.f,
// // 			0.f, 1.f, 0.f, 1.f,
// // 			1.f, 0.f, 1.f, 0.f,
// // 			1.f, 1.f, 1.f, 1.f
// // 		};
// // 		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
// //
// // 		// Connect the xy to the "vert" attribute of the vertex shader.
// // 		constexpr auto stride = 4 * sizeof(GLfloat);
// // 		glEnableVertexAttribArray(shader->Attrib("vert"));
// // 		glVertexAttribPointer(shader->Attrib("vert"), 2, GL_FLOAT, GL_FALSE, stride, nullptr);
// //
// // 		glEnableVertexAttribArray(shader->Attrib("corner"));
// // 		glVertexAttribPointer(shader->Attrib("corner"), 2, GL_FLOAT, GL_FALSE,
// // 			stride, reinterpret_cast<const GLvoid *>(2 * sizeof(GLfloat)));
// //
// // 		glBindBuffer(GL_ARRAY_BUFFER, 0);
// // 		glBindVertexArray(0);
// //
// // 		colorI = shader->Uniform("color");
// // 		scaleI = shader->Uniform("scale");
// // 		glyphSizeI = shader->Uniform("glyphSize");
// // 		glyphI = shader->Uniform("glyph");
// // 		aspectI = shader->Uniform("aspect");
// // 		positionI = shader->Uniform("position");
// // 	}
// //
// // 	// We must update the screen size next time we draw.
// // 	screenWidth = 0;
// // 	screenHeight = 0;
// // }



int Font::WidthRawString(const char *str) const noexcept
{
	// TTF_MeasureUTF8
	// TTF_SizeUTF8(TTF_Font *font, const char *text, int *w, int *h);
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
