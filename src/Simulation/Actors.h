/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <tuple>
#include <vector>
#include "PhysicsWorld.h"

class Actor
{
protected:
	PhysicsWorld& m_world;

	virtual btRigidBody* get_connecting_body() = 0;
	virtual btVector3 get_connecting_point() = 0;

	virtual bool is_ready() = 0;

public:
	Actor(PhysicsWorld& world);
	virtual ~Actor() {}

	// pos in other body's local frame
	void attach(Actor& other);

	// pos in world frame
	virtual void create(const btVector3& pos, float scale = 1.f) = 0;
	virtual void update(float elapsed_time) = 0;
	virtual void process_player_input(Controller& ctlr) = 0;
	virtual const btRigidBody& body() = 0;
};

class Vehicle : public Actor
{
public:
	Vehicle(PhysicsWorld& world) : Actor(world) {}
	virtual ~Vehicle() {}

	// control interface
	virtual void accelarate(float torque = 5.f) = 0;
	virtual void brake() = 0;
	virtual void turn(float degrees) = 0;
	virtual void steer_left() = 0;
	virtual void steer_right() = 0;
	virtual void steer_center() = 0;
};

class Gun : public Actor
{
protected:
	btRigidBody* m_body;
	btRigidBody* m_bottom_base;
	btScalar m_base_half_height;
	btHingeConstraint* m_body_hinge; // hinge between gun body and base
	btHingeConstraint* m_base_hinge; // hinge_between top and bottom bases
	float m_barrel_radius;
	btRigidBody* m_shell;
	btVector3 m_mozzle;
	float m_time_since_last_shot;
	int m_max_bullets;
	std::vector<btRigidBody*> m_bullets;
	int m_projectile_texture;
	
	virtual btRigidBody* get_connecting_body() { return m_bottom_base; }
	virtual btVector3 get_connecting_point();

	virtual bool is_ready() { return m_body != NULL; }
	void fire_bullet();
	void fire_shell();

public:
	enum
	{
		Bullet = 0,
		Shell
	};
	Gun(PhysicsWorld& world);
	virtual ~Gun() {}

	virtual void create(const btVector3& pos, float scale = 1.f);
	virtual void update(float elapsed_time);
	virtual void process_player_input(Controller& ctlr);
	virtual const btRigidBody& body() { return *m_body; }
	
	void aim(float yaw_delta, float pitch_delta);
	void fire(int ammo_type = Bullet);
};

class Car : public Vehicle
{
protected:
	Gun m_gun;
	btRigidBody* m_car_body;
	btRigidBody* m_front_left_wheel;
	btRigidBody* m_front_right_wheel;
	btRigidBody* m_rear_left_wheel;
	btRigidBody* m_rear_right_wheel;
	btRigidBody* m_left_steer_box;
	btRigidBody* m_right_steer_box;

	btHingeConstraint* m_left_steer_hinge;
	btHingeConstraint* m_right_steer_hinge;

	std::tuple<btRigidBody*, btHingeConstraint*> 
	create_steer_box(float half_size, btRigidBody& car_body, const btVector3& pivot_in_car);
	
	btRigidBody* 
	create_wheel(float radius, float width, float spacing, btRigidBody& car_body, const btVector3& pivot_in_car);

	virtual btRigidBody* get_connecting_body() { return m_car_body; }
	virtual btVector3 get_connecting_point();
	
	virtual bool is_ready() { return m_car_body != NULL; }

public:
	Car(PhysicsWorld& world);
	virtual ~Car() {}

	virtual void create(const btVector3& pos, float sacle = 1.f);
	virtual void update(float elapsed_time);
	virtual void process_player_input(Controller& ctlr);

	// control interface
	virtual void accelarate(float torque = 5.f);
	virtual void brake();
	virtual void turn(float degrees);
	virtual void steer_left();
	virtual void steer_right();
	virtual void steer_center();

	virtual const btRigidBody& body() { return *m_car_body; }
};

class Tank : public Vehicle
{
protected:
	Gun m_gun;
	btRigidBody* m_tank_body;
	btRigidBody* m_gear[4];
	
	float m_max_velocity;
	int m_max_update;
	int m_update;

	virtual btRigidBody* get_connecting_body() { return m_tank_body; }
	virtual btVector3 get_connecting_point() { return btVector3(0.f, 0.75f, -1.f); }

	virtual bool is_ready() { return false; }

public:
	Tank(PhysicsWorld& world);
	virtual ~Tank() {}

	// control interface
	virtual void accelarate(float torque = 5.f);
	virtual void brake();
	virtual void turn(float torque);
	virtual void steer_left();
	virtual void steer_right();
	virtual void steer_center() {}

	virtual void create(const btVector3& pos, float scale = 1.f);
	virtual void update(float elapsed_time);
	virtual void process_player_input(Controller& ctlr);

	virtual const btRigidBody& body() { return *m_tank_body; }
};
