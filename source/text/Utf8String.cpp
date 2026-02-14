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

#include "Utf8String.h"

#include "Utf8.h"

#include <cassert>
#include <cstring>
#include <utility>
#include <vector>

using namespace std;



Utf8String::Utf8StringIterator::Utf8StringIterator(const 
string* str, size_t bytePos)
	: codepointStartByte(bytePos), codepointNextByte(bytePos), str(str)
{
	// if (str && bytePos < str->size())
	codepoint = Utf8::DecodeCodePoint(*str, this->codepointNextByte);
}

Utf8String::Utf8StringIterator::Utf8StringIterator(const string* str, size_t bytePos, char32_t codepoint)
	: codepointStartByte(bytePos), codepointNextByte(bytePos), str(str), codepoint(codepoint)
{
}

char32_t Utf8String::Utf8StringIterator::operator*() const
{
	assert((str && codepoint) && "Trying to deference invalidated iterator!");
	return codepoint;
}

Utf8String::Utf8StringIterator & Utf8String::Utf8StringIterator::operator++()
{
	codepointStartByte = codepointNextByte;
	codepoint = Utf8::DecodeCodePoint(*str, codepointNextByte);
	return *this;
}

bool Utf8String::Utf8StringIterator::operator!=(const Utf8StringIterator& other) const
{
	return codepointNextByte != other.codepointNextByte;
}

bool Utf8String::Utf8StringIterator::operator==(const Utf8StringIterator &other) const
{
	return !(*this != other);
}


Utf8String::Utf8String(string str)
	: str(move(str))
{
}

Utf8String::Utf8String(const char *str)
	: str(str)
{
}

Utf8String::Utf8StringIterator Utf8String::begin() const
{
	return Utf8String::Utf8StringIterator(&str);
}

Utf8String::Utf8StringIterator Utf8String::end() const
{
	return {&str, string::npos, '\0'};
}

size_t Utf8String::length() const
{
	size_t count = 0;
	for (auto it = begin(); it != end(); ++it)
		++count;
	return count;
}

size_t Utf8String::size() const
{
	return str.size();
}

Utf8String Utf8String::substr(size_t pos, size_t len) const
{
	string result;
	size_t count = 0;
	size_t collected = 0;

	for (auto it = begin(); it != end(); ++it) {
		if (count >= pos) {
			result += str.substr(it.codepointStartByte, it.codepointNextByte - it.codepointStartByte);
			if (++collected == len)
				break;
		}
		++count;
	}

	return {result};
}

size_t Utf8String::find(const char* search, size_t start) const
{
	// Note: we must ensure that the found result is on a codepoint boundary
	size_t count = 0;
	size_t searchSize = strlen(search);

	for(auto it = begin(); it != end(); ++it)
	{
		if(count >= start)
			if(search == str.substr(it.codepointStartByte, searchSize))
				return count;
		++count;
	}
	return string::npos;
}

void Utf8String::append(const Utf8String& text, size_t start, size_t size)
{
	str += text.substr(start, size).to_string();
}

string Utf8String::to_string() const
{
	return str;
}

const char* Utf8String::c_str() const
{
	return str.c_str();
}

const char* Utf8String::data() const
{
	return this->str.data();
}

bool Utf8String::empty() const
{
	return str.empty();
}

size_t Utf8String::capacity() const
{
	return str.capacity();
}

size_t Utf8String::max_size() const
{
	return str.max_size();
}

void Utf8String::clear()
{
	str.clear();
}

void Utf8String::shrink_to_fit()
{
	str.shrink_to_fit();
}

void Utf8String::reserve(size_t size)
{
	str.reserve(size);
}

void Utf8String::assign(const char* it, size_t size)
{
	this->str.assign(it, size);
}

bool Utf8String::operator==(const Utf8String &other) const
{
	return str.c_str() == other.c_str();
}

bool Utf8String::operator==(const char* other) const
{
	return str.c_str() == other;
}

Utf8String &Utf8String::operator+=(char ch)
{
	str += ch;
	return *this;
}

Utf8String &Utf8String::operator+=(char32_t codepoint)
{
	str += Utf8::UTF32ToUTF8(codepoint);
	return *this;
}

Utf8String Utf8String::operator+(const Utf8String &other) const
{
	return {this->str + other.str};
}
