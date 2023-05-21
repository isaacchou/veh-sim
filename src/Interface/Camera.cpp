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

//~~~
// Camera
//~~~
Camera::Camera()
{	
	m_pos = glm::vec3(0.0f, 10.0f, -60.0f);
	m_focal_point = glm::vec3(0.f, 0.f, 0.f);
	m_up = glm::vec3(0.f, 1.f, 0.f);
	
	// 180 to -180 but use -360..360 so it can fully turn around
	// 0 is in the +z direction; -90 is right, -180 is back, 90 is left
	m_yaw = 0.f;
	m_yaw_lo = -360.f;
	m_yaw_hi = 360.f;
	
	// 90 to -90; 90 is up and -90 is down
	m_pitch = 0.f;
	m_pitch_lo = -90.f;
	m_pitch_hi = 90.f;

	m_other_camera = NULL;
}

glm::mat4 Camera::get_view_matrix()
{
	debug_log_mute("camera pos: (%f, %f, %f) focal point: (%f, %f, %f)\n", 
		m_pos.x, m_pos.y, m_pos.z, m_focal_point.x, m_focal_point.y, m_focal_point.z);
	// up is always +y axis
	// camera position and focal point determine the view matrix
	return glm::lookAt(m_pos, m_focal_point, m_up);
}

void Camera::turn(float yaw_delta, float pitch_delta)
{
	m_yaw += yaw_delta;
	m_pitch += pitch_delta;
	while (m_yaw > 360.f) m_yaw -= 360.f;
	while (m_yaw < -360.f) m_yaw += 360.f;

	if (m_yaw > m_yaw_hi) m_yaw = m_yaw_hi;
	if (m_yaw < m_yaw_lo) m_yaw = m_yaw_lo;
	if (m_pitch > m_pitch_hi) m_pitch = m_pitch_hi;
	if (m_pitch < m_pitch_lo) m_pitch = m_pitch_lo;
}

void Camera::set_limits(float yaw_lo, float yaw_hi, float pitch_lo, float pitch_hi)
{
	m_yaw_lo = yaw_lo;
	m_yaw_hi = yaw_hi;
	m_pitch_lo = pitch_lo;
	m_pitch_hi = pitch_hi;
	turn(0.f, 0.f);
}

glm::vec3 Camera::get_view_direction() const
{	// a vector based on yaw and pitch
	float dir_z = glm::cos(glm::radians(m_yaw)) * glm::cos(glm::radians(m_pitch));
	float dir_x = glm::sin(glm::radians(m_yaw)) * glm::cos(glm::radians(m_pitch)) * -1.f;
	float dir_y = glm::sin(glm::radians(m_pitch));
	// a vector relative to the unit z-axis
	return glm::vec3(dir_x, dir_y, dir_z) - glm::vec3(0.f, 0.f, 1.f);
}

//~~~
// BodyCam:
// Has a fixed focual_length which is from the camera position to the center of the followed object
// The viewing direction is then added to the focal direction
//~~~
BodyCam::BodyCam() :
	m_camera_pos(0.f, 0.f, 0.f)
{
	m_update = 0;
	m_max_update = 300;
	m_pos_index = 0;
}

void BodyCam::turn(float yaw_delta, float pitch_delta)
{
	float cur_yaw = get_yaw(), cur_pitch = get_pitch();
	Camera::turn(yaw_delta, pitch_delta);
	if (cur_yaw != get_yaw() || cur_pitch != get_pitch()) {
		m_update = m_max_update;
	}
}

void BodyCam::process_player_input(Controller& ctlr)
{
	glm::vec2 pos = ctlr.get_cursor_movement();
	if (!ctlr.is_key_pressed(GLFW_KEY_LEFT_SHIFT) && 
		!ctlr.is_mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT)) {
		turn(pos.x * 0.015f, pos.y * -0.015f);
	}
	ctlr.get_scroll_movement();
}

