/* Utf8String.h
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

#include "Utf8.h"

#include <vector>



class Utf8String {
public:
	class Utf8StringIterator {
	public:
		explicit Utf8StringIterator(const std::string *str, size_t bytePos = 0);
		Utf8StringIterator(const std::string *str, size_t bytePos, char32_t codepoint);
		char32_t operator*() const;
		Utf8StringIterator &operator++();
		bool operator!=(const Utf8StringIterator &other) const;
		bool operator==(const Utf8StringIterator &other) const;

		size_t codepointStartByte = 0;
		size_t codepointNextByte = 0;

	private:
		const std::string *str;
		char32_t codepoint = '\0';
	};

	Utf8String() = default;
	Utf8String(std::string str);
	Utf8String(const char *str);

	Utf8StringIterator begin() const;
	Utf8StringIterator end() const;

	// Returns number of codepoints.
	size_t length() const;
	// Returns number of bytes (chars) of data.
	size_t size() const;
	// Returns substring from codepoint pos with length len codepoints.
	Utf8String substr(size_t pos, size_t len = std::string::npos) const;
	// Returns index of codepoint containing `search`
	size_t find(const char *search, size_t size = 0) const;
	void append(const Utf8String &text, size_t start, size_t size);

	std::string to_string() const;
	const char *c_str() const;
	const char *data() const;

	// Check if string is empty.
	bool empty() const;

	// Capacity methods.
	size_t capacity() const;
	size_t max_size() const;
	void clear();
	void shrink_to_fit();
	void reserve(size_t size);
	void assign(const char *it, size_t size);

	// Compare to constant char string.
	// bool operator==(const Utf8String *other) const;
	bool operator==(const Utf8String &other) const;
	bool operator==(const char *other) const;
	// 8-bit char codepoints
	Utf8String & operator+=(char ch);
	// 32-bit UTF32 codepoints
	Utf8String & operator+=(char32_t codepoint);
	// strings
	// Utf8String & operator+=(const std::string& s);
	// Utf8String
	Utf8String& operator+=(const Utf8String& other);

	Utf8String operator+(char c) const;
	Utf8String operator+(const Utf8String &other) const;

private:
	std::string str;
};
