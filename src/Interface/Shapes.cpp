/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <vector>
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Shapes.h"
#include "../Utils.h"

void Shape::set_texture(unsigned int texture)
{	// a (default) texture for all faces
	m_default_texture = texture;
}

void CompoundShape::create_mesh()
{
	for (child_shape& child : m_child_shapes) {
		child.shape->create_mesh();
	}
}

void CompoundShape::draw(unsigned int shader_program, const glm::mat4& trans) const
{
	for (const child_shape& child: m_child_shapes) {
		// combine the child transform
		child.shape->draw(shader_program, trans * child.trans);
	}
}

void CompoundShape::add_child_shape(Shape* shape, const glm::mat4& trans)
{
	child_shape child = { shape, trans };
	m_child_shapes.push_back(child);
}

void CompoundShape::set_texture(unsigned int texture)
{
	Shape::set_texture(texture); // set our own texture
	for (child_shape& child : m_child_shapes) {
		child.shape->set_texture(texture);
	}
}

SimpleShape::SimpleShape()
{
	m_VAO = 0;
	m_VBO = 0;
 	m_num_vertices = 0;
}

SimpleShape::~SimpleShape()
{
	glDeleteVertexArrays(1, &m_VAO);
	glDeleteBuffers(1, &m_VBO);
}

void SimpleShape::setup_shape(uv_vertex* vertices, int num)
{
	std::vector<glm::vec3> normal;
	uv_vertex* p = vertices;
	for (int n = 0; n < num; n += 3) {
		glm::vec3 p1(p[n].x, p[n].y, p[n].z);
		glm::vec3 p2(p[n + 1].x, p[n + 1].y, p[n + 1].z);
		glm::vec3 p3(p[n + 2].x, p[n + 2].y, p[n + 2].z);
		// one normal vector for each vertex!!!
		glm::vec3 norm = glm::normalize(glm::cross(p2 - p1, p3 - p1));
		normal.push_back(norm);
		normal.push_back(norm);
		normal.push_back(norm);
	}
	unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	
	size_t offset = sizeof(uv_vertex) * num;
	size_t buffer_size = offset + normal.size() * sizeof(glm::vec3);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, offset, vertices);
	glBufferSubData(GL_ARRAY_BUFFER, offset, normal.size() * sizeof(glm::vec3), normal.data());

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(uv_vertex), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(uv_vertex), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)offset);
	glEnableVertexAttribArray(2);

	m_VAO = VAO;
	m_VBO = VBO;
	m_num_vertices = num;
}

void SimpleShape::draw(unsigned int shader_program, const glm::mat4& trans) const
{
	glm::mat4 model = trans;
	glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, &model[0][0]);
	
	// if no texture is set, draw wireframe
	for (int i = 0; i < m_mesh_index.size(); i++)
	{
		int index = m_mesh_index[i];
		int n = (i == m_mesh_index.size() - 1) ? m_num_vertices - index : m_mesh_index[i + 1] - index;
		int t = (i < m_textures.size()) ? m_textures[i] : m_default_texture;
		
		glPolygonMode(GL_FRONT_AND_BACK, t == 0 ? GL_LINE : GL_FILL);
		glActiveTexture(GL_TEXTURE0 + t);
		glBindTexture(GL_TEXTURE_2D, t);
		glUniform1i(glGetUniformLocation(shader_program, "txtr"), t);

		glBindVertexArray(m_VAO);
		glDrawArrays(GL_TRIANGLES, index, n);
	}
}

void SphereShape::create_mesh()
{	// create vertices/triangles for a UV-sphere
	int step = 10;
	std::vector<uv_vertex> vertices;
	for (int u = 0; u <= 180; u += step) {
		for (int v = 0; v <= 360; v += step) {

			float latitude = glm::radians((float)(u - 90));
			float longitude = glm::radians((float)v);

			uv_vertex uv;
			uv.x = m_radius * glm::cos(latitude) * glm::cos(longitude);
			uv.z = m_radius * glm::cos(latitude) * glm::sin(longitude);
			uv.y = m_radius * glm::sin(latitude);
			uv.texture_x = (float)v / 360.f;
			uv.texture_y = (float)u / 180.f;
			vertices.push_back(uv);
		}
	}

	int nlat = (180 / step) + 1;
	int nlon = (360 / step) + 1;
	std::vector<uv_vertex> triangles;
	uv_vertex* line = vertices.data();
	uv_vertex* next_line = line + nlon;
	for (int u = 0; u < (nlat - 1); u += 1) {
		for (int v = 0; v < (nlon - 1); v += 1) {

			triangles.push_back(line[v]);
			triangles.push_back(next_line[v]);
			triangles.push_back(next_line[v + 1]);

			triangles.push_back(line[v]);
			triangles.push_back(next_line[v + 1]);
			triangles.push_back(line[v + 1]);
		}
		line += nlon;
		next_line += nlon;
	}
	m_mesh_index.push_back(0);
	setup_shape(triangles.data(), (int)triangles.size());
}

