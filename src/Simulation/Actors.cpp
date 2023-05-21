/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>
#include "Actors.h"
#include "../Interface/ShapeDesc.h"
#include "../Interface/TextureMaps.h"
#include "../Utils.h"

void log_obj_rotation(const char* obj_name, const btRigidBody& obj)
{
	const btTransform& trans = obj.getWorldTransform();
	btQuaternion rotation = trans.getRotation();
	btVector3 axis = rotation.getAxis();
	btScalar angle = rotation.getAngle();
	debug_log("[%s] axis: (%f, %f, %f) angle: %f\n", obj_name, axis.x(), axis.y(), axis.z(), glm::degrees(angle));
}

static void apply_torque_impulse(btRigidBody& obj, const btVector3& torque)
{	// the input velocity is in object local frame
	btTransform trans = obj.getCenterOfMassTransform();
	btQuaternion rotation = trans.getRotation();
	btVector3 axis = rotation.getAxis();
	btScalar angle = rotation.getAngle();
	
	obj.activate(true);
	obj.applyTorqueImpulse(torque.rotate(axis, angle));
}

static btVector3 get_angular_velocity_local(btRigidBody& obj)
{	// the input velocity is in object local frame
	btTransform trans = obj.getWorldTransform();
	return trans.getBasis().transpose() * obj.getAngularVelocity();
}

Actor::Actor(PhysicsWorld& world) : m_world(world)
{
	m_world.add_actor(*this);
}

void Actor::attach(Actor& other)
{
	btRigidBody* a = get_connecting_body();
	btRigidBody* b = other.get_connecting_body();

	btTransform trans_a, trans_b;
	trans_a.setIdentity();
	trans_a.setOrigin(get_connecting_point());
	trans_b.setIdentity();
	trans_b.setOrigin(other.get_connecting_point());
	btFixedConstraint* contact = new btFixedConstraint(*a, *b, trans_a, trans_b);
	m_world.addConstraint(contact, true);
}

Car::Car(PhysicsWorld& world) : Vehicle(world), m_gun(world)
{
	m_car_body = NULL;
	m_front_left_wheel = NULL;
	m_front_right_wheel = NULL;
	m_rear_left_wheel = NULL;
	m_rear_right_wheel = NULL;
	m_left_steer_box = NULL;
	m_right_steer_box = NULL;
	m_left_steer_hinge = NULL;
	m_right_steer_hinge = NULL;
}

void Car::accelarate(float torque)
{
	if (torque == 0.f || !is_ready()) return;

	apply_torque_impulse(*m_front_left_wheel, btVector3(0.f, -torque * 0.4f, 0.f));
	apply_torque_impulse(*m_front_right_wheel, btVector3(0.f, torque * 0.4f, 0.f));
	apply_torque_impulse(*m_rear_left_wheel, btVector3(0.f, -torque * 0.6f, 0.f));
	apply_torque_impulse(*m_rear_right_wheel, btVector3(0.f, torque * 0.6f, 0.f));
}

void Car::brake()
{
	if (!is_ready()) return;

	m_front_left_wheel->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
	m_front_right_wheel->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
	m_rear_left_wheel->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
	m_rear_right_wheel->setAngularVelocity(btVector3(0.f, 0.f, 0.f));
}

void Car::turn(float degrees)
{
	if (!is_ready()) return;
	
	btScalar left_ang_lo = glm::degrees(m_left_steer_hinge->getLowerLimit());
	float angle = left_ang_lo + degrees;
	if (angle < -30.f) angle = -30.f;
	else if (angle > 30.f) angle = 30.f;

	if (angle != left_ang_lo) {
		m_left_steer_box->activate(true);
		m_right_steer_box->activate(true);
		
		angle = glm::radians(angle);
		m_left_steer_hinge->setLimit(angle, angle);
		m_right_steer_hinge->setLimit(angle, angle);
	}
}

void Car::steer_left()
{
	turn(-1.f);
}

void Car::steer_right()
{
	turn(1.f);
}

void Car::steer_center()
{
	if (!is_ready()) return;

	m_left_steer_box->activate(true);
	m_right_steer_box->activate(true);
	m_left_steer_hinge->setLimit(0.f, 0.f);
	m_right_steer_hinge->setLimit(0.f, 0.f);
}

void Car::update(float elapsed_time)
{
}

