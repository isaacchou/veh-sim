/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "TextureMaps.h"

struct uv_vertex
{
	float x;
	float y;
	float z;
	float texture_x;
	float texture_y;
};

class Shape
{
public:
	enum class Type {
		Ground,
		Box,
		Sphere,
		Cylinder,
		Cone,
		Capsule,
		Convex,
		Compound
	};
	virtual ~Shape() {}
	virtual void set_texture(unsigned int texture) { m_default_texture = texture; }
	virtual void add_texture(unsigned int texture, int repeat = 1) {
		while (repeat-- > 0) m_textures.push_back(texture); 
	}
	
	unsigned int get_default_texture() const { return m_default_texture; }
	Type get_type() const { return m_type; }
	const float* param() const { return m_param; }	
	const std::vector<uv_vertex>& mesh() const { return m_mesh; }
	const std::vector<int>& face_index() const { return m_face_index; }
	virtual std::string to_json(const glm::mat4& trans) const;

protected:
	Type m_type;
	float m_param[4];	
	// mesh is the sequence of vertices that form triangles of the shape
	// plus the texture coordinates
	std::vector<uv_vertex> m_mesh;
	// indices into the mesh that mark beginning of a face of the shape
	// each face of the shape gets its own texture map
	std::vector<int> m_face_index;

	unsigned int m_default_texture; // a (default) texture for all faces
	std::vector<unsigned int> m_textures;

	Shape() : m_param{0.f}, m_default_texture(0) {}
};

class CompoundShape : public Shape
{
public:
	struct child_shape {
		Shape* shape;
		glm::mat4 trans;
	};

protected:	
	std::vector<child_shape> m_child_shapes;

public:
	CompoundShape() { m_type = Type::Compound; }
	virtual ~CompoundShape() {}
	virtual void set_texture(unsigned int texture);
	void add_child_shape(Shape* child, const glm::mat4& trans);
	void add_child_shape(Shape* child, const glm::vec3& origin, const glm::vec3& rotation, float angle);
	const std::vector<child_shape>& get_child_shapes() const { return m_child_shapes; }
	virtual std::string to_json(const glm::mat4& trans) const;
};

class SphereShape : public Shape
{
protected:
	float& m_radius;

public:
	SphereShape(float radius) : m_radius(m_param[0])
	{
		m_type = Type::Sphere;
		m_radius = radius;
		create_mesh();
	}
	virtual ~SphereShape() {}
	virtual void create_mesh();
	float radius() const { return m_radius; }
};

class CapsuleShape : public Shape
{
protected:
	float& m_radius;
	float& m_height;
	void create_mesh();

public:
	CapsuleShape(float radius, float height) : 
		m_radius(m_param[0]), m_height(m_param[1])
	{
		m_type = Type::Capsule;
		m_radius = radius;
		m_height = height;
		create_mesh();
	}
	virtual ~CapsuleShape() {}
	float radius() const { return m_radius; }
	float height() const { return m_height; }
};

class CylinderShape : public Shape
{
protected:
	float& m_radius;
	float& m_half_height;
	void create_mesh();

public:
	CylinderShape(float radius, float half_height) :
		m_radius(m_param[0]), m_half_height(m_param[1])
	{
		m_type = Type::Cylinder;
		m_radius = radius;
		m_half_height = half_height;
		create_mesh();
	}
	virtual ~CylinderShape() {}
};

class ConeShape : public Shape
{
protected:
	float& m_radius;
	float& m_height;
	void create_mesh();

public:
	ConeShape(float radius, float height) :
		m_radius(m_param[0]), m_height(m_param[1])
	{
		m_type = Type::Cone;
		m_radius = radius;
		m_height = height;
		create_mesh();
	}
	virtual ~ConeShape() {}
};

