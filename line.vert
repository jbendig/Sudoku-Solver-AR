// Copyright 2017 James Bendig. See the COPYRIGHT file at the top-level
// directory of this distribution.
//
// Licensed under:
//   the MIT license
//     <LICENSE-MIT or https://opensource.org/licenses/MIT>
//   or the Apache License, Version 2.0
//     <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>,
// at your option. This file may not be copied, modified, or distributed
// except according to those terms.

#version 330 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec3 Color;

void main()
{
	gl_Position = vec4(position,1.0);
	Color = color;
}
