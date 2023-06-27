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

std::string to_string(const glm::mat4& m)
{
	std::string s;
	int n = sizeof(m) / sizeof(float);
	const float* p = &m[0][0];
	for (int i = 0; i < n; i++) {
		if (i != 0) s += ",";
		s += std::to_string(*p++);
	}
	return std::move(s);
}

std::string CompoundShape::to_json(const glm::mat4& trans) const
{
	std::string json = "{\"child\":[";
	bool first = true;
	for (const child_shape& child : m_child_shapes) {
		if (!first) json += ",";
		json += child.shape->to_json(child.trans);
		first = false;
	}
	json += "],\"trans\":[" + to_string(trans) + "]}";
	return std::move(json);
}

std::string Shape::to_json(const glm::mat4& trans) const
{
	std::string json = "{\"mesh\":[";
	bool first = true;
	for (const auto& uv : m_mesh) {
		if (!first) json += ",";
		json += std::to_string(uv.x) + ",";
		json += std::to_string(uv.y) + ",";
		json += std::to_string(uv.z) + ",";
		json += std::to_string(uv.texture_x) + ",";
		json += std::to_string(uv.texture_y);
		first = false;
	}

	json += "],\"face_index\":[";
	first = true;
	for (auto& i : m_face_index) {
		if (!first) json += ",";
		json += std::to_string(i);
		first = false;
	}
	json += "],";
	
	if (m_default_texture != 0) {
		json += "\"default_texture\":" + std::to_string(m_default_texture) + ",";
	}
	
	if (m_textures.size() > 0) {
		json += "\"textures\":[";
		first = true;
		for (auto& i : m_textures) {
			if (!first) json += ",";
			json += std::to_string(i);
			first = false;
		}
		json += "],";
	}
	json += "\"trans\":[" + to_string(trans) + "]}";
	return std::move(json);
}

void CompoundShape::add_child_shape(Shape* shape, const glm::mat4& trans)
{
	child_shape child = { shape, trans };
	m_child_shapes.push_back(child);
}

void CompoundShape::add_child_shape(Shape* child, const glm::vec3& origin, const glm::vec3& rotation, float angle)
{
	glm::mat4 trans(1.f);
	// translate first then rotate
	// because translate() moves the object along the axes
	// and rotate() will change the object's axes
	trans = glm::translate(trans, origin);
	trans = glm::rotate(trans, glm::radians(angle), rotation);
	add_child_shape(child, trans);
}

