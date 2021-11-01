#version 100

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

precision mediump float;

varying vec2 Texcoord;

uniform sampler2D tex;

void main()
{
	gl_FragColor = texture2D(tex, Texcoord);
}
