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
	// camera parameters all in world frame
	glm::vec3 m_pos;		 // position
	glm::vec3 m_up;			 // up direction
	glm::vec3 m_focal_point; // the focal point -> where the camera is looking at

	float m_yaw;			 // 180 to -180, 0 is in the +z direction; -90 is right, -180 is back, 90 is left
	float m_pitch;			 // 90 to -90; 90 is up and -90 is down
	float m_yaw_lo, m_yaw_hi;
	float m_pitch_lo, m_pitch_hi;
	
	// if not NULL, we are switching from the other camera
	const Camera* m_other_camera;

	glm::vec3 get_view_direction() const;

public:
	enum class Type {
		Drone,
		Body
	};
	Camera();
	virtual ~Camera() {}

	virtual void process_player_input(Controller& ctlr) = 0;
	virtual void update(const glm::mat4& player_trans) = 0;
	glm::mat4 get_view_matrix();

	// movement and viewing angle control
	void set_limits(float yaw_lo, float yaw_hi, float pitch_lo, float pitch_hi);
	virtual void turn(float yaw_delta, float pitch_delta);
	void switch_from(const Camera& other_camera) { m_other_camera = &other_camera; }
	
	// public access
	virtual void set_position(const glm::vec3 pos) { m_pos = pos; }
	glm::vec3 get_position() const { return m_pos; }
	glm::vec3 get_focal_point() const { return m_focal_point; }
	float get_yaw() const { return m_yaw; }
	float get_pitch() const { return m_pitch; }
};

// a camera attached to a rigid body
class BodyCam : public Camera
{
protected:
	int m_update;
	int m_max_update;
	int m_pos_index;
	std::vector<glm::vec3> m_pos_buffer;
	// position of camera in the followed body's local frame
	glm::vec3 m_camera_pos;

	virtual void turn(float yaw_delta, float pitch_delta);

public:
	BodyCam();
	virtual ~BodyCam() {}
	virtual void process_player_input(Controller& ctlr);
	virtual void update(const glm::mat4& player_trans);
	virtual void set_position(const glm::vec3& pos) { m_camera_pos = pos; }
};

// a camera that can move and look around
class DroneCam : public Camera
{
protected:
	float m_focal_length;	// how far the focal point is from the camera

	void move(const glm::vec3& delta);
	void move_forward(float distance);
	void side_step(float distance);

public:
	DroneCam() : m_focal_length(50.f) {}
	virtual ~DroneCam() {}
	
	virtual void process_player_input(Controller& ctlr);
	virtual void update(const glm::mat4& player_trans);
};
