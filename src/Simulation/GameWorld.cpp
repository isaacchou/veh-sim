/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <glm/gtc/matrix_transform.hpp>
#include "GameWorld.h"

void GameWorld::create_ground()
{	// ground spans along x and z axises at y = 0
	ShapeDesc* shape = new GroundShapeDesc (1000, 1000);
	shape->add_texture(get_texture_map().checker_board(100, 100, Color("white"), Color("gray")));
	createRigidBody(shape,
					btVector3(0.0f, 0.0f, 0.0f),
					btQuaternion(btVector3(0.f, 1.f, 0.f), 0.f));
}

void GameWorld::create_target(float radius, const btVector3& pos)
{
	SphereShapeDesc* shape = new SphereShapeDesc (radius);
	shape->add_texture(get_texture_map().vertical_stripes(8, Color("red"), Color("white")));
	createRigidBody(shape, pos,
					btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(-90.f)),
					10.f);
}

void GameWorld::create_beach_ball(float radius, const btVector3& pos)
{
	SphereShapeDesc* shape = new SphereShapeDesc(radius);
	shape->add_texture(get_texture_map().horizontal_stripes(8, Color("red"), Color("white")));
	createRigidBody(shape, pos,
					btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(-90.f)),
					10.f);
}

void GameWorld::create_house(const btVector3& pos)
{
	CompoundShapeDesc* house = new CompoundShapeDesc ();
	
	glm::mat4 trans(1.f);

	ShapeDesc* box = new BoxShapeDesc (10.f, 10.f, 30.f);
	house->add_child_shape_desc(box, trans);
	
	trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 15.f, 0.f));
	ShapeDesc* roof = new WedgeShapeDesc (12.f, 5.f, 30.f, 30.f);
	house->add_child_shape_desc(roof, trans);
	
	// nested compound shapes: steeple
	CompoundShapeDesc* steeple = new CompoundShapeDesc ();
	
	ShapeDesc* steeple_box = new BoxShapeDesc (5.f, 10.f, 5.f);
	steeple->add_child_shape_desc(steeple_box, glm::mat4(1.f));
	
	ShapeDesc* steeple_roof = new PyramidShapeDesc (5.f, 10.f, 5.f);
	trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 20.f, 0.f));
	steeple->add_child_shape_desc(steeple_roof, trans);
	//-- end of steeple

	trans = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 15.f, 0.f));
	house->add_child_shape_desc(steeple, trans);

	// add textures
	box->add_texture(get_texture_map().solid_color(Color(100, 50, 200)), 6);
	roof->add_texture(get_texture_map().solid_color(Color(255, 200, 200)), 5);
	steeple_roof->add_texture(get_texture_map().solid_color(Color(150, 200, 200)), 5);
	steeple_box->add_texture(get_texture_map().solid_color(Color(150, 200, 200)), 5);

	createRigidBody(house,
					pos, btQuaternion(btVector3(0.f, 1.f, 0.f), glm::radians(45.f)),
					1000.f);
}

