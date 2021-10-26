#version 300 es

/*
 * Copyright (C) 2021 Philip Jones
 *
 * Licensed under the MIT License.
 * See either the LICENSE file, or:
 *
 * https://opensource.org/licenses/MIT
 *
 * I don't think you can really copyright this shader though :)
 */

precision highp float;

in vec2 Texcoord;

out vec4 FragColor;

uniform sampler2D tex;

void main()
{
	FragColor = texture(tex, Texcoord);
}
