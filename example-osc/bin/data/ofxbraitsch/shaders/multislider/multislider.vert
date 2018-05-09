#version 410

#pragma include <of_default_vertex_in_attributes.glsl>
#pragma include <of_default_uniforms.glsl>

void main(){
	gl_Position = modelViewProjectionMatrix * position;
}
