/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <unordered_set>
#include <glm/glm.hpp>

class Controller
{
public:
	std::unordered_set<int> m_keyboard;
	std::unordered_set<int> m_mouse;
	glm::vec2 m_cursor_cur_pos;
	glm::vec2 m_cursor_last_pos;
	glm::vec2 m_scroll_pos;

	Controller();
	virtual ~Controller() {}

	void sync_from(Controller& other);

	// control interface
	bool is_key_pressed(int key);
	bool is_mouse_button_pressed(int key);
	glm::vec2 get_cursor_movement();
	glm::vec2 get_scroll_movement();
};
