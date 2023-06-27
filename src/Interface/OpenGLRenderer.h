/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <map>
#include <glm/glm.hpp>
#include "Renderer.h"
#include "Camera.h"
#include "Controller.h"
#include "Shapes.h"

struct GLFWwindow;

class GLFWController : public Controller
{
public:
	GLFWController() {}
	virtual ~GLFWController() {}

	// callback handlers
	void register_callbacks(GLFWwindow* window);
	void process_mouse_move(double xpos, double ypos);
	void process_mouse_button(int button, int action, int mods);
	void process_mouse_wheel_scroll(double xoffset, double yoffset);
	void process_keyboard_input(int key, int scancode, int action, int mods);
};

class OpenGLShape
{
protected:
	struct child_shape {
		OpenGLShape* shape;
		glm::mat4 trans;
	};
	// OpenGL vertex Array Object and 
	unsigned int m_VAO;
	// OpenGL Vertex Buffer Object
	unsigned int m_VBO;
	
	int m_num_vertices;
	std::vector<int> m_face_index;
	std::vector<unsigned int> m_textures;
	unsigned int m_default_texture;
	std::vector<child_shape> m_child_shapes;

public:
	OpenGLShape(const std::vector<uv_vertex>& mesh, std::vector<int> face_index);
	// Compound shape constructor
	OpenGLShape() : m_VAO(0), m_VBO(0),
		m_num_vertices(0), m_default_texture(0) {}
	void add_child_shape(OpenGLShape* shape, glm::mat4 trans);
	void add_texture(unsigned int txtr) { m_textures.push_back(txtr); }
	void set_default_texture(unsigned int txtr) { m_default_texture = txtr; }
	virtual ~OpenGLShape();

	void draw(unsigned int shader_program, const glm::mat4& trans, const std::map<int, int>& texture_id_map) const;
	
	static OpenGLShape* from_json(const char* json, glm::mat4& trans);
};

class OpenGLRenderer : public Renderer
{
protected:
	GLFWwindow* m_window;
	unsigned int m_shader_program;
	GLFWController m_controller;
	glm::mat4 m_player_trans; // transformation from player local to world frame
	Camera m_camera;

	int m_next_shape_id;
	std::map<int, const OpenGLShape*> m_shapes;
	std::map<int, glm::mat4> m_trans;
	std::map<int, int> m_texture_id_map; // maps shape texture id to OpenGL texture id

	unsigned int create_texture(size_t width, size_t height, unsigned char* data);

public:
	OpenGLRenderer() : m_window(NULL), m_shader_program(0), m_next_shape_id(0), m_player_trans(1.f) {}
	virtual ~OpenGLRenderer() { 
		for (auto& s : m_shapes) delete s.second;
		teardown(); 
	}
	int init(const char* title);
	void teardown();
	
	void setup_camera(bool follow, const glm::vec3& eye, const glm::vec3& target);
	bool render(float elapsed_time);

	// Renderer responsibilities
	virtual int how_many_controllers() { return 1; }
	virtual Controller& get_controller(int) { return m_controller; }

	virtual void add_shape(int id, const char* json);
	virtual void update_shape(int id, const glm::mat4& trans);
	virtual void remove_shape(int id);
	virtual void add_texture(int id, size_t width, size_t height, unsigned char* data) {
		m_texture_id_map.insert({ id, create_texture(width, height, data) });
	}
	virtual void set_player_transform(int which, const glm::mat4& trans) { if (which == 0) m_player_trans = trans; }
	virtual void pre_connect() {}
	virtual void post_connect() {}
	virtual void begin_update() {}
	virtual bool end_update(float elapsed_time) { return render(elapsed_time); }
};