// In texture coordinates, (0, 0) is the bottom left, (1, 1) the top right
void CapsuleShape::create_mesh()
{	// total height = height + 2 * radius
	int step = 10;
	std::vector<uv_vertex> vertices;
	float texture_offset = 0;
	for (int u = 0; u <= 180; u += step) {
		
		float latitude = glm::radians((float)(u - 90));  // from -90 to 90
		float offset = m_height / 2.f;
		float texture_y = ((float)u / 180.f) * m_radius / (offset + m_radius) + texture_offset;
		if (u <= 90) offset = -offset;

		uv_vertex uv;
		uv.y = m_radius * glm::sin(latitude) + offset;
		uv.texture_y = texture_y;
		for (int v = 0; v <= 360; v += step) {

			float longitude = glm::radians((float)v);
			uv.x = m_radius * glm::cos(latitude) * glm::cos(longitude);
			uv.z = m_radius * glm::cos(latitude) * glm::sin(longitude);
			uv.texture_x = (float)v / 360.f;
			vertices.push_back(uv);
		}
		debug_log_mute("capsule latitude: %f y: %f texture_y: %f offset: %f\n", latitude, uv.y, texture_y, offset);

		// repeat the equator on the other side
		if (u == 90) {
			// flip it back as it's part of the northern hemispere (u >= 90)
			offset = -offset;
			texture_offset = offset / (offset + m_radius);
			texture_y += texture_offset;
			uv.y = m_radius* glm::sin(latitude) + offset;
			uv.texture_y = texture_y;
			
			debug_log_mute("capsule latitude: %f y: %f texture_y: %f offset: %f\n", latitude, uv.y, texture_y, offset);
			for (int v = 0; v <= 360; v += step) {

				float longitude = glm::radians((float)v);
				uv.x = m_radius * glm::cos(latitude) * glm::cos(longitude);
				uv.z = m_radius * glm::cos(latitude) * glm::sin(longitude);
				uv.texture_x = (float)v / 360.f;
				vertices.push_back(uv);
			}
		}
	}

	int nlat = (180 / step) + 2;
	int nlon = (360 / step) + 1;
	std::vector<uv_vertex> triangles;
	uv_vertex* line = vertices.data();
	uv_vertex* next_line = line + nlon;
	for (int u = 0; u < (nlat - 1); u += 1) {
		for (int v = 0; v < (nlon - 1); v += 1) {

			triangles.push_back(line[v]);
			triangles.push_back(next_line[v]);
			triangles.push_back(next_line[v + 1]);

			triangles.push_back(line[v]);
			triangles.push_back(next_line[v + 1]);
			triangles.push_back(line[v + 1]);
		}
		line += nlon;
		next_line += nlon;
	}
	m_mesh_index.push_back(0);
	setup_shape(triangles.data(), (int)triangles.size());
}

