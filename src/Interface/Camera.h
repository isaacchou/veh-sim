/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "Controller.h"

class Camera
{
protected:
	bool m_follow; // is the camera following the player?
	// camera parameters all in world frame
	glm::vec3 m_pos;		 // position
	glm::vec3 m_up;			 // up direction
	glm::vec3 m_focal_point; // the focal point -> where the camera is looking at
	float m_focal_length;	 // how far the focal point is from the camera
	// yaw is the angle of rotation around the vertical (+y) axis
	// measured in degrees with range (-360, 360)
	//	* 0 is in the +z direction (reference direction)
	//	* 90 is left, 180 is back, -90 is right
	// pitch is the angle of rotation around the side-to-side (+x) axis
	// measured in degrees with range (-90, 90)
	//	* 90 is up and -90 is down
	float m_yaw, m_pitch;
	// yaw wraps around while pitch stops at ends of range
	inline float check_yaw_range(float yaw) {
		while (yaw > 360.f) yaw -= 360.f;
		while (yaw < -360.f) yaw += 360.f;
		return yaw;
	}
	inline float check_pitch_range(float pitch) { return glm::clamp(pitch, -90.f, 90.f); }
	inline float calc_yaw_from_vec(const glm::vec3& dir) { return check_yaw_range(glm::degrees(glm::atan(-dir.x, dir.z))); }
	inline float calc_pitch_from_vec(const glm::vec3& dir) { return check_pitch_range(glm::degrees(glm::asin(dir.y))); }
	// view direction is a unit (normalized) vector in world frame
	glm::vec3 get_view_direction() const;
	// movement and viewing angle control
	void turn(float yaw_delta, float pitch_delta);
	void move(const glm::vec3& delta);
	void move_forward(float distance);
	void side_step(float distance);

	// these are used when following the player
	// position of camera in the followed body's local frame
	glm::vec3 m_camera_pos;
	float m_resting_yaw, m_resting_pitch;
	const int m_max_update;
	int m_update, m_pos_index;
	std::vector<glm::vec3> m_pos_buffer;

public:
	Camera();
	virtual ~Camera() {}
	void setup(bool follow, const glm::vec3& eye, const glm::vec3& target);
	void process_player_input(Controller& ctlr);
	void update(const glm::mat4& player_trans);
	glm::mat4 get_view_matrix();
};