void Car::process_player_input(Controller& ctlr)
{	// car movements
	if (ctlr.is_key_pressed(GLFW_KEY_LEFT)) steer_left();
	else if (ctlr.is_key_pressed(GLFW_KEY_RIGHT)) steer_right();
	else if (ctlr.is_key_pressed(GLFW_KEY_END)) steer_center();
	
	if (ctlr.is_key_pressed(GLFW_KEY_UP)) accelarate(5.f);
	else if (ctlr.is_key_pressed(GLFW_KEY_DOWN)) accelarate(-5.f);
	else if (ctlr.is_key_pressed(GLFW_KEY_SPACE)) brake();
	
	m_gun.process_player_input(ctlr);
}

//~~~
// About btHingeConstraint:
// pivotInA and pivotInB are the points of contact in local frames
// i.e. the same world point expressed in frame A and B
// axisInA and axisInB are the axes in local frames that can rotate
// the two objects will turn so that the two axes will align
//~~~

std::tuple<btRigidBody*, btHingeConstraint*> 
Car::create_steer_box(float box_size, btRigidBody& car_body, const btVector3& pivot_in_car)
{
	unsigned texture = 0; // for now
	btTransform trans = car_body.getCenterOfMassTransform();
	btVector3 pivot_in_box(0.f, -box_size, 0.f);
	ShapeDesc* shape = new CylinderShapeDesc (box_size, box_size);
	shape->add_texture(m_world.get_texture_map().solid_color(Color(45, 45, 45)), 6);
	btRigidBody* box = m_world.createRigidBody(shape,
											   trans(pivot_in_car) - pivot_in_box,
											   btQuaternion(btVector3(0.f, 0.f, 1.f), 0.f), 1.0);
	btHingeConstraint* hinge = new btHingeConstraint(car_body, *box, pivot_in_car, pivot_in_box,
													 btVector3(0.f, 1.f, 0.f), btVector3(0.f, 1.f, 0.f), false);
	hinge->setLimit(glm::radians(0.f), glm::radians(0.f));
	m_world.addConstraint(hinge, true);
	return std::make_tuple(box, hinge);
}

btRigidBody* Car::create_wheel(float radius, float width, float spacing, btRigidBody& car_body, const btVector3& pivot_in_car)
{	// assuming the car center is at x=0, z= 0
	bool left = pivot_in_car.x() > 0;
	btTransform trans = car_body.getCenterOfMassTransform();
	float pivot_in_wheel = width + spacing;
	
	ShapeDesc* cylinder = new CylinderShapeDesc (radius, width);
	cylinder->add_texture(m_world.get_texture_map().solid_color(Color(20, 20, 20)));	// inside face 
	cylinder->add_texture(m_world.get_texture_map().solid_color(Color(30, 30, 30)));
	cylinder->add_texture(m_world.get_texture_map().solid_color(Color(50, 50, 50)));	// outside face
	btRigidBody* wheel = m_world.createRigidBody(cylinder,
												 trans(pivot_in_car) + btVector3(left ? pivot_in_wheel : -pivot_in_wheel, 0.f, 0.f),
												 btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(left ? 90.f : -90.f)),
												 10.f);
	btHingeConstraint* hinge; // untracked hinges between the wheels and body/steer boxes
	hinge = new btHingeConstraint(car_body, *wheel, pivot_in_car, btVector3(0.f, pivot_in_wheel, 0.f),
								  btVector3(left ? -1.f : 1.f, 0.f, 0.f), btVector3(0.f, 1.f, 0.f), false);
	m_world.addConstraint(hinge, true);
	return wheel;
}

btVector3 Car::get_connecting_point()
{	// at the surface center of the car body
	return btVector3(0.f, 1.2f, 0.f);
}

