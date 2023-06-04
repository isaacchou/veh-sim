/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include "Camera.h"
#include "../Utils.h"

Camera::Camera() : m_max_update(15)
{	
	m_up = glm::vec3(0.f, 1.f, 0.f);
	setup(false, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));

	m_camera_pos = glm::vec3(0.f, 0.f, 0.f);
	m_update = 0;
	m_pos_index = 0;
	m_resting_yaw = m_yaw;
	m_resting_pitch = m_pitch;
}

void Camera::setup(bool follow, const glm::vec3& eye, const glm::vec3& target)
{
	m_pos = eye;
	m_camera_pos = eye;
	m_focal_point = target;
	if (m_focal_point == m_pos) {
		m_focal_point += glm::vec3(0.f, 0.f, 1.f);
	}
	m_follow = follow;
	// when following the player, eye and target are in player's local frame
	// m_pos is temporarily incorrect at this point until the first update event 
	// when the player transorm is known; however (m_focal_point - m_pos) is usable
	m_focal_length = glm::length(m_focal_point - m_pos);
	glm::vec3 dir = glm::normalize(m_focal_point - m_pos);
	m_yaw = calc_yaw_from_vec(dir);
	m_pitch = calc_pitch_from_vec(dir);
	// remeber the resting (initial) yaw and pitch so we can move camera 
	// back to the starting viewing direction when following the player
	m_resting_yaw = m_yaw;
	m_resting_pitch = m_pitch;
}

glm::mat4 Camera::get_view_matrix()
{	// up is always +y axis
	// camera position and focal point determine the view matrix
	return glm::lookAt(m_pos, m_focal_point, m_up);
}

void Camera::turn(float yaw_delta, float pitch_delta)
{
	float cur_yaw = m_yaw, cur_pitch = m_pitch;
	m_yaw = check_yaw_range(m_yaw + yaw_delta);
	m_pitch = check_pitch_range(m_pitch + pitch_delta);
	if (cur_yaw != m_yaw || cur_pitch != m_pitch) {
		// on change start the update cycle
		m_update = m_max_update;
	}
}

void Camera::move(const glm::vec3& delta)
{
	m_pos += delta;
}

void Camera::move_forward(float distance)
{
	glm::vec3 direction = m_focal_point - m_pos;
	move(glm::normalize(glm::vec3(direction.x, 0.f, direction.z)) * distance);
}

void Camera::side_step(float distance)
{
	glm::vec3 direction = m_focal_point - m_pos;
	move(glm::normalize(glm::cross(direction, m_up)) * distance);
}

glm::vec3 Camera::get_view_direction() const
{	// a vector based on yaw and pitch
	float yaw = glm::radians(m_yaw), pitch = glm::radians(m_pitch);
	float dir_z = glm::cos(yaw) * glm::cos(pitch);
	float dir_x = glm::sin(yaw) * glm::cos(pitch) * -1.f;
	float dir_y = glm::sin(pitch);
	return glm::vec3(dir_x, dir_y, dir_z);
}

void Camera::process_player_input(Controller& ctlr)
{
	if (!m_follow) {
		// camera movements
		float distance = 1.f;
		if (ctlr.is_key_pressed(GLFW_KEY_W)) move_forward(distance);
		else if (ctlr.is_key_pressed(GLFW_KEY_S)) move_forward(-distance);
		if (ctlr.is_key_pressed(GLFW_KEY_A)) side_step(-distance);
		else if (ctlr.is_key_pressed(GLFW_KEY_D)) side_step(distance);
		if (ctlr.is_key_pressed(GLFW_KEY_PAGE_UP)) m_pos.y += 0.5f * distance;
		else if (ctlr.is_key_pressed(GLFW_KEY_PAGE_DOWN)) m_pos.y -= 0.5f * distance;
	}

	glm::vec2 pos = ctlr.get_cursor_movement();
	if (!ctlr.is_key_pressed(GLFW_KEY_LEFT_SHIFT) && 
		!ctlr.is_mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT)) {
		float step = 0.015f;
		turn(pos.x * step, pos.y * -step);
	}
	ctlr.get_scroll_movement();
}

void Camera::update(const glm::mat4& player_trans)
{	
	if (!m_follow) {
		m_focal_point = get_view_direction() * m_focal_length + m_pos;
		return;
	}	
	// At rest, the camera will be at m_camera_pos in player's frame 
	// looking at the center (0, 0, 0)
	// transform the local camera position to world frame
	glm::vec3 pos = glm::vec3(player_trans * glm::vec4(m_camera_pos, 1.f));
	// camera stablization is provided to counter vibration of the vehicle
	// it works by averaging the past positions
	const int num_pos = 90; // ~1.5 second
	m_pos_index %= num_pos;
	int n = (int)m_pos_buffer.size();
	if (n < num_pos) {
		m_pos_buffer.push_back(pos);
		n += 1;
	} else {
		m_pos_buffer[m_pos_index] = pos;
	}
	m_pos_index += 1;

	m_pos = glm::vec3(0.f, 0.f, 0.f);
	for (auto& p : m_pos_buffer) {
		m_pos += p;
	}
	m_pos /= (float)n;

	// gradually move focal point to initial position
	// only re-point the camera when following the player
	if (m_update > 0) {
		float n = static_cast<float>(m_update - m_max_update) / static_cast<float>(m_max_update);
		turn((m_yaw - m_resting_yaw) * n, (m_pitch - m_resting_pitch) * n);
		m_update -= 1;
	}
	glm::vec3 focal_point = get_view_direction() * glm::length(m_camera_pos) + m_camera_pos;
	// transform to world frame
	m_focal_point = glm::vec3(player_trans * glm::vec4(focal_point, 1.f));
}
