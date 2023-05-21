/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "TextureMaps.h"
#include "ShapeDesc.h"

class Shape
{
protected:
	unsigned int m_default_texture; // a (default) texture for all faces
	std::vector<unsigned int> m_textures;

	Shape() : m_default_texture(0) {}

public:
	virtual ~Shape() {}
	virtual void draw(unsigned int shader_program, const glm::mat4& trans) const = 0;
	virtual void set_texture(unsigned int texture);

	virtual void create_mesh() = 0;
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
	CompoundShape() {}
	virtual ~CompoundShape() {}
	virtual void draw(unsigned int shader_program, const glm::mat4& trans) const;
	virtual void set_texture(unsigned int texture);
	void add_child_shape(Shape* child, const glm::mat4& trans);
	const std::vector<child_shape>& get_child_shapes() { return m_child_shapes; }
	
	virtual void create_mesh();
};

class SimpleShape : public Shape
{
protected:
	unsigned int m_VAO;
	unsigned m_VBO;
	int m_num_vertices;
	std::vector<int> m_mesh_index;

	void setup_shape(uv_vertex* vertices, int num);

public:
	SimpleShape();
	virtual ~SimpleShape();
	virtual void add_texture(unsigned int texture, int repeat = 1) {
		while (repeat-- > 0) m_textures.push_back(texture); 
	}
	virtual void draw(unsigned int shader_program, const glm::mat4& trans) const;
};

class SphereShape : public SimpleShape
{
protected:
	float m_radius;

public:
	SphereShape(float radius)
	{
		m_radius = radius;
	}
	virtual ~SphereShape() {}

	virtual void create_mesh();
};

class CapsuleShape : public SimpleShape
{
protected:
	float m_radius;
	float m_height;

public:
	CapsuleShape(float radius, float height)
	{
		m_radius = radius;
		m_height = height;
	}
	virtual ~CapsuleShape() {}

	virtual void create_mesh();
};

class CylinderShape : public SimpleShape
{
protected:
	float m_radius;
	float m_half_height;

public:
	CylinderShape(float radius, float half_height)
	{
		m_radius = radius;
		m_half_height = half_height;
	}
	virtual ~CylinderShape() {}
	
	virtual void create_mesh();
};

class ConeShape : public SimpleShape
{
protected:
	float m_radius;
	float m_height;

public:
	ConeShape(float radius, float height)
	{
		m_radius = radius;
		m_height = height;
	}
	virtual ~ConeShape() {}

	virtual void create_mesh();
};

class BoxShape : public SimpleShape
{
protected:
	float m_cx;
	float m_cy;
	float m_cz;

public:
	BoxShape (float cx, float cy, float cz)
	{
		m_cx = cx;
		m_cy = cy;
		m_cz = cz;
	}
	virtual ~BoxShape() {}

	virtual void create_mesh();
};

class GroundShape : public SimpleShape
{	// can be a static plane or height field terrain shape
protected:
	float m_width;
	float m_length;

public:
	// static plane or height field terrain
	GroundShape(float width, float length)
	{
		m_width = width;
		m_length = length;
	}
	virtual ~GroundShape() {}

	virtual void create_mesh();
};

class ConvexShape : public SimpleShape
{
protected:
	std::vector<uv_vertex> m_triangles; // vertices that form triangles carrying texture information
	
public:
	ConvexShape(const ConvexShapeDesc& desc) {
		desc.get_mesh(m_triangles, m_mesh_index);
	}
	virtual ~ConvexShape() {}

	virtual void create_mesh() {
		setup_shape(m_triangles.data(), (int)m_triangles.size());
	}
};