class BoxShape : public Shape
{
protected:
	float& m_cx;
	float& m_cy;
	float& m_cz;
	void create_mesh();

public:
	BoxShape (float cx, float cy, float cz) :
		m_cx(m_param[0]), m_cy(m_param[1]), m_cz(m_param[2])
	{
		m_type = Type::Box;
		m_cx = cx;
		m_cy = cy;
		m_cz = cz;
		create_mesh();
	}
	virtual ~BoxShape() {}
};

class GroundShape : public Shape
{	// can be a static plane or height field terrain shape
protected:
	float& m_width;
	float& m_length;
	void create_mesh();

public:
	// static plane or height field terrain
	GroundShape(float width, float length) :
		m_width(m_param[0]), m_length(m_param[1])
	{
		m_type = Type::Ground;
		m_width = width;
		m_length = length;
		create_mesh();
	}
	virtual ~GroundShape() {}
};

class ConvexShape : public Shape
{
protected:
	// vertices are the points that define the shape
	std::vector<uv_vertex> m_vertices;	
	ConvexShape() { m_type = Type::Convex; }
	virtual ~ConvexShape() {}

public:
	const std::vector<uv_vertex>& get_vertices() const { return m_vertices; }
};

class PyramidShape: public ConvexShape
{
public:
	PyramidShape(float cx, float cy, float cz) 
	{
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
		// create mesh
		int v[] = { 
			// side faces (0 - 11)
			0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1, 
			// bottom face (12 - 17)
			1, 4, 3, 3, 2, 1 };
		for (int i : v) {
			m_mesh.push_back(m_vertices[i]);
		}
		m_face_index.push_back(0);  // all side faces mapped to one texture
		m_face_index.push_back(12); // bottom face has its own texture
	}
	virtual ~PyramidShape() {}
};

class WedgeShapeDesc : public ConvexShape
{
public:
	WedgeShapeDesc(float cx, float cy, float cz, float half_length)
	{
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
		
		// create mesh
		int v[] = { 
			// face #1: front and back roofs (0 - 11)
			0, 2, 1, 1, 2, 3, 0, 1, 5, 1, 4, 5, 
			// face #2: left and right gables (12 - 17)
			0, 5, 2, 1, 3, 4,
			// face #3: bottom (18 - 23)
			2, 4, 3, 2, 5, 4 };
		for (int i : v) {
			m_mesh.push_back(m_vertices[i]);
		}
		// for the gables, the tip vertices are at the mid-point of the texture
		m_mesh[12].texture_y = .5f;
		m_mesh[15].texture_y = .5f;
		
		m_face_index.push_back(0);  // front and back roofs
		m_face_index.push_back(12); // gables
		m_face_index.push_back(18); // bottom face
	}
	virtual ~WedgeShapeDesc() {}
};

class V150 : public ConvexShape
{
protected:
	void face(std::vector<int> v) {
		m_face_index.push_back((int)m_mesh.size());
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
		for (int i = 0; i < n; i++) m_mesh.push_back(m_vertices[v[t[n - 1 - i]]]);
	}

public:
	V150(float scale) {
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
		face({ 0, 1, 2, 3, 6, 7 });
		face({ 3, 4, 5, 6 });

		face({ 0, 8, 9, 1 });
		face({ 1, 9, 10, 2 });
		face({ 0, 7, 11, 8 });
		face({ 8, 11, 12 });
		face({ 12, 13, 9, 8 });
		face({ 9, 13, 10 });
												   		
		face({ 2, 10, 14, 4, 3 });
		face({ 7, 6, 5, 15, 11 });
		face({ 5, 4, 14, 15 });

		face({ 16, 17, 13, 12 });
		face({ 12, 15, 19, 16 });
		face({ 13, 17, 18, 14 });
		face({ 14, 18, 19, 15 });
		face({ 16, 19, 18, 17 });
	}
	virtual ~V150() {}
};

Shape* CreateGearShape(float radius, float half_thickness, int num_teeth, float tooth_half_width = 0.f);
