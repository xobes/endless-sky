/* FontSet.cpp
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

#include "FontSet.h"

#include "Font.h"

#include <map>

using namespace std;

namespace {
	std::map<std::string, Font> fonts;
}



void FontSet::Add(const std::filesystem::path &path, int size, string name)
{
	// For backwards compatibility, size alone will map to a font without a name.
	if(name.empty())
		name = to_string(size);
	if(!fonts.contains(name))
		fonts[name].Load(path, size);
}



// Retained for backwards compatibility.
const Font &FontSet::Get(int size)
{
	return Get(to_string(size));
}



const Font &FontSet::Get(const string &name)
{
	return fonts[name];
}



void FontSet::MarkFrameStart()
{
	for(auto &entry : fonts)
		entry.second.MarkTexturesUnused();
}



void FontSet::MarkFrameEnd()
{
	for(auto &entry : fonts)
		entry.second.ClearUnusedTextures();
}