void Car::create(const btVector3& pos, float)
{	// car dimensions: W=6, L=12, H=3
	float car_body_weight = 10.f;
	float wheel_weight = 5.f;

	float car_half_width = 3.f;
	float car_half_thickness = 2.f;
	float car_half_length = 6.f;
	float wheel_distance = 2.55f;	// distance between a wheel and center of car body: wheel_distance < car_half_length
	float steer_box_size = 0.21f;	// half size
	// bottom of car has to be above ground, so the following has to be true:
	// wheel_radius > (steer_box_size + 2 * car_half_thickness)
	float wheel_radius = 1.4f;
	float wheel_width = .5;			// half width
	float wheel_spacing = .07f;

	// car chassis
	unsigned int texture = 0;
	ShapeDesc* shape = new V150(0.6f);
	shape->set_texture(m_world.get_texture_map().solid_color(Color(60, 60, 60)));
	m_car_body = m_world.createRigidBody(shape,
										 btVector3(0.0f, wheel_radius - (car_half_thickness + steer_box_size), 0.0f) + pos,
										 btQuaternion(btVector3(0.f, 0.f, 1.f), 0.f), car_body_weight);
	btVector3 pivot_in_car(car_half_width - steer_box_size, -car_half_thickness, wheel_distance);
	std::tie(m_left_steer_box, m_left_steer_hinge) = create_steer_box(steer_box_size, *m_car_body, pivot_in_car);
	pivot_in_car.setX(-pivot_in_car.x()); // the opposite side
	std::tie(m_right_steer_box, m_right_steer_hinge) = create_steer_box(steer_box_size, *m_car_body, pivot_in_car);
	
	// front left wheel
	pivot_in_car = btVector3(steer_box_size, 0.f, 0.f); // connected to the steer box
	m_front_left_wheel = create_wheel(wheel_radius, wheel_width, wheel_spacing, *m_left_steer_box, pivot_in_car);

	// front right wheel
	pivot_in_car = btVector3(-steer_box_size, 0.f, 0.f); // connected to the steer box
	m_front_right_wheel = create_wheel(wheel_radius, wheel_width, wheel_spacing, *m_right_steer_box, pivot_in_car);

	// rear left wheel
	pivot_in_car = btVector3(car_half_width, -car_half_thickness + steer_box_size, -wheel_distance);
	m_rear_left_wheel = create_wheel(wheel_radius, wheel_width, wheel_spacing, *m_car_body, pivot_in_car);

	// rear right wheel
	pivot_in_car = btVector3(-car_half_width, -car_half_thickness + steer_box_size, -wheel_distance);
	m_rear_right_wheel = create_wheel(wheel_radius, wheel_width, wheel_spacing, *m_car_body, pivot_in_car);

	// create the gun turret
	m_gun.create(pos + get_connecting_point(), 0.7f);
	attach(m_gun);
}

Tank::Tank(PhysicsWorld& world) : Vehicle(world), m_gun(world), m_gear{0}
{
	m_tank_body = NULL;
	m_max_update = 120; // will be adjusted to frame rate in the update loop
	m_update = 0;
}

void Tank::accelarate(float torque)
{
	int i = 0;
	for (float x : {1.f, -1.f}) {
		for (float z : {1.f, -1.f}) {
			if (get_angular_velocity_local(*m_gear[i]).length() < 10.f) {
				apply_torque_impulse(*m_gear[i], btVector3(0.f, -x * torque * 0.15f, 0.f));
			}
			i += 1;
		}
	}
	m_update = m_max_update;
}

void Tank::brake()
{
	m_update = 0;
}

void Tank::turn(float torque)
{
	btVector3 f = btVector3(0.f, torque * 0.15f, 0.f);
	for (btRigidBody* gear : m_gear) {
		btVector3 v = get_angular_velocity_local(*gear);
		if (v.length() < 10.f || v.dot(f) < 0.f) {
			apply_torque_impulse(*gear, f);
		}
	}
	m_update = m_max_update;
}

void Tank::steer_left()
{
	turn(8.f);
}

void Tank::steer_right()
{
	turn(-8.f);
}

void Tank::update(float elapsed_time)
{	// counter the tension in the tracks to stablize the tank
	float fps = 1.f / elapsed_time;
	if (m_update > m_max_update) m_update = m_max_update;
	for (btRigidBody* gear : m_gear) {
		btVector3 v = get_angular_velocity_local(*gear);
		float n = -2.5f * ((float)(m_max_update - m_update) / (float)m_max_update);
		btVector3 torque = n * v;
		// prevent over compensating
		float f = torque.length();
		if (f > 2.5f) torque *= (2.5f / f);
		apply_torque_impulse(*gear, torque);
		debug_log_mute("fps = %f, m_update = %d, n = %f, v = (%f, %f, %f)\n", fps, m_update, n, v.x(), v.y(), v.z());
	}
	m_max_update = static_cast<int>(2.f * fps);
	if (m_max_update < 10) m_max_update = 10;
	if (m_update > 0) m_update -= 1;
}

