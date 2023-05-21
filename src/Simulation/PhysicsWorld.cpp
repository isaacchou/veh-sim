/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <chrono>
#include "PhysicsWorld.h"
#include "Actors.h"
#include "../Interface/Shapes.h"
#include "../Utils.h"

PhysicsWorld::PhysicsWorld()
{	
	collisionConfiguration = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collisionConfiguration);
	overlappingPairCache = new btDbvtBroadphase();
	solver = new btSequentialImpulseConstraintSolver;
	dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
	dynamicsWorld->setGravity(btVector3(0.f, -10.f, 0.f));
}

PhysicsWorld::~PhysicsWorld()
{
	delete dynamicsWorld;
	delete solver;
	delete overlappingPairCache;
	delete dispatcher;
	delete collisionConfiguration;

	// optional: it will be cleared by the destructor when the array goes out of scope
	collisionShapes.clear();
}

btCollisionShape* create_collision_shape(const ShapeDesc& shape)
{
	if (shape.m_type == ShapeDesc::Type::Compound) {
		btCompoundShape* collision_shape = new btCompoundShape();
		const CompoundShapeDesc& compound_shape_desc = dynamic_cast<const CompoundShapeDesc&>(shape);
		for (auto& child : compound_shape_desc.get_child_shape_desc()) {
			btTransform trans;
			trans.setFromOpenGLMatrix(&child.m_trans[0][0]);
			collision_shape->addChildShape(trans, create_collision_shape(*child.m_desc));
		}
		return collision_shape;
	}
	
	btCollisionShape* collision_shape = NULL;
	switch (shape.m_type) {
		case ShapeDesc::Type::Ground:
			collision_shape = new btStaticPlaneShape(btVector3(0.f, 1.f, 0.f), 0);
		break;
		case ShapeDesc::Type::Box:
			collision_shape = new btBoxShape(btVector3(shape.m_param[0], shape.m_param[1], shape.m_param[2]));
		break;
		case ShapeDesc::Type::Sphere:
			collision_shape = new btSphereShape(shape.m_param[0]);
		break;
		case ShapeDesc::Type::Cylinder:
			collision_shape = new btCylinderShape(btVector3(shape.m_param[0], shape.m_param[1], shape.m_param[0]));
		break;
		case ShapeDesc::Type::Capsule:
			collision_shape = new btCapsuleShape(shape.m_param[0], shape.m_param[1]);
		break;
		case ShapeDesc::Type::Cone:
			collision_shape = new btConeShape(shape.m_param[0], shape.m_param[1]);
		break;
		case ShapeDesc::Type::Pyramid:
		case ShapeDesc::Type::Wedge:
		case ShapeDesc::Type::V150:
		{
			const ConvexShapeDesc& convex_shape = dynamic_cast<const ConvexShapeDesc&>(shape);
			std::vector<glm::vec3> vertices;
			convex_shape.get_vertices(vertices);
			btConvexHullShape* convex_hull_shape = new btConvexHullShape ();
			for (const glm::vec3& v : vertices) {
				convex_hull_shape->addPoint (btVector3 (v.x, v.y, v.z));
			}
			collision_shape = convex_hull_shape;
		}
		break;
		default:
			return NULL;
	}
	return collision_shape;
}

btRigidBody* PhysicsWorld::createRigidBody(ShapeDesc* shape,
	btVector3 origin, btQuaternion rotation, btScalar mass)
{
	btCollisionShape* collision_shape = create_collision_shape((const ShapeDesc&)*shape);
	collisionShapes.push_back(collision_shape);

	btVector3 localInertia(0, 0, 0);
	// rigidbody is dynamic if and only if mass is non zero, otherwise static
	if (mass != 0.f) {
		collision_shape->calculateLocalInertia(mass, localInertia);
	}
	btTransform trans(rotation, origin);
	// using motionstate is recommended, it provides interpolation capabilities, 
	// and only synchronizes 'active' objects
	btDefaultMotionState* myMotionState = new btDefaultMotionState(trans);
	btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState, collision_shape, localInertia);
	btRigidBody* body = new btRigidBody(rbInfo);
	dynamicsWorld->addRigidBody(body);
	
	glm::mat4 m;
	trans.getOpenGLMatrix(&m[0][0]);
	int id = m_observer.add_shape(shape, m);
	collision_shape->setUserIndex(id);
	return body;
}

