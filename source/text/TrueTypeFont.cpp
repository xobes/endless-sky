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

#include "TrueTypeFont.h"

#include "../Color.h"
#include "DisplayText.h"
#include "../GameData.h"
#include "../Logger.h"
// #include "../image/ImageBuffer.h"
// #include "../image/ImageFileData.h"
#include "../Point.h"
#include "../Screen.h"

#include <SDL2/SDL_ttf.h>

using namespace std;

namespace {
	/// Shared VAO and VBO quad (0,0) -> (1,1)
	GLuint vao = 0;
	GLuint vbo = 0;

	GLuint texI = 0;

	void DebugPrint(unsigned int *pixels, int height, int columns)
	{
		char buf[9];
		string row;
		for(int y = 0; y < height; y++)
		{
			row = "";
			for(int x = 0; x < columns; x++)
			{
				int i = (y * columns) + x;
				int value = pixels[i];
				value >>= 24;
				value &= 0xFF;
				std::sprintf(buf, "%02X", value);
				if(value > 50)
					row += buf;
				else if(value)
					row += "..";
				else
					row += "  ";
			}
			Logger::LogError(row);
		}
	}

	void DebugPrint2(void *pixin, int height, int columns)
	{
		auto *pixels = static_cast<unsigned int *>(malloc(height * columns * sizeof(unsigned int)));
		if(pixels == nullptr)
		{
			Logger::LogError("malloc failed");
			return;
		}

		for(int y = 0; y < height * columns; y++)
		{
			unsigned int b = static_cast<unsigned int *>(pixin)[y];
			pixels[y] = 0xFF & b >> 24;
		}

		DebugPrint(pixels, height, columns);
		free(pixels);
	}
}


void TrueTypeFont::Load(const filesystem::path &path, double size)
{
	Init();
	fontSize = size;
	font = TTF_OpenFont(path.c_str(), size);
}
//
//
//
// TrueTypeFont::TrueTypeFont(int size)
// 	:font(TTF_OpenFont(fontPath.c_str(), size)
// {
// }



TrueTypeFont::~TrueTypeFont()
{
	TTF_CloseFont(font);
}



void TrueTypeFont::Draw(const DisplayText &text, const Point &point, const Color &color,
	double dx, double dy
	) const
{
// // void Font::DrawAliased(const string &str, double x, double y, const Color &color) const

	// Bind.
	glUseProgram(shader->Object());
	glBindVertexArray(vao);

	// Reallocate buffer for texture id texI. TODO: only delete/create when size changes?
	if(texI)
		glDeleteTextures(1, &texI);

	texI = shader->Uniform("tex");
	glGenTextures(1, &texI);
	Logger::LogError("texI = " + to_string(texI));

	// Upload the new texture "image".
	GLint texture = 0;
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// ----------------------------------------------------------------------------------------
	SDL_Color sdlColor(255, 255, 255, 255);

#define USE_BLENDED
#ifdef USE_BLENDED
	// Note: returns ARGB, sorted out in the shader;
	SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text.GetText().c_str(), sdlColor);  // ARGB, 32-bit/pixel
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

	float h = surface->h;
	SDL_FreeSurface(surface);
	// h = fontSize + 5; // h * h / static_cast<float>(fontSize);
	GLfloat sizeV[2] = {static_cast<float>(columns + dx), //surface->w),
						static_cast<float>(h + dy)};
	glUniform2fv(shader->Uniform("size"), 1, sizeV);

	float x = point.X();
	float y = point.Y();
	float position[2]{x, y};
	glUniform2fv(shader->Uniform("position"), 1, position);
	glUniform4fv(shader->Uniform("color"), 1, color.Get());
	// Logger::LogError(string("surface->w = ") + to_string(surface->w));
	// Logger::LogError(string("surface->h = ") + to_string(surface->h));
	// Logger::LogError(string("Screen::Width() = ") + to_string(Screen::Width()));
	// Logger::LogError(string("Screen::Height() = ") + to_string(Screen::Height()));
	// Logger::LogError(string("scale = ") + to_string(scale[0]) + ", " + to_string(scale[1]));
	// Logger::LogError(string("position = ") + to_string(position[0]) + ", " + to_string(position[1]));
	// Logger::LogError(string("color = ") + to_string(color.Get()[0]) + ", " + to_string(color.Get()[1]) + ", " +
	// 	to_string(color.Get()[2]) + ", " + to_string(color.Get()[3]));

	// // Activate texture id 0.
	// glActiveTexture(GL_TEXTURE0);

	// // Let the frag shader know we're using texture 0.
	// glUniform1i(texI, 0);

	// Draw the rectangle of triangles.
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// Clean up.
	glBindVertexArray(0);
	// glBindTexture(GL_TEXTURE_2D, 0);

	// Unuse the shader program
	glUseProgram(0);
}