void Tank::process_player_input(Controller& ctlr)
{
	if (ctlr.is_key_pressed(GLFW_KEY_LEFT) || ctlr.is_key_pressed(GLFW_KEY_A)) steer_left();
	else if (ctlr.is_key_pressed(GLFW_KEY_RIGHT) || ctlr.is_key_pressed(GLFW_KEY_D)) steer_right();
	
	if (ctlr.is_key_pressed(GLFW_KEY_UP) || ctlr.is_key_pressed(GLFW_KEY_W)) accelarate(5.f);
	else if (ctlr.is_key_pressed(GLFW_KEY_DOWN) || ctlr.is_key_pressed(GLFW_KEY_S)) accelarate(-5.f);
	else if (ctlr.is_key_pressed(GLFW_KEY_SPACE)) brake();

	m_gun.process_player_input(ctlr);
}

void Tank::create(const btVector3& pos, float)
{	// Tank dimension: 6 x 10 x 1.5
	float body_width = 3.f;	  // half width
	float body_height = .75f; // half height
	float body_length = 5.f;  // half length
	BoxShapeDesc* body_shape = new BoxShapeDesc(body_width, body_height, body_length);
	body_shape->add_texture(m_world.get_texture_map().diagonal_stripes(160, 32, 2, Color("gold"), Color("black")));
	body_shape->set_texture(m_world.get_texture_map().solid_color("#505050"));
	m_tank_body = m_world.createRigidBody(body_shape, pos, btQuaternion(0.f, 0.f, 0.f), 10.f);

	float gear_pos = 4.f - 0.3348078f;
	float spacing = 0.5f;
	float gear_thickness = 1.0f;
	float gear_radius = 1.f;
	int num_teeth = 6;
	float tooth_half_width = 0.08f;
	int i = 0;
	for (float x : {1.f, -1.f}) {
		for (float z : {1.f, -1.f}) {
			glm::mat4 trans(1.f);
			
			CompoundShapeDesc* gear_shape = new CompoundShapeDesc();
			CylinderShapeDesc* axle = new CylinderShapeDesc(gear_radius * 0.5f, gear_thickness);
			gear_shape->add_child_shape_desc(axle, trans);

			ShapeDesc* guard_disk = new CylinderShapeDesc(gear_radius + tooth_half_width * 3.f, tooth_half_width);
			trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, gear_thickness + tooth_half_width, 0.f));
			gear_shape->add_child_shape_desc(guard_disk, trans);
			ShapeDesc* gear = CreateGearShapeDesc(gear_radius, gear_thickness * 0.1f, num_teeth, tooth_half_width * 0.9f);
			trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, gear_thickness * 0.5f, 0.f));
			gear_shape->add_child_shape_desc(gear, trans);

			guard_disk = new CylinderShapeDesc(gear_radius + tooth_half_width * 3.f, tooth_half_width);
			trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -(gear_thickness + tooth_half_width), 0.f));
			gear_shape->add_child_shape_desc(guard_disk, trans);
			gear = CreateGearShapeDesc(gear_radius, gear_thickness * 0.1f, num_teeth, tooth_half_width * 0.9f);
			trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, -gear_thickness * 0.5f, 0.f));
			gear_shape->add_child_shape_desc(gear, trans);
			gear_shape->set_texture(m_world.get_texture_map().solid_color(Color(80, 80, 80)));

			btVector3 pivot_in_tank(x * (body_width + spacing), 0.f, z * gear_pos);
			m_gear[i] = m_world.createRigidBody(gear_shape, pivot_in_tank + pos + btVector3(x * gear_thickness, 0.f, 0.f),
												btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(x * 90.f)), 0.1f);
			btHingeConstraint* hinge = new btHingeConstraint(*m_tank_body, *m_gear[i], 
															 pivot_in_tank, btVector3(0.f, gear_thickness, 0.f),
															 btVector3(-x, 0.f, 0.f), btVector3(0.f, 1.f, 0.f), false);
			m_world.addConstraint(hinge, true);
			i += 1;
		}
	}

	float wheel_radius = .6f;
	for (float x : {1.f, -1.f}) {
		for (float z : {1.f, 0.f, -1.f}) {
			// bottom wheels
			CylinderShapeDesc* bottom_wheel = new CylinderShapeDesc(wheel_radius, gear_thickness * 0.4f);
			bottom_wheel->set_texture(m_world.get_texture_map().solid_color(Color(80, 80, 80)));
			// position the bottom of the wheel below the drive gear for better climbing capability
			btVector3 pivot_in_tank(x * (body_width + spacing), -(gear_radius + tooth_half_width + 0.5f) + wheel_radius, z * 1.8f);
			btRigidBody* gear = m_world.createRigidBody(bottom_wheel, pivot_in_tank + pos + btVector3(x * gear_thickness, 0.f, 0.f),
														btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(x * 90.f)), 0.1f);
			btHingeConstraint* hinge = new btHingeConstraint(*m_tank_body, *gear, 
															 pivot_in_tank, btVector3(0.f, gear_thickness, 0.f),
															 btVector3(-x, 0.f, 0.f), btVector3(0.f, 1.f, 0.f), false);
			m_world.addConstraint(hinge, true);

			if (z != 0.f) {
				// top wheels
				CylinderShapeDesc* top_wheel = new CylinderShapeDesc(wheel_radius, gear_thickness * 0.4f);
				top_wheel->set_texture(m_world.get_texture_map().solid_color(Color(80, 80, 80)));
				btVector3 pivot_in_tank(x * (body_width + spacing), gear_radius - wheel_radius, z);
				gear = m_world.createRigidBody(top_wheel, pivot_in_tank + pos + btVector3(x * gear_thickness, 0.f, 0.f),
											   btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(x * 90.f)), 0.1f);
				hinge = new btHingeConstraint(*m_tank_body, *gear, 
											  pivot_in_tank, btVector3(0.f, gear_thickness, 0.f),
											  btVector3(-x, 0.f, 0.f), btVector3(0.f, 1.f, 0.f), false);
				m_world.addConstraint(hinge, true);
			}
		}
	}

	float track_width = (((2.f * glm::radians(180.f) * gear_radius) / num_teeth) - 2.f * tooth_half_width) / 2.f;
	// track length break-down:
	const float pi = glm::radians(180.f);
	float section[] = { 2.f * gear_pos, pi * gear_radius, 2.f * gear_pos, pi * gear_radius};
	for (int c = 0; c < 3; c++) {
		section[c + 1] += section[c];
	}
	float track_length = section[3];

	for (float side : {1.f, -1.f}) {
		btVector3 track_pos(side * (body_width + spacing + gear_thickness), gear_radius + tooth_half_width, -gear_pos);
		btQuaternion track_rotation = btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(0.f));

		btRigidBody* last_track = NULL;
		btRigidBody* first_track = NULL;
		float angle = 0.0;
		int num_tracks = 0;
		for (float x = 0.f; x < track_length; x += 2.f * (track_width + tooth_half_width)) {
			// determine which section we are in
			if (first_track == NULL) {
				// place the first track at x = 0
			} else if (x < section[0]) {
				// top section
				track_pos += btVector3(0.f, 0.f, 2.f * (track_width + tooth_half_width));
				track_rotation = btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(0.f));
			} else if (x < section[1]) {
				// front gear
				float cx = x - section[0];
				angle = cx / gear_radius;
				track_pos = btVector3(side * (body_width + spacing + gear_thickness), 0.f, gear_pos); // front gear center
				track_pos += btVector3(0.f, gear_radius * glm::cos(angle), gear_radius * glm::sin(angle));
				track_rotation = btQuaternion(btVector3(1.f, 0.f, 0.f), angle);
			} else if (x < section[2]) {
				// bottom section
				track_pos.setY(-(gear_radius + tooth_half_width));
				track_pos += btVector3(0.f, 0.f, -2.f * (track_width + tooth_half_width));
				track_rotation = btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(180.f));
			} else if (x < section[3]) {
				// back gear
				float cx = x - section[2];
				angle = pi + cx / gear_radius;
				track_pos = btVector3(side * (body_width + spacing + gear_thickness), 0.f, -gear_pos); // back gear center
				track_pos += btVector3(0.f, gear_radius * glm::cos(angle), gear_radius * glm::sin(angle));
				track_rotation = btQuaternion(btVector3(1.f, 0.f, 0.f), angle);
			}
			
			if ((track_length - x) < 2.f * (track_width + tooth_half_width)) {
				debug_log("Last track width = %f\n", track_length - x);
				continue;
			}
			BoxShapeDesc* track_shape = new BoxShapeDesc(gear_thickness, 0.1f, track_width);
			track_shape->set_texture(m_world.get_texture_map().solid_color("#505050"));
			btRigidBody* track = m_world.createRigidBody(track_shape, track_pos + pos, track_rotation, 0.1f);
			track->setFriction(1.5f);
			if (last_track != NULL) {
				btHingeConstraint* hinge = new btHingeConstraint(*track, *last_track, 
																 btVector3(0.f, 0.f, -(track_width + tooth_half_width)),
																 btVector3(0.f, 0.f, track_width + tooth_half_width), 
																 btVector3(1.f, 0.f, 0.f), btVector3(1.f, 0.f, 0.f), false);
				hinge->setLimit(glm::radians(180.f), glm::radians(-180.f), 20.f);
				m_world.addConstraint(hinge, true);
			}
			if (first_track == NULL) first_track = track;
			last_track = track;
			num_tracks += 1;
		}
		btHingeConstraint* hinge = new btHingeConstraint(*first_track, *last_track, 
														 btVector3(0.f, 0.f, -(track_width + tooth_half_width)),
														 btVector3(0.f, 0.f, track_width + tooth_half_width), 
														 btVector3(1.f, 0.f, 0.f), btVector3(1.f, 0.f, 0.f), false);
		hinge->setLimit(glm::radians(180.f), glm::radians(-180.f), 20.f);
		m_world.addConstraint(hinge, true);
		debug_log_mute("track width + spacing: %f, # of tracks: %d\n", 2.f * (track_width + tooth_half_width), num_tracks);
	}
	// create the gun turret
	m_gun.create(pos + get_connecting_point(), 0.9f);
	attach(m_gun);
}