void GameWorld::create_test_scene()
{
	create_ground();
	
	unsigned int texture_1 = get_texture_map().diagonal_stripes(100, 100, 0, Color("gold"), Color("blue"));
	unsigned int texture_2 = get_texture_map().horizontal_stripes(5, Color("grey"), Color("white"));
	unsigned int texture_3 = get_texture_map().checker_board(5, 5, Color("yellow"), Color("red"));

	ShapeDesc* shape = new BoxShapeDesc(15.f, 3.f, 10.f);
	shape->add_texture(get_texture_map().diagonal_stripes(160, 32, 3, Color("gold"), Color("red")), 2);
	shape->add_texture(get_texture_map().checker_board(15, 10, Color("orange"), Color("blue")), 4);
	createRigidBody(shape,
					btVector3(0.0f, 3.0f, 0.0f),
					btQuaternion(btVector3(0.f, 0.f, 1.f), 0.f));
	// dominoes
	shape = new BoxShapeDesc(2.f, 5.f, 5.f);
	shape->add_texture(get_texture_map().solid_color(Color("#8888FF")), 6);
	createRigidBody(shape,
					btVector3(-100.0f, 10.0f, 0.0f),
					btQuaternion(btVector3(0.f, 0.f, 1.f), 0.f), 80.f);

	shape = new BoxShapeDesc(2.f, 5.f, 5.f);
	shape->add_texture(get_texture_map().solid_color(Color("gold")), 6);
	createRigidBody(shape,
					btVector3(-120.0f, 10.0f, 0.0f),
					btQuaternion(btVector3(0.f, 0.f, 1.f), 0.f), 80.f);
	
	shape = new BoxShapeDesc(2.f, 5.f, 5.f);
	shape->add_texture(get_texture_map().solid_color(Color("orange")), 6);
	createRigidBody(shape,
					btVector3(-140.0f, 10.0f, 0.0f),
					btQuaternion(btVector3(0.f, 0.f, 1.f), 0.f), 80.f);
	// capsule
	shape = new CapsuleShapeDesc(5.0f, 5.f);
	shape->add_texture(get_texture_map().checker_board(10, 10, Color("green"), Color("white")));
	createRigidBody(shape,
					btVector3(0.f, 30.0f, 0.0f),
					btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(0.f)),
					10.f);
	// cylinder
	shape = new CylinderShapeDesc(5.0f, 12.f);
	shape->add_texture(get_texture_map().checker_board(5, 5, Color("red"), Color("white")));
	shape->add_texture(get_texture_map().checker_board(10, 10, Color("red"), Color("blue")));
	shape->add_texture(get_texture_map().checker_board(5, 5, Color("blue"), Color("black")));
	createRigidBody(shape,
					btVector3(15.1f, 30.0f, 0.0f),
					btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(90.f)),
					10.f);
	// cone
	shape = new ConeShapeDesc(5.0f, 10.0f);
	shape->add_texture(texture_1);
	shape->add_texture(texture_2);
	createRigidBody(shape,
					btVector3(12.f, 50.0f, 0.0f),
					btQuaternion(btVector3(1.f, 0.f, 1.f), glm::radians(120.f)),
					10.f);
	// sphere
	shape = new SphereShapeDesc(10.0f);
	shape->add_texture(get_texture_map().solid_color(Color("#DEB887")));
	createRigidBody(shape,
					btVector3(-20.f, 50.0f, 0.0f),
					btQuaternion(btVector3(0.f, 1.f, 0.f), 0.f),
					100.f);
	// pyramid
	shape = new PyramidShapeDesc(5.f, 15.f, 5.f);
	shape->add_texture(texture_3);
	shape->add_texture(texture_2);
	createRigidBody(shape,
					btVector3(50.f, 50.0f, 50.f),
					btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(60.f)),
					10.f);
	// wedge (e.g. house roof)
	shape = new WedgeShapeDesc(5.f, 5.f, 5.f, 8.f);
	shape->add_texture(texture_1);
	shape->add_texture(texture_2);
	shape->add_texture(texture_3);
	createRigidBody(shape,
					btVector3(50.f, 50.0f, 50.f),
					btQuaternion(btVector3(0.f, 0.f, 1.f), glm::radians(0.f)),
					10.f);
	// wedge (ramp)
	shape = new WedgeShapeDesc(5.f, 50.f, 10.f, 10.f);
	shape->add_texture(get_texture_map().solid_color(Color(128, 128, 128)), 3);
	createRigidBody(shape,
					btVector3(80.f, 50.0f, -30.f),
					btQuaternion(btVector3(0.f, 1.f, 1.f), glm::radians(60.f)),
					100.f);

	ShapeDesc* gear = CreateGearShapeDesc(5.f, 1.f, 10);
	gear->set_texture(get_texture_map().solid_color(Color(64, 64, 64)));
	createRigidBody(gear,
					btVector3(-20.f, 20.0f, -60.f),
					btQuaternion(btVector3(1.f, 0.f, 0.f), glm::radians(90.f)),
					1.f);

	create_target(5.f, btVector3(0.f, 80.f, 0.f));
	create_target(3.f, btVector3(0.f, 80.f, 10.f));
	create_target(3.f, btVector3(0.f, 80.f, -10.f));
	create_beach_ball(3.f, btVector3(10.f, 80.f, 0.f));
	create_beach_ball(3.f, btVector3(-10.f, 80.f, 0.f));

	create_house(btVector3(-100.f, 10.f, -100.f));
}