void TrueTypeFont::Init()
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

		// GLfloat vertexData[] = {
		// 	-.5f, -.5f,
		// 	 .5f, -.5f,
		// 	-.5f,  .5f,
		// 	 .5f,  .5f
		// };
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


//
//
// bool TrueTypeFont::loadFromRenderedText(std::string textureText, Color textColor)
// {
// 	// TODO: TRY:
// 	// SDL_Surface * TTF_RenderUNICODE_Shaded(TTF_Font *font,
// 	// const Uint16 *text, SDL_Color fg, SDL_Color bg
// 	// );
// 	// TTF_RenderUTF8_Shaded
//
// 	//Get rid of preexisting texture
// 	// free();
// 	//
// 	// //Render text surface
// 	// SDL_Surface *textSurface = TTF_RenderText_Solid(gFont, textureText.c_str(), textColor);
// 	// if(textSurface == NULL)
// 	// {
// 	// 	printf("Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
// 	// } else
// 	// {
// 	// 	//Create texture from surface pixels
// 	// 	mTexture = SDL_CreateTextureFromSurface(gRenderer, textSurface);
// 	// 	if(mTexture == NULL)
// 	// 	{
// 	// 		printf("Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
// 	// 	} else
// 	// 	{
// 	// 		//Get image dimensions
// 	// 		mWidth = textSurface->w;
// 	// 		mHeight = textSurface->h;
// 	// 	}
// 	//
// 	// 	//Get rid of old surface
// 	// 	SDL_FreeSurface(textSurface);
// 	// }
// 	//
// 	// //Return success
// 	// return mTexture != NULL;
// }
//
//
//
// // void Font::Draw(const DisplayText &text, const Point &point, const Color &color) const
// // {
// // 	DrawAliased(text, round(point.X()), round(point.Y()), color);
// // }
//
//
//
// // void Font::DrawAliased(const DisplayText &text, double x, double y, const Color &color) const
// // {
// // 	int width = -1;
// // 	const string truncText = TruncateText(text, width);
// // 	const auto &layout = text.GetLayout();
// // 	if(width >= 0)
// // 	{
// // 		if(layout.align == Alignment::CENTER)
// // 			x += (layout.width - width) / 2;
// // 		else if(layout.align == Alignment::RIGHT)
// // 			x += layout.width - width;
// // 	}
// // 	DrawAliased(truncText, x, y, color);
// // }
// //
// //
// // void Font::Draw(const string &str, const Point &point, const Color &color) const
// // {
// // 	DrawAliased(str, round(point.X()), round(point.Y()), color);
// // }
// //
// //
// // void Font::DrawAliased(const string &str, double x, double y, const Color &color) const
// // {
// // 	glUseProgram(shader->Object());
// // 	glBindTexture(GL_TEXTURE_2D, texture);
// // 	glBindVertexArray(vao);
// //
// // 	glUniform4fv(colorI, 1, color.Get());
// //
// // 	// Update the scale, only if the screen size has changed.
// // 	if(Screen::Width() != screenWidth || Screen::Height() != screenHeight)
// // 	{
// // 		screenWidth = Screen::Width();
// // 		screenHeight = Screen::Height();
// // 		scale[0] = 2.f / screenWidth;
// // 		scale[1] = -2.f / screenHeight;
// // 	}
// // 	glUniform2fv(scaleI, 1, scale);
// // 	glUniform2f(glyphSizeI, glyphWidth, glyphHeight);
// //
// // 	GLfloat textPos[2] = {
// // 		static_cast<float>(x - 1.),
// // 		static_cast<float>(y)
// // 	};
// // 	int previous = 0;
// // 	bool isAfterSpace = true;
// // 	bool underlineChar = false;
// // 	const int underscoreGlyph = max(0, min(GLYPHS - 1, '_' - 32));
// //
// // 	for(char c: str)
// // 	{
// // 		if(c == '_')
// // 		{
// // 			underlineChar = showUnderlines;
// // 			continue;
// // 		}
// //
// // 		int glyph = Glyph(c, isAfterSpace);
// // 		if(c != '"' && c != '\'')
// // 			isAfterSpace = !glyph;
// // 		if(!glyph)
// // 		{
// // 			textPos[0] += space;
// // 			continue;
// // 		}
// //
// // 		glUniform1i(glyphI, glyph);
// // 		glUniform1f(aspectI, 1.f);
// //
// // 		textPos[0] += advance[previous * GLYPHS + glyph] + KERN;
// // 		glUniform2fv(positionI, 1, textPos);
// //
// // 		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
// //
// // 		if(underlineChar)
// // 		{
// // 			glUniform1i(glyphI, underscoreGlyph);
// // 			glUniform1f(aspectI, static_cast<float>(advance[glyph * GLYPHS] + KERN)
// // 								/ (advance[underscoreGlyph * GLYPHS] + KERN));
// //
// // 			glUniform2fv(positionI, 1, textPos);
// //
// // 			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
// // 			underlineChar = false;
// // 		}
// //
// // 		previous = glyph;
// // 	}
// //
// // 	glBindVertexArray(0);
// // 	glUseProgram(0);
// // }
// //
// //
// // int Font::Width(const string &str, char after) const
// // {
// // 	return WidthRawString(str.c_str(), after);
// // }
// //
// //
// // int Font::FormattedWidth(const DisplayText &text, char after) const
// // {
// // 	int width = -1;
// // 	const string truncText = TruncateText(text, width);
// // 	return width < 0 ? WidthRawString(truncText.c_str(), after) : width;
// // }
// //
// //
// // int Font::Height() const noexcept
// // {
// // 	return height;
// // }
// //
// //
// // int Font::Space() const noexcept
// // {
// // 	return space;
// // }
// //
// //
// // void Font::ShowUnderlines(bool show) noexcept
// // {
// // 	showUnderlines = show || Preferences::Has("Always underline shortcuts");
// // }
// //
// //
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
// //
// //
// // int Font::WidthRawString(const char *str, char after) const noexcept
// // {
// // 	int width = 0;
// // 	int previous = 0;
// // 	bool isAfterSpace = true;
// //
// // 	for(; *str; ++str)
// // 	{
// // 		if(*str == '_')
// // 			continue;
// //
// // 		int glyph = Glyph(*str, isAfterSpace);
// // 		if(*str != '"' && *str != '\'')
// // 			isAfterSpace = !glyph;
// // 		if(!glyph)
// // 			width += space;
// // 		else
// // 		{
// // 			width += advance[previous * GLYPHS + glyph] + KERN;
// // 			previous = glyph;
// // 		}
// // 	}
// // 	width += advance[previous * GLYPHS + max(0, min(GLYPHS - 1, after - 32))];
// //
// // 	return width;
// // }
// //
// //
// // // Param width will be set to the width of the return value, unless the layout width is negative.
// // string Font::TruncateText(const DisplayText &text, int &width) const
// // {
// // 	width = -1;
// // 	const auto &layout = text.GetLayout();
// // 	const string &str = text.GetText();
// // 	if(layout.width < 0 || (layout.align == Alignment::LEFT && layout.truncate == Truncate::NONE))
// // 		return str;
// // 	width = layout.width;
// // 	switch(layout.truncate)
// // 	{
// // 		case Truncate::NONE:
// // 			width = WidthRawString(str.c_str());
// // 			return str;
// // 		case Truncate::FRONT:
// // 			return TruncateFront(str, width);
// // 		case Truncate::MIDDLE:
// // 			return TruncateMiddle(str, width);
// // 		case Truncate::BACK:
// // 		default:
// // 			return TruncateBack(str, width);
// // 	}
// // }
// //
// //
// // string Font::TruncateBack(const string &str, int &width) const
// // {
// // 	return TruncateEndsOrMiddle(str, width,
// // 		[](const string &str, int charCount) {
// // 			return str.substr(0, charCount) + "...";
// // 		});
// // }
// //
// //
// // string Font::TruncateFront(const string &str, int &width) const
// // {
// // 	return TruncateEndsOrMiddle(str, width,
// // 		[](const string &str, int charCount) {
// // 			return "..." + str.substr(str.size() - charCount);
// // 		});
// // }
// //
// //
// // string Font::TruncateMiddle(const string &str, int &width) const
// // {
// // 	return TruncateEndsOrMiddle(str, width,
// // 		[](const string &str, int charCount) {
// // 			return str.substr(0, (charCount + 1) / 2) + "..." + str.substr(str.size() - charCount / 2);
// // 		});
// // }
// //
// //
// // string Font::TruncateEndsOrMiddle(const string &str, int &width,
// // 								function<string(const string &, int)> getResultString) const
// // {
// // 	int firstWidth = WidthRawString(str.c_str());
// // 	if(firstWidth <= width)
// // 	{
// // 		width = firstWidth;
// // 		return str;
// // 	}
// //
// // 	int workingChars = 0;
// // 	int workingWidth = 0;
// //
// // 	int low = 0, high = str.size() - 1;
// // 	while(low <= high)
// // 	{
// // 		// Think "how many chars to take from both ends, omitting in the middle".
// // 		int nextChars = (low + high) / 2;
// // 		int nextWidth = WidthRawString(getResultString(str, nextChars).c_str());
// // 		if(nextWidth <= width)
// // 		{
// // 			if(nextChars > workingChars)
// // 			{
// // 				workingChars = nextChars;
// // 				workingWidth = nextWidth;
// // 			}
// // 			low = nextChars + (nextChars == low);
// // 		} else
// // 			high = nextChars - 1;
// // 	}
// // 	width = workingWidth;
// // 	return getResultString(str, workingChars);
// // }
