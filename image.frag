#version 300 es
precision mediump float;

in vec2 TexCoord;

uniform sampler2D inputTexture;

void main()
{
	gl_FragColor = texture(inputTexture,TexCoord);
}
