/* fill.frag
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/
precision mediump float;
precision mediump sampler2DArray;

// The user must supply a texture and a color (white by default).
uniform sampler2D tex;
uniform vec4 color;

// Input from vert
in vec2 fragTexCoord;

// Output color.
out vec4 finalColor;

// Multiply the texture by the user-specified color (including alpha).
void main() {
	finalColor = vec4(texture(tex, fragTexCoord).a * 1.2) * color;
}
