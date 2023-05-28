/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct uv_vertex
{
	float x;
	float y;
	float z;
	float texture_x;
	float texture_y;
};

class ShapeDesc
{
public:
	enum class Type {
		Ground,
		Box,
		Sphere,
		Cylinder,
		Cone,
		Capsule,
		Pyramid,
		Wedge,
		V150, // armored personel carrier vehicle body (V-150)
		Compound
	};

	struct child_desc {
		ShapeDesc* m_desc;
		glm::mat4 m_trans;
	};

	Type m_type;
	float m_param[4];
	unsigned int m_default_texture; // a (default) texture for all faces
	std::vector<unsigned int> m_textures;

	ShapeDesc() : m_default_texture(0) {}
	virtual ~ShapeDesc() {}

	virtual void set_texture(unsigned int texture) { m_default_texture = texture; }
	virtual void add_texture(unsigned int texture, int repeat = 1) {
		while (repeat-- > 0) m_textures.push_back(texture); 
	}
};
			
class CompoundShapeDesc : public ShapeDesc
{
private:
	std::vector<child_desc> m_child_desc;

public:	
	CompoundShapeDesc() { m_type = Type::Compound; }
	virtual ~CompoundShapeDesc() {
		for (auto& desc : m_child_desc) {
			delete desc.m_desc;
		}
	}
	
	void add_child_shape_desc(ShapeDesc* child, const glm::mat4& trans) {
		m_child_desc.push_back({ child, trans });
	}

	void add_child_shape_desc(ShapeDesc* child, glm::vec3 origin, glm::vec3 rotation, float angle) {
		glm::mat4 trans(1.f);
		// translate first then rotate
		// because translate() moves the object along the axes
		// and rotate() will change the object's axes
		trans = glm::translate(trans, origin);
		trans = glm::rotate(trans, glm::radians(angle), rotation);
		add_child_shape_desc(child, trans);
	}
	
	const std::vector<child_desc>& get_child_shape_desc() const { return m_child_desc; }

	virtual void set_texture(unsigned int texture) {
		ShapeDesc::set_texture(texture); // set our own texture
		for (child_desc& child : m_child_desc) {
			child.m_desc->set_texture(texture);
		}
	}
};

class GroundShapeDesc : public ShapeDesc
{
public:
	GroundShapeDesc(float width, float length)
	{
		m_type = Type::Ground;
		m_param[0] = width;
		m_param[1] = length;
	}
	virtual ~GroundShapeDesc() {}
};

class BoxShapeDesc : public ShapeDesc
{
public:
	BoxShapeDesc(float cx, float cy, float cz)
	{
		m_type = Type::Box;
		m_param[0] = cx;
		m_param[1] = cy;
		m_param[2] = cz;
	}
	virtual ~BoxShapeDesc() {}
};

class SphereShapeDesc : public ShapeDesc
{
public:
	SphereShapeDesc(float radius)
	{
		m_type = Type::Sphere;
		m_param[0] = radius;
	}
	virtual ~SphereShapeDesc() {}
};

class CylinderShapeDesc : public ShapeDesc
{
public:
	CylinderShapeDesc(float radius, float half_height)
	{
		m_type = Type::Cylinder;
		m_param[0] = radius;
		m_param[1] = half_height;
	}
	virtual ~CylinderShapeDesc() {}
};

class ConeShapeDesc : public ShapeDesc
{
public:
	ConeShapeDesc(float radius, float height)
	{
		m_type = Type::Cone;
		m_param[0] = radius;
		m_param[1] = height;
	}
	virtual ~ConeShapeDesc() {}
};

class CapsuleShapeDesc : public ShapeDesc
{
public:
	CapsuleShapeDesc(float radius, float height)
	{
		m_type = Type::Capsule;
		m_param[0] = radius;
		m_param[1] = height;
	}
	virtual ~CapsuleShapeDesc() {}
};

class ConvexShapeDesc : public ShapeDesc
{
protected:
	std::vector<uv_vertex> m_vertices;
	
