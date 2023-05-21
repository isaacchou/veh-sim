/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <stdlib.h>
#include <glad/gl.h>
#include "Shaders.h"
#include "../Utils.h"

static unsigned int createShader(GLenum type, const char* shader_code)
{
	unsigned int shader;
	shader = glCreateShader(type);
	glShaderSource(shader, 1, &shader_code, NULL);
	glCompileShader(shader);

	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char log[512];
		glGetShaderInfoLog(shader, 512, NULL, log);
		debug_log("Failed to compile shader: %s\n", log);
		exit(EXIT_FAILURE);
	}
	return shader;
}

unsigned int setupShaderProgram()
{
	unsigned int vertexShader;
	vertexShader = createShader(GL_VERTEX_SHADER, R"glsl(
		#version 330 core

		layout(location = 0) in vec3 pos;
		layout(location = 1) in vec2 t;
		layout(location = 2) in vec3 n;

		out vec3 normal;
		out vec2 txtr_pos;

		uniform mat4 model;
		uniform mat4 view;
		uniform mat4 projection;

		void main()
		{
			normal = mat3(transpose(inverse(model))) * n;
			txtr_pos = t;
			gl_Position = projection * view * model * vec4(pos, 1.0);
		}
	)glsl");

	unsigned int fragmentShader;
	fragmentShader = createShader(GL_FRAGMENT_SHADER, R"glsl(
		#version 330 core

		struct Light {
			float ambient;
			vec3 direction;
		};
		in vec3 normal;
		in vec2 txtr_pos;
		out vec4 clr;

		uniform sampler2D txtr;
		uniform Light light;

		void main()
		{
			float light = max(dot(normal, -light.direction), 0.0) + light.ambient;
			clr = vec4(light * texture(txtr, txtr_pos).rgb, 1.0);
		}
	)glsl");
	unsigned int shaderProgram;
	shaderProgram = glCreateProgram();

	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	int success;
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		char log[512];
		glGetProgramInfoLog(shaderProgram, 512, NULL, log);
		debug_log("Failed to link program: %s\n", log);
		exit(EXIT_FAILURE);
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	return shaderProgram;
}
