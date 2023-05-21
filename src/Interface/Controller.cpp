/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include "Controller.h"

Controller::Controller()
{
	m_cursor_cur_pos = { 0.f, 0.f };
	m_cursor_last_pos = { 0.f, 0.f };
	m_scroll_pos = { 0.f, 0.f };
}

void Controller::sync_from(Controller& other)
{	// basically an operator=
	m_cursor_cur_pos = other.m_cursor_cur_pos;
	m_cursor_last_pos = other.m_cursor_last_pos;
	m_scroll_pos = other.m_scroll_pos;
		
	m_keyboard.clear();
	for (auto key : other.m_keyboard) {
		m_keyboard.insert(key);
	}
	
	m_mouse.clear();
	for (auto button : other.m_mouse) {
		m_mouse.insert(button);
	}
}

bool Controller::is_key_pressed(int key)
{
	return m_keyboard.find(key) != m_keyboard.end();
}

bool Controller::is_mouse_button_pressed(int key)
{	
	return m_mouse.find(key) != m_mouse.end();
}

glm::vec2 Controller::get_cursor_movement()
{
	glm::vec2 movement = m_cursor_cur_pos - m_cursor_last_pos;
	m_cursor_last_pos = m_cursor_cur_pos;
	return movement;
}

glm::vec2 Controller::get_scroll_movement()
{
	glm::vec2 movement = m_scroll_pos;
	m_scroll_pos = { 0.f, 0.f };
	return movement;
}
