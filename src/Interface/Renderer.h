/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <glm/glm.hpp>

class ShapeDesc;
class Controller;

// Pure abstract class/interface
class Renderer
{
public:	
	virtual int how_many_controllers() = 0;
	virtual Controller& get_controller(int which) = 0;
	virtual void set_player_transform(int which, const glm::mat4& trans) = 0;
	virtual void setup_camera(bool follow, const glm::vec3& eye, const glm::vec3& target) = 0;

	virtual void add_shape(int id, const ShapeDesc& shape_desc, const glm::mat4& trans) = 0;
	virtual void update_shape(int id, const glm::mat4& trans) = 0;
	virtual void remove_shape(int id) = 0;
	virtual void add_texture(int id, size_t width, size_t height, unsigned char* data) = 0;
	virtual void pre_connect() = 0;
	virtual void post_connect() = 0;
	virtual void begin_update() = 0;
	virtual bool end_update(float elapsed_time) = 0; // return true to continue
};
