#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform constants {
	mat4 model;
	mat4 view;
	mat4 proj;
}
PushConstants;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
	gl_Position = PushConstants.proj * PushConstants.view * PushConstants.model * vec4(inPosition, 1.0);
	fragColor = inColor;
	fragTexCoord = inTexCoord;
}