Gun::Gun(PhysicsWorld& world) : Actor(world)
{
	m_body = NULL;
	m_bottom_base = NULL;
	m_base_half_height = .25f;
	
	m_body_hinge = NULL; // hinge between gun body and base
	m_base_hinge = NULL; // hinge_between top and bottom bases
	m_max_bullets = 30;
	m_time_since_last_shot = 0.f;
	m_barrel_radius = 0.f;
	m_shell = NULL; // only one shell at a time

	// all textures have to be created at scene creation time
	m_projectile_texture = m_world.get_texture_map().solid_color(Color(255, 128, 0));
}

btVector3 Gun::get_connecting_point()
{
	return btVector3(0.f, -m_base_half_height, 0.f);
}

void Gun::create(const btVector3& pos, float scale)
{	
	float body_half_width = 1.f * scale;
	float body_half_height = .5f * scale;
	float body_length = 1.5f * scale;
	float barrel_length = 4.f * scale;
	float base_radius = 2.2f * scale;
	
	m_barrel_radius = body_half_height * 0.75f;
	m_base_half_height *= scale;
	
	// Part I: a rotating base consists of two identical cylinders joined by a hinge
	CylinderShapeDesc* bottom_base = new CylinderShapeDesc(base_radius, m_base_half_height);
	CylinderShapeDesc* top_base = new CylinderShapeDesc(base_radius, m_base_half_height);
	BoxShapeDesc* body = new BoxShapeDesc(body_half_width, body_half_height, body_length);
	bottom_base->set_texture(m_world.get_texture_map().solid_color("#404040"));
	top_base->set_texture(m_world.get_texture_map().solid_color("#404040"));
	body->set_texture(m_world.get_texture_map().solid_color("#505050"));

	glm::mat4 m(1.f);
	CompoundShapeDesc* turret = new CompoundShapeDesc();
	turret->add_child_shape_desc(top_base, m);
	m = glm::translate(glm::mat4(1.f), glm::vec3(0.f, m_base_half_height + body_half_height, 0.f));
	turret->add_child_shape_desc(body, m);

	btTransform trans;
	trans.setIdentity();

	trans.setOrigin(btVector3(0.f, 0.f, 0.f));
	m_bottom_base = m_world.createRigidBody(bottom_base,
											trans.getOrigin() + pos, trans.getRotation(), 5.f);
	trans.setOrigin(btVector3(0.f, 2.f * m_base_half_height, 0.f));
	btRigidBody* top_base_body = m_world.createRigidBody(turret,
														 trans.getOrigin() + pos, trans.getRotation(), 2.f);
	m_base_hinge = new btHingeConstraint(*m_bottom_base, *top_base_body,
										 btVector3(0.f, m_base_half_height, 0.f), btVector3(0.f, -m_base_half_height, 0.f),
										 btVector3(0.f, 1.f, 0.f), btVector3(0.f, 1.f, 0.f));
	m_world.addConstraint(m_base_hinge, true);

	// Part II: gun barrel and turret
	CompoundShapeDesc* gun_shape = new CompoundShapeDesc();
	CylinderShapeDesc* barrel = new CylinderShapeDesc(m_barrel_radius, barrel_length);
	CylinderShapeDesc* joint = new CylinderShapeDesc(m_barrel_radius, body_half_width * 0.75f);

	m = glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
	gun_shape->add_child_shape_desc(joint, m);
	m = glm::translate(m, glm::vec3(0.f, 0.f, barrel_length));
	m = glm::rotate(m, glm::radians(90.f), glm::vec3(1.f, 0.f, 0.f));
	gun_shape->add_child_shape_desc(barrel, m);
	gun_shape->set_texture(m_world.get_texture_map().solid_color("#404040"));
	m_body = m_world.createRigidBody(gun_shape, btVector3(0.f, 3.f * m_base_half_height + body_half_height, body_length) + pos,
									btQuaternion(btVector3(0.f, 1.f, 0.f), glm::radians(0.f)),
									5.f);
	m_body_hinge = new btHingeConstraint(*top_base_body, *m_body,
										 btVector3(0.f, m_base_half_height + body_half_height, body_length), 
										 btVector3(0.f, 0.f, 0.f),
										 btVector3(1.f, 0.f, 0.f), btVector3(1.f, 0.f, 0.f));
	m_world.addConstraint(m_body_hinge, true);
	m_mozzle = btVector3(0.f, 0.f, 2 * barrel_length); // pos related to center of mass of m_body
	// initial direction
	m_base_hinge->setLimit(glm::radians(0.f), glm::radians(0.f));
	m_body_hinge->setLimit(glm::radians(5.f), glm::radians(5.f));
}