void CylinderShape::create_mesh()
{	// a cylinder along the Y-axis centered at (0, 0, 0)
	int step = 10;
	std::vector<uv_vertex> vertices;
	for (int u = 0; u <= 360; u += step) {
		float angle = glm::radians((float)u);
		float sine = glm::sin(angle);
		float cosine = glm::cos(angle);
		
		uv_vertex uv;
		uv.x = m_radius * glm::sin(angle);
		uv.z = m_radius * glm::cos(angle);
		uv.y = m_half_height;

		uv.texture_x = 0.5f + sine / 2.f;;
		uv.texture_y = 0.5f + cosine / 2.f;;
		vertices.push_back(uv);
	}

	std::vector<uv_vertex> triangles;
	
	// top mesh
	uv_vertex* data = vertices.data();
	m_mesh_index.push_back((int)triangles.size());
	uv_vertex center = { 0.f, m_half_height, 0.f, 0.5f, 0.5f };
	for (int i = 0; i < vertices.size() - 1; i++) {
		triangles.push_back(center);
		triangles.push_back(data[i]);
		triangles.push_back(data[i + 1]);
	}

	// side mesh
	data = vertices.data();
	m_mesh_index.push_back((int)triangles.size());
	int n = (int)vertices.size() - 1;
	for (int i = 0; i < vertices.size() - 1; i++) {

		float x1 = (float)i / (float)n;
		float x2 = (float)(i + 1) / (float)n;
		uv_vertex a1 = data[i];
		a1.texture_x = x1;
		a1.texture_y = 0.f;

		uv_vertex b1 = a1;
		b1.y = -m_half_height;
		b1.texture_x = x1;
		b1.texture_y = 1.f;

		uv_vertex a2 = data[i + 1];
		a2.texture_x = x2;
		a2.texture_y = 0.f;

		uv_vertex b2 = a2;
		b2.y = -m_half_height;
		b2.texture_x = x2;
		b2.texture_y = 1.f;

		triangles.push_back(a1);
		triangles.push_back(b1);
		triangles.push_back(a2);

		triangles.push_back(b1);
		triangles.push_back(b2);
		triangles.push_back(a2);
	}

	// bottom mesh
	data = vertices.data();
	m_mesh_index.push_back((int)triangles.size());
	center.y = -m_half_height;
	for (int i = 0; i < vertices.size() - 1; i++) {

		uv_vertex b1 = data[i];
		b1.y = -m_half_height;

		uv_vertex b2 = data[i + 1];
		b2.y = -m_half_height;

		triangles.push_back(center);
		triangles.push_back(b2);
		triangles.push_back(b1);
	}
	setup_shape(triangles.data(), (int)triangles.size());
}

void ConeShape::create_mesh()
{	// a cone along the Y-axis centered at (0, 0, 0) 
	int step = 10;
	float halfHeight = m_height / 2.f;
	std::vector<uv_vertex> vertices;
	for (int u = 0; u <= 360; u += step) {
		float angle = glm::radians((float)u);
		float sine = glm::sin(angle);
		float cosine = glm::cos(angle);

		uv_vertex uv;
		uv.x = m_radius * glm::sin(angle);
		uv.z = m_radius * glm::cos(angle);
		uv.y = -halfHeight;

		uv.texture_x = 0.5f + sine / 2.f;;
		uv.texture_y = 0.5f + cosine / 2.f;;
		vertices.push_back(uv);
	}

	std::vector<uv_vertex> triangles;

	// side (slope) mesh
	uv_vertex* data = vertices.data();
	uv_vertex center = { 0.f, halfHeight, 0.f, 0.5f, 0.5f };
	for (int i = 0; i < vertices.size() - 1; i++) {
		triangles.push_back(center);
		triangles.push_back(data[i]);
		triangles.push_back(data[i + 1]);
	}
	
	m_mesh_index.push_back(0);
	m_mesh_index.push_back((int)triangles.size());

	// bottom mesh
	data = vertices.data();
	center.y = -halfHeight;
	for (int i = 0; i < vertices.size() - 1; i++) {
		triangles.push_back(center);
		triangles.push_back(data[i + 1]);
		triangles.push_back(data[i]);
	}
	setup_shape(triangles.data(), (int)triangles.size());
}

