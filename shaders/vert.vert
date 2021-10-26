#version 300 es

/*
 * Copyright (C) 2017-2020 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 */

in vec2 position;
in vec2 texcoord;

out vec2 Texcoord;

void main()
{
	Texcoord = texcoord;
	gl_Position = vec4(position, 0.0, 1.0);
}