void Gun::aim(float yaw_delta, float pitch_delta)
{
	if (!is_ready()) return;

	btScalar cur_yaw = glm::degrees(m_base_hinge->getLowerLimit());
	btScalar yaw = cur_yaw + yaw_delta;
	if (yaw < -90.f) yaw = -90.f;
	else if (yaw > 90.f) yaw = 90.f;
	if (yaw != cur_yaw) {
		m_bottom_base->activate(true);
		m_base_hinge->setLimit(glm::radians(yaw), glm::radians(yaw));
	}
	
	float max_pitch = 30.f;
	btScalar cur_pitch = glm::degrees(m_body_hinge->getLowerLimit());
	btScalar pitch = cur_pitch + pitch_delta;
	if (pitch < -5.f) pitch = -5.f;
	else if (pitch > max_pitch) pitch = max_pitch;
	if (pitch != cur_pitch) {
		m_body->activate(true);
		m_body_hinge->setLimit(glm::radians(pitch), glm::radians(pitch));
	}
}

void Gun::update(float elapsed_time)
{	// house keeping: delete spent bullets and shell
	m_time_since_last_shot += elapsed_time;

	if (!is_ready()) return;
	if (m_bullets.empty() && m_shell == NULL) return;

	Timer timer; // timing starts now
	for (btRigidBody*& bullet : m_bullets) {
		if (m_world.has_contact(bullet)) {
			m_world.removeRigidBody(bullet);
			bullet = NULL;
		}
	}
	m_bullets.erase(std::remove(m_bullets.begin(), m_bullets.end(), (btRigidBody*)NULL), m_bullets.end());

	if (m_shell != NULL) {
		if (m_world.has_contact(m_shell)) {
			m_world.removeRigidBody(m_shell);
			m_shell = NULL;
		}
	}
	debug_log_mute("elapsed time in Gun::update(): %f sesonds\n", timer.get_elapsed_time());
}