void BodyCam::update(const glm::mat4& player_trans)
{	
	if (m_other_camera) {
		// nothing to transition from the otrher camera
		m_other_camera = NULL;
	}

	glm::vec3 pos = glm::vec3(player_trans * glm::vec4(m_camera_pos, 1.f));
	// stablize the camera by averaging the past positions
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

	float x = 0.f, y = 0.f, z = 0.f;
	m_pos = glm::vec3(0.f, 0.f, 0.f);
	for (auto& p : m_pos_buffer) {
		m_pos += p;
	}
	m_pos /= (float)n;

	// gradually move focal point to initial position
	if (m_update > 0) {
		float n = static_cast<float>(m_update - m_max_update) / static_cast<float>(m_max_update);
		Camera::turn(get_yaw() * n, get_pitch() * n);
		m_update -= 1;
	}
	// always look at the center of mass of the body (0.f, 0.f, 0.f)
	glm::vec3 dir = get_view_direction();
	// pointing from camera to origin (0.f, 0.f, 0.f)
	glm::vec3 focal_dir = glm::normalize(glm::vec3(0.f, 0.f, 0.f) - m_camera_pos);
	// plus tilt and turn
	focal_dir += dir;
	glm::vec3 focal_point = focal_dir * glm::length(m_camera_pos) + m_camera_pos;
	// transform to world frame
	m_focal_point = glm::vec3(player_trans * glm::vec4(focal_point, 1.f));
}

//~~~
// DroneCam
// Has a fixed focal length
// Viewing direction is added to the +z-axis to be the focal direction
//~~~
void DroneCam::move(const glm::vec3& delta)
{
	m_pos += delta;
}

void DroneCam::move_forward(float distance)
{
	glm::vec3 direction = m_focal_point - m_pos;
	move(glm::normalize(glm::vec3(direction.x, 0.f, direction.z)) * distance);
}

void DroneCam::side_step(float distance)
{
	glm::vec3 direction = m_focal_point - m_pos;
	move(glm::normalize(glm::cross(direction, m_up)) * distance);
}

void DroneCam::process_player_input(Controller& ctlr)
{	// camera movements
	float distance = 1.f;
	if (ctlr.is_key_pressed(GLFW_KEY_W)) move_forward(distance);
	else if (ctlr.is_key_pressed(GLFW_KEY_S)) move_forward(-distance);
	if (ctlr.is_key_pressed(GLFW_KEY_A)) side_step(-distance);
	else if (ctlr.is_key_pressed(GLFW_KEY_D)) side_step(distance);

	glm::vec2 pos = ctlr.get_cursor_movement();
	ctlr.get_scroll_movement();
	turn(pos.x * 0.02f, pos.y * -0.02f);
}

void DroneCam::update(const glm::mat4&)
{
	if (m_other_camera) {
		// Transition from BodyCam to DroneCam:
		// Keep the other focal point and position but maintain m_pos.y
		// recalc the focal length and view direction
		glm::vec3 pos = m_other_camera->get_position();
		glm::vec3 focal_point = m_other_camera->get_focal_point();
		// calc the new focal length
		m_pos = glm::vec3(pos.x, m_pos.y, pos.z);  // maintaining y position
		m_focal_length = glm::length(focal_point - m_pos);
		float pitch = glm::asin((focal_point.y - m_pos.y) / m_focal_length);
		
		glm::vec3 focal_dir = focal_point - pos;  // a vector pointing from camera to focal point
		focal_dir = glm::vec3(focal_dir.x, 0.f, focal_dir.z); // projection on x-z plane
		float len = glm::length(focal_dir);
		float yaw = glm::asin(-focal_dir.x / len);

		// calc camera pos
		float yaw_degree = glm::degrees(yaw);
		float pitch_degree = glm::degrees(pitch);

		if (focal_dir.z < 0.f) {
			yaw_degree = 180.f - yaw_degree;
			yaw = glm::radians(yaw_degree);
		}
		m_yaw = yaw_degree;
		m_pitch = pitch_degree;
		debug_log_mute("yaw = %f, pitch = %f\n", m_yaw, m_pitch);

		m_other_camera = NULL;
	} 		
	// initially looking at +z-axis
	m_focal_point = (glm::vec3(0.f, 0.f, 1.f) + get_view_direction()) * m_focal_length + m_pos;
}