	ConvexShapeDesc() {}
	virtual ~ConvexShapeDesc() {}

	void face(std::vector<uv_vertex>& triangles, std::vector<int>& mesh_index, std::vector<int> v) const {
		mesh_index.push_back((int)triangles.size());
		size_t n = v.size();
		std::vector<int> t;
		if (n == 3) {
			// simple triangle
			t = { 0, 1, 2 };
		} else if (n == 4) {
			t = { 0, 1, 2, 2, 3, 0 };
		} else if (n == 5) {
			t = { 0, 1, 2, 2, 3, 4, 4, 0, 2 };
		} else if (n == 6) {
			t = { 0, 1, 2, 2, 3, 4, 4, 5, 0, 0, 2, 4 };
		}
		n = t.size();
		for (int i = 0; i < n; i++) triangles.push_back(m_vertices[v[t[n - 1 - i]]]);
	}

public:
	virtual void get_vertices(std::vector<glm::vec3>& vertices) const {
		for (const uv_vertex& uv : m_vertices) {
			vertices.push_back(glm::vec3(uv.x, uv.y, uv.z));
		}
	}
	virtual void get_mesh(std::vector<uv_vertex>& triangles, std::vector<int>& mesh_index) const = 0;
};

class PyramidShapeDesc : public ConvexShapeDesc
{
public:
	PyramidShapeDesc(float cx, float cy, float cz) 
	{
		m_type = Type::Pyramid;
		m_param[0] = cx;
		m_param[1] = cy;
		m_param[2] = cz;

		uv_vertex vertices[] = {
			{  0.f,  1.f,  0.f, .5f, .5f },
			{  1.f, -1.f,  1.f, 0.f, 0.f },
			{  1.f, -1.f, -1.f, 0.f, 1.f },
			{ -1.f, -1.f, -1.f, 1.f, 1.f },
			{ -1.f, -1.f,  1.f, 1.f, 0.f }
		};

		for (uv_vertex& uv : vertices) {
			uv.x *= cx; uv.y *= cy; uv.z *= cz;
			m_vertices.push_back(uv);
		}
	}
	virtual ~PyramidShapeDesc() {}
	
	virtual void get_mesh(std::vector<uv_vertex>& triangles, std::vector<int>& mesh_index) const {
		int v[] = { 
			// side faces (0 - 11)
			0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 
			// bottom face (12 - 17)
			1, 2, 3, 3, 4, 1 };
		for (int i : v) {
			triangles.push_back(m_vertices[i]);
		}
		mesh_index.push_back(0);  // all side faces mapped to one texture
		mesh_index.push_back(12); // bottom face has its own texture
	}
};

class WedgeShapeDesc : public ConvexShapeDesc
{
public:
	WedgeShapeDesc(float cx, float cy, float cz, float half_length)
	{
		m_type = Type::Wedge;
		m_param[0] = cx;
		m_param[1] = cy;
		m_param[2] = cz;
		m_param[3] = half_length;

		uv_vertex vertices[] = {
			// top vertices
			{  0.f,  1.f,  1.f, .5f, 0.f },
			{  0.f,  1.f, -1.f, .5f, 1.f },
			// bottom vertices
			{  1.f, -1.f,  1.f, 0.f, 0.f },
			{  1.f, -1.f, -1.f, 0.f, 1.f },
			{ -1.f, -1.f, -1.f, 1.f, 1.f },
			{ -1.f, -1.f,  1.f, 1.f, 0.f }
		};

		for (uv_vertex& uv : vertices) {
			uv.x *= cx; uv.y *= cy; 
			uv.z *= (uv.x == 0.f ? half_length : cz);
			m_vertices.push_back(uv);
		}
	}
	virtual ~WedgeShapeDesc() {}