void BoxShape::create_mesh()
{
	uv_vertex vertices[] = {
		//----vertex----|--texture--
		// looking at the cube in the +z direction
		// triangle #1 of front face (z = -1)
		{-1.f, -1.f, -1.f, 0.0f, 0.0f},
		{ 1.f,  1.f, -1.f, 1.0f, 1.0f},
		{ 1.f, -1.f, -1.f, 1.0f, 0.0f},
		// triangle #2 of front face (z = -1)
		{ 1.f,  1.f, -1.f, 1.0f, 1.0f},
		{-1.f, -1.f, -1.f, 0.0f, 0.0f},
		{-1.f,  1.f, -1.f, 0.0f, 1.0f},
		
		// triangle #1 of back face (z = 1)
		{-1.f, -1.f,  1.f, 0.0f, 0.0f},
		{ 1.f, -1.f,  1.f, 1.0f, 0.0f},
		{ 1.f,  1.f,  1.f, 1.0f, 1.0f},
		// triangle #2 of back face (z = 1)
		{ 1.f,  1.f,  1.f, 1.0f, 1.0f},
		{-1.f,  1.f,  1.f, 0.0f, 1.0f},
		{-1.f, -1.f,  1.f, 0.0f, 0.0f},
		
		// triangle #1 of left face (x = -1)
		{-1.f,  1.f,  1.f, 1.0f, 0.0f},
		{-1.f,  1.f, -1.f, 1.0f, 1.0f},
		{-1.f, -1.f, -1.f, 0.0f, 1.0f},
		// triangle #2 of left face (x = -1)
		{-1.f, -1.f, -1.f, 0.0f, 1.0f},
		{-1.f, -1.f,  1.f, 0.0f, 0.0f},
		{-1.f,  1.f,  1.f, 1.0f, 0.0f},
		
		// triangle #1 of right face (x = 1)
		{ 1.f,  1.f,  1.f, 1.0f, 0.0f},
		{ 1.f, -1.f, -1.f, 0.0f, 1.0f},
		{ 1.f,  1.f, -1.f, 1.0f, 1.0f},
		// triangle #2 of right face (x = 1)
		{ 1.f, -1.f, -1.f, 0.0f, 1.0f},
		{ 1.f,  1.f,  1.f, 1.0f, 0.0f},
		{ 1.f, -1.f,  1.f, 0.0f, 0.0f},
		
		// triangle #1 of bottom face (y = -1)
		{-1.f, -1.f, -1.f, 0.0f, 1.0f},
		{ 1.f, -1.f, -1.f, 1.0f, 1.0f},
		{ 1.f, -1.f,  1.f, 1.0f, 0.0f},
		// triangle #2 of bottom face (y = -1)
		{ 1.f, -1.f,  1.f, 1.0f, 0.0f},
		{-1.f, -1.f,  1.f, 0.0f, 0.0f},
		{-1.f, -1.f, -1.f, 0.0f, 1.0f},
		
		// triangle #1 of top face (y = 1)
		{-1.f,  1.f, -1.f, 0.0f, 1.0f},
		{ 1.f,  1.f,  1.f, 1.0f, 0.0f},
		{ 1.f,  1.f, -1.f, 1.0f, 1.0f},
		// triangle #2 of top face (y = 1)
		{ 1.f,  1.f,  1.f, 1.0f, 0.0f},
		{-1.f,  1.f, -1.f, 0.0f, 1.0f},
		{-1.f,  1.f,  1.f, 0.0f, 0.0f}
	};

	int num_vertices = sizeof(vertices) / sizeof(uv_vertex);
	for (int i = 0; i < num_vertices; i += 1)
	{
		vertices[i].x *= m_cx;
		vertices[i].y *= m_cy;
		vertices[i].z *= m_cz;
	}
	m_mesh_index.push_back(0);
	m_mesh_index.push_back(6);
	m_mesh_index.push_back(12);
	m_mesh_index.push_back(18);
	m_mesh_index.push_back(24);
	m_mesh_index.push_back(30);
	setup_shape(vertices, num_vertices);
}

//~~~
// GroundShape can be either static plane or height field terrain
//~~~
void GroundShape::create_mesh()
{
	uv_vertex vertices[4] = {
		{  1.f, 0.f,  1.f, 0.f, 0.f },
		{  1.f, 0.f, -1.f, 0.f, 1.f },
		{ -1.f, 0.f, -1.f, 1.f, 1.f },
		{ -1.f, 0.f,  1.f, 1.f, 0.f }
	};

	for (uv_vertex& uv : vertices) {
		uv.x *= (m_width / 2.f);
		uv.z *= (m_length / 2.f);
	}

	std::vector<uv_vertex> triangles;
	m_mesh_index.push_back(0);
	triangles.push_back(vertices[0]);
	triangles.push_back(vertices[1]);
	triangles.push_back(vertices[2]);

	triangles.push_back(vertices[0]);
	triangles.push_back(vertices[2]);
	triangles.push_back(vertices[3]);
	setup_shape(triangles.data(), (int)triangles.size());
}