void CompoundShape::set_texture(unsigned int texture)
{
	Shape::set_texture(texture); // set our own texture
	for (child_shape& child : m_child_shapes) {
		child.shape->set_texture(texture);
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
	uv_vertex* line = vertices.data();
	uv_vertex* next_line = line + nlon;
	for (int u = 0; u < (nlat - 1); u += 1) {
		for (int v = 0; v < (nlon - 1); v += 1) {

			m_mesh.push_back(line[v]);
			m_mesh.push_back(next_line[v]);
			m_mesh.push_back(next_line[v + 1]);

			m_mesh.push_back(line[v]);
			m_mesh.push_back(next_line[v + 1]);
			m_mesh.push_back(line[v + 1]);
		}
		line += nlon;
		next_line += nlon;
	}
	m_face_index.push_back(0);
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
	uv_vertex* line = vertices.data();
	uv_vertex* next_line = line + nlon;
	for (int u = 0; u < (nlat - 1); u += 1) {
		for (int v = 0; v < (nlon - 1); v += 1) {

			m_mesh.push_back(line[v]);
			m_mesh.push_back(next_line[v]);
			m_mesh.push_back(next_line[v + 1]);

			m_mesh.push_back(line[v]);
			m_mesh.push_back(next_line[v + 1]);
			m_mesh.push_back(line[v + 1]);
		}
		line += nlon;
		next_line += nlon;
	}
	m_face_index.push_back(0);
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

	// top mesh
	uv_vertex* data = vertices.data();
	m_face_index.push_back((int)m_mesh.size());
	uv_vertex center = { 0.f, m_half_height, 0.f, 0.5f, 0.5f };
	for (int i = 0; i < vertices.size() - 1; i++) {
		m_mesh.push_back(center);
		m_mesh.push_back(data[i]);
		m_mesh.push_back(data[i + 1]);
	}

	// side mesh
	m_face_index.push_back((int)m_mesh.size());
	int n = (int)vertices.size() - 1;
	for (int i = 0; i < n; i++) {

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

		m_mesh.push_back(a1);
		m_mesh.push_back(b1);
		m_mesh.push_back(a2);

		m_mesh.push_back(b1);
		m_mesh.push_back(b2);
		m_mesh.push_back(a2);
	}

	// bottom mesh
	m_face_index.push_back((int)m_mesh.size());
	center.y = -m_half_height;
	for (int i = 0; i < vertices.size() - 1; i++) {

		uv_vertex b1 = data[i];
		b1.y = -m_half_height;

		uv_vertex b2 = data[i + 1];
		b2.y = -m_half_height;

		m_mesh.push_back(center);
		m_mesh.push_back(b2);
		m_mesh.push_back(b1);
	}
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

	// side (slope) mesh
	uv_vertex* data = vertices.data();
	uv_vertex center = { 0.f, halfHeight, 0.f, 0.5f, 0.5f };
	for (int i = 0; i < vertices.size() - 1; i++) {
		m_mesh.push_back(center);
		m_mesh.push_back(data[i]);
		m_mesh.push_back(data[i + 1]);
	}
	
	m_face_index.push_back(0);
	m_face_index.push_back((int)m_mesh.size());

	// bottom mesh
	data = vertices.data();
	center.y = -halfHeight;
	for (int i = 0; i < vertices.size() - 1; i++) {
		m_mesh.push_back(center);
		m_mesh.push_back(data[i + 1]);
		m_mesh.push_back(data[i]);
	}
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
		m_mesh.push_back(vertices[i]);
	}
	m_face_index.push_back(0);
	m_face_index.push_back(6);
	m_face_index.push_back(12);
	m_face_index.push_back(18);
	m_face_index.push_back(24);
	m_face_index.push_back(30);
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

	m_face_index.push_back(0);
	m_mesh.push_back(vertices[0]);
	m_mesh.push_back(vertices[1]);
	m_mesh.push_back(vertices[2]);

	m_mesh.push_back(vertices[0]);
	m_mesh.push_back(vertices[2]);
	m_mesh.push_back(vertices[3]);
}

Shape* CreateGearShape(float radius, float half_thickness, int num_teeth, float tooth_half_width)
{
	if (tooth_half_width == 0.f) {
		tooth_half_width = radius * glm::sin(glm::radians(360.f / (2.f * num_teeth))) * .5f;
	}
	CompoundShape* gear_shape = new CompoundShape();
	CylinderShape* disk = new CylinderShape (radius, half_thickness);
	disk->set_texture(gear_shape->get_default_texture());
	gear_shape->add_child_shape(disk, glm::mat4(1.f));

	float n = 360.f / num_teeth;  // Make sure this divides evenly!!
	for (float i = 0.f; i < 360.f; i += n) {
		Shape* tooth = new BoxShape (tooth_half_width, half_thickness, tooth_half_width * 2);
		float x = radius * glm::sin(glm::radians(i));
		float z = radius * glm::cos(glm::radians(i));
		
		glm::mat4 m(1.f);
		m = glm::translate(m, glm::vec3(x, 0.f, z));
		m = glm::rotate(m, glm::radians(i), glm::vec3(0.f, 1.f, 0.f));
		tooth->set_texture(gear_shape->get_default_texture());
		gear_shape->add_child_shape (tooth, m);
	}
	return gear_shape;
}
