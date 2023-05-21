/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <vector>
#include <btBulletDynamicsCommon.h>
#include "../Interface/SceneObserver.h"

class Actor;

class PhysicsWorld
{
private:
	std::vector<Actor*> m_actors;

protected:
	btDefaultCollisionConfiguration* collisionConfiguration;
	btCollisionDispatcher* dispatcher;
	btBroadphaseInterface* overlappingPairCache;
	btSequentialImpulseConstraintSolver* solver;
	btDiscreteDynamicsWorld* dynamicsWorld;
	btAlignedObjectArray<btCollisionShape*> collisionShapes;

	SceneObserver m_observer;

	virtual void create_scene() {}
	virtual void update_scene();

	void teardown();

	// multi-player aware
	virtual void process_player_input(int which, Controller& ctlr) = 0;
	virtual const btRigidBody& get_player_body(int which) = 0;

	virtual void update_objects(float elapsed_time);
	const glm::mat4 get_body_transform(const btRigidBody& body);
	
public:
	PhysicsWorld();
	virtual ~PhysicsWorld();

	virtual int how_many_players() = 0;
	
	int run(const char* title);
	btRigidBody* createRigidBody(ShapeDesc* shape, btVector3 origin, btQuaternion rotation, btScalar mass = 0.f);
	void removeRigidBody(btRigidBody* body);
	void addConstraint(btTypedConstraint* constraint, bool disableCollisionsBetweenLinkedBodies = false) {
		dynamicsWorld->addConstraint(constraint, disableCollisionsBetweenLinkedBodies);
	}
	void add_actor(Actor& actor) { m_actors.push_back(&actor); }
	bool has_contact(btRigidBody* object);

	// methods to interact with players and observers
	TextureMap& get_texture_map() { return m_observer.get_texture_map(); }
	SceneObserver& get_scene_observer() { return m_observer; }
};
