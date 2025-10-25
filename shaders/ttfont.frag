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

uniform vec2 size;
// The user must supply a texture and a color (white by default).
uniform sampler2D tex;
uniform vec4 color;

// Input from vert
in vec2 fragTexCoord;

// Output color.
out vec4 finalColor;

// Multiply the texture by the user-specified color (including alpha).
void main() {
	vec4 col;
//	col = vec4(texture(tex, fragTexCoord).w) * color;

	col = vec4(texture(tex, fragTexCoord).a);
	col = vec4(col.rgb * color.rgb, color.a);
//	col = mix(col, color, 0.000000000001);
//	col = vec4(col.w, col.w, col.w, col.w) * color;
//	col = vec4(col.w, col.w, col.w, 1) * vec4(color.xyz, 1);
//	float c = col.a > 0 ? 1 : 0;
//	col = vec4(c, c, c, col.a) * color; // mix(color, vec4(1,1,1,1), 0.9999999);
//	col = vec4(color.rgb, col.r);
//	col = vec4(col.w, col.w, col.w, col.w) * mix(color, vec4(1,1,1,1), 0.000000009999);
//	col = mix(col, color, 0.00000000000000009999);
//	col = mix(col, vec4(color.xyz, 0.01), 0.9999999);
//	col += vec4(.01, .01, .01, .01);
//	col += vec4(-.0, -.15, -.1, .01);

	// draw box around
//	col = mix(col, vec4(1, 0, 0, 1), (fragTexCoord.x >= 1./size.x) ? 0. : 1.);
//	col = mix(col, vec4(1, 0, 0, 1), (fragTexCoord.y >= 1./size.y) ? 0. : 1.);
//	col = mix(col, vec4(1, 0, 0, 1), (fragTexCoord.x <= (1. - 1./size.x)) ? 0. : 1.);
//	col = mix(col, vec4(1, 0, 0, 1), (fragTexCoord.y <= (1. - 1./size.y)) ? 0. : 1.);
	finalColor = col;
}
