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

class OpenGLRenderer : public Renderer
{
protected:
	GLFWwindow* m_window;
	unsigned int m_shader_program;
	GLFWController m_controller;
	glm::mat4 m_player_trans; // transformation from player local to world frame

	BodyCam m_body_camera;
	DroneCam m_drone_camera;
	Camera* m_camera; // active camera

	int m_next_shape_id;
	std::map<int, const Shape*> m_shapes;
	std::map<int, glm::mat4> m_trans;
	std::map<int, int> m_texture_id_map; // maps shape texture id to OpenGL texture id

	unsigned int create_texture(size_t width, size_t height, unsigned char* data);

public:
	OpenGLRenderer() : m_window(NULL), m_shader_program(0), m_next_shape_id(0), m_camera(&m_drone_camera) {}
	virtual ~OpenGLRenderer() { 
		for (auto& s : m_shapes) delete s.second;
		teardown(); 
	}
	int init(const char* title);
	void teardown();
	
	void setup_camera(Camera::Type which, glm::vec3 pos);
	Shape* create_shape_from_desc(const ShapeDesc& desc);
	bool render(float elapsed_time);

	// Renderer responsibilities
	virtual int how_many_controllers() { return 1; }
	virtual Controller& get_controller(int) { return m_controller; }

	virtual void add_shape(int id, const ShapeDesc& shape_desc, const glm::mat4& trans);
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