	virtual void get_mesh(std::vector<uv_vertex>& triangles, std::vector<int>& mesh_index) const {
		int v[] = { 
			// face #1: front and back roofs (0 - 11)
			0, 2, 1, 1, 2, 3, 0, 1, 5, 1, 4, 5, 
			// face #2: left and right gables (12 - 17)
			0, 5, 2, 1, 3, 4,
			// face #3: bottom (18 - 23)
			2, 4, 3, 2, 5, 4 };
		for (int i : v) {
			triangles.push_back(m_vertices[i]);
		}
		// for the gables, the tip vertices are at the mid-point of the texture
		triangles[12].texture_y = .5f;
		triangles[15].texture_y = .5f;
		
		mesh_index.push_back(0);  // front and back roofs
		mesh_index.push_back(12); // gables
		mesh_index.push_back(18); // bottom face
	}
};

class V150 : public ConvexShapeDesc
{
public:
	V150(float scale) {
		m_type = Type::V150;
		m_param[0] = scale;

		uv_vertex vertices[] = {
			// default dimensions: W=10, L=20, H=5
			// texture coordinates are not defined!!
			{  2.5f, 2.f, 6.f,  1.f, 1.f },
			{ -2.5f, 2.f, 6.f,  1.f, 1.f },
			{ -3.5f, 2.f,  3.5f, 0.f, 1.f },
			{ -3.5f, 2.f, -5.f,  0.f, 1.f },
			{ -4.25f, 1.f, -10.65f,  0.f, 1.f },
			{  4.25f, 1.f, -10.65f,  0.f, 0.f },
			{  3.5f, 2.f, -5.f,  0.f, 0.f },
			{  3.5f, 2.f,  3.5f, 1.f, 0.f },

			{  3.f, 1.f, 7.f, 1.f, 1.f },
			{ -3.f, 1.f, 7.f, 1.f, 1.f },
			{ -5.f, 0.f, 4.f, 1.f, 0.f },
			{  5.f, 0.f, 4.f, 1.f, 0.f },

			{  4.5f, 0.f,  10.f, 0.f, 0.f },
			{ -4.5f, 0.f,  10.f, 0.f, 0.f },
			{ -5.f,  0.f, -10.f, 0.f, 0.f },
			{  5.f,  0.f, -10.f, 0.f, 0.f },

			{  4.f, -3.f,  7.f, 1.f, 1.f },
			{ -4.f, -3.f,  7.f, 0.f, 1.f },
			{ -4.f, -3.f, -8.f, 0.f, 0.f },
			{  4.f, -3.f, -8.f, 1.f, 0.f }
		};
		for (uv_vertex& uv : vertices) {
			uv.x *= scale; uv.y *= scale; uv.z *= scale;
			m_vertices.push_back(uv);
		}
	}
	virtual ~V150() {}
	virtual void get_mesh(std::vector<uv_vertex>& triangles, std::vector<int>& mesh_index) const {
		face(triangles, mesh_index, { 0, 1, 2, 3, 6, 7 });
		face(triangles, mesh_index, { 3, 4, 5, 6 });

		face(triangles, mesh_index, { 0, 8, 9, 1 });
		face(triangles, mesh_index, { 1, 9, 10, 2 });
		face(triangles, mesh_index, { 0, 7, 11, 8 });
		face(triangles, mesh_index, { 8, 11, 12 });
		face(triangles, mesh_index, { 12, 13, 9, 8 });
		face(triangles, mesh_index, { 9, 13, 10 });
												   		
		face(triangles, mesh_index, { 2, 10, 14, 4, 3 });
		face(triangles, mesh_index, { 7, 6, 5, 15, 11 });
		face(triangles, mesh_index, { 5, 4, 14, 15 });

		face(triangles, mesh_index, { 16, 17, 13, 12 });
		face(triangles, mesh_index, { 12, 15, 19, 16 });
		face(triangles, mesh_index, { 13, 17, 18, 14 });
		face(triangles, mesh_index, { 14, 18, 19, 15 });
		face(triangles, mesh_index, { 16, 19, 18, 17 });
	}
};

ShapeDesc* CreateGearShapeDesc(float radius, float half_thickness, int num_teeth, float tooth_half_width = 0.f);