void PhysicsWorld::removeRigidBody(btRigidBody* body)
{
	int id = body->getCollisionShape()->getUserIndex();
	m_observer.remove_shape(id);
}

void PhysicsWorld::teardown()
{	// remove the rigid bodies from the dynamics world and delete them
	for (int i = dynamicsWorld->getNumCollisionObjects() - 1; i >= 0; i--)
	{
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[i];
		btRigidBody* body = btRigidBody::upcast(obj);
		if (body && body->getMotionState())
		{
			delete body->getMotionState();
		}
		dynamicsWorld->removeCollisionObject(obj);
		// need to remove all constraints before deleting rigid bodies
		//delete obj;
	}

	// delete collision shapes
	for (int j = 0; j < collisionShapes.size(); j++)
	{
		btCollisionShape* shape = collisionShapes[j];
		collisionShapes[j] = 0;
		delete shape;
	}
}

const glm::mat4 PhysicsWorld::get_body_transform(const btRigidBody& body)
{
	glm::mat4 model;
	btTransform trans = body.getCenterOfMassTransform();
	trans.getOpenGLMatrix(&model[0][0]);
	return model;
}

void PhysicsWorld::update_scene()
{
	for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--)
	{
		btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
		btRigidBody* body = btRigidBody::upcast(obj);
		btTransform trans;
		if (body && body->getMotionState()) {
			body->getMotionState()->getWorldTransform(trans);
		} else {
			trans = obj->getWorldTransform();
		}

		btCollisionShape* shape = obj->getCollisionShape();
		if (shape != NULL)
		{
			glm::mat4 model;
			trans.getOpenGLMatrix(&model[0][0]);
			int id = shape->getUserIndex();
			m_observer.update_shape(id, model);
		}
	}
}

void PhysicsWorld::update_objects(float elapsed_time)
{	// called once for every simulation step
	for (Actor* actor : m_actors) {
		actor->update(elapsed_time);
	}
}

int PhysicsWorld::run(const char* title)
{	// initialization
	std::chrono::high_resolution_clock timer;
	std::chrono::high_resolution_clock::time_point last_time = timer.now();
	bool cont = true;
	while (cont)
	{	// physics simulation
		using fseconds = std::chrono::duration<float>;
		std::chrono::high_resolution_clock::time_point cur_time = timer.now();
		float elapsed_time = std::chrono::duration_cast<fseconds>(cur_time - last_time).count();
		last_time = cur_time;

		//--- observer communicates with renderes and players 
		int n = how_many_players();
		for (int i = 0; i < n; i++) {
			// this is the only time we read from the renderer/player client
			process_player_input(i, m_observer.get_controller(i));
		}
		// after processing inputs, update object and enviromental 
		// states before the physics simulation
		m_observer.begin_update();
		update_objects(elapsed_time); // objects could be removed
		dynamicsWorld->stepSimulation(elapsed_time * 3.f, 10);
		
		for (int i = 0; i < n; i++) {
			m_observer.set_player_transform(i, get_body_transform(get_player_body(i)));
		}
		update_scene();
		m_observer.update();
		//---
		cont = m_observer.end_update(elapsed_time);
	}
	return EXIT_SUCCESS;
}

bool PhysicsWorld::has_contact(btRigidBody* object)
{
	struct contact_callback : public btCollisionWorld::ContactResultCallback
	{
		int m_NumContacts;
		contact_callback() { m_NumContacts = 0; }

		virtual btScalar addSingleResult(btManifoldPoint& cp,
										 const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
										 const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
		{
			m_NumContacts += 1;
			return 1.f; // not sure what this affects??
		}
	};
	contact_callback ccb;
	dynamicsWorld->contactTest(object, ccb);
	return ccb.m_NumContacts > 0;
}