void Gun::fire(int ammo_type)
{
	if (ammo_type == Bullet) fire_bullet();
	else fire_shell();
}

void Gun::fire_shell()
{
	if (m_shell != NULL) return;

	btScalar caliber = m_barrel_radius;
	btTransform trans = m_body->getCenterOfMassTransform();
	btQuaternion rotation = trans.getRotation();
	ShapeDesc* projectile = new CapsuleShapeDesc(caliber, caliber);
	projectile->add_texture(m_projectile_texture);
	m_shell = m_world.createRigidBody(projectile, trans(m_mozzle + btVector3(0.f, 0.f, 2.f * caliber)), // non-overlaping with the barrel
									  rotation * btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(90.f)), 2.f);
	// enable CCD (Continuous Collision Detection) if distance 
	// is larger than one bullet caliber in one simulation step
	m_shell->setCcdMotionThreshold(caliber);
	btVector3 propulsion = btVector3(0.f, 0.f, 300.f).rotate(rotation.getAxis(), rotation.getAngle());
	m_shell->applyImpulse(propulsion, btVector3(0.f, 0.f, 0.f));
	m_body->applyImpulse(propulsion * -0.05f, btVector3(0.f, 0.f, 0.f));
}

void Gun::fire_bullet()
{
	if (m_bullets.size() >= m_max_bullets || m_time_since_last_shot < 0.1f) {
		return;
	}
	btScalar caliber = 0.75f * m_barrel_radius;
	btTransform trans = m_body->getCenterOfMassTransform();
	btQuaternion rotation = trans.getRotation();
	ShapeDesc* projectile = new SphereShapeDesc(caliber);
	projectile->add_texture(m_projectile_texture);
	btRigidBody* bullet = m_world.createRigidBody(projectile, trans(m_mozzle + btVector3(0.f, 0.f, caliber)), // non-overlaping with the barrel
												  rotation, .5f);
	for (auto b : m_bullets) {
		// reduce the objects for contact check
		bullet->setIgnoreCollisionCheck(b, true);
	}
	// enable CCD (Continuous Collision Detection) if distance 
	// is larger than one bullet caliber in one simulation step
	bullet->setCcdMotionThreshold(caliber);
	btVector3 propulsion = btVector3(0.f, 0.f, 80.f).rotate(rotation.getAxis(), rotation.getAngle());
	bullet->applyImpulse(propulsion, btVector3(0.f, 0.f, 0.f));
	m_body->applyImpulse(propulsion * -0.05f, btVector3(0.f, 0.f, 0.f));
	m_bullets.push_back(bullet);
	debug_log_mute("# of bullets: %d, time between shots: %f seconds\n",
				   m_bullets.size(), m_time_since_last_shot);
	m_time_since_last_shot = 0.f;
}

void Gun::process_player_input(Controller& ctlr)
{
	glm::vec2 pos = ctlr.get_cursor_movement();
	if (ctlr.is_key_pressed(GLFW_KEY_LEFT_SHIFT) || ctlr.is_mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT)) {
		aim(pos.x * 0.05f, pos.y * -0.05f);
	}
	glm::vec2 scroll = ctlr.get_scroll_movement();
	aim(scroll.x * 3.f, scroll.y * -5.f);
	if (ctlr.is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT)) fire(Gun::Bullet);
	if (ctlr.is_key_pressed(GLFW_KEY_ENTER)) fire(Gun::Shell);
}

ShapeDesc* CreateGearShapeDesc (float radius, float half_thickness, int num_teeth, float tooth_half_width)
{
	if (tooth_half_width == 0.f) {
		tooth_half_width = radius * glm::sin(glm::radians(360.f / (2.f * num_teeth))) * .5f;
	}
	CompoundShapeDesc* gear_shape = new CompoundShapeDesc();
	CylinderShapeDesc* disk = new CylinderShapeDesc (radius, half_thickness);
	disk->set_texture(gear_shape->m_default_texture);
	gear_shape->add_child_shape_desc(disk, glm::mat4(1.f));

	float n = 360.f / num_teeth;  // Make sure this divides evenly!!
	for (float i = 0.f; i < 360.f; i += n) {
		ShapeDesc* tooth = new BoxShapeDesc (tooth_half_width, half_thickness, tooth_half_width * 2);
		float x = radius * glm::sin(glm::radians(i));
		float z = radius * glm::cos(glm::radians(i));
		
		glm::mat4 m(1.f);
		m = glm::translate(m, glm::vec3(x, 0.f, z));
		m = glm::rotate(m, glm::radians(i), glm::vec3(0.f, 1.f, 0.f));
		tooth->set_texture(gear_shape->m_default_texture);
		gear_shape->add_child_shape_desc (tooth, m);
	}
	return gear_shape;
}
