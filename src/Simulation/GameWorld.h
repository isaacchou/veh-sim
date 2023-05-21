/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include "PhysicsWorld.h"
#include "Actors.h" 

class GameWorld : public PhysicsWorld
{
protected:
	std::vector<Actor*> m_actors;
	
	void create_ground();
	void create_target(float radius, const btVector3& pos);
	void create_beach_ball(float radius, const btVector3& pos);
	void create_house(const btVector3& pos);

	void add_actor(Actor* actor, const btVector3& pos) { 
		actor->create(pos);
		m_actors.push_back(actor); 
	}

public:
	GameWorld() {}
	virtual ~GameWorld() {
		for (auto& actor : m_actors) delete actor;
	}
	virtual int how_many_players() { return (int)m_actors.size(); }
	virtual void process_player_input(int which, Controller& ctlr) { m_actors[which]->process_player_input(ctlr); }
	virtual const btRigidBody& get_player_body(int which) { return m_actors[which]->body(); }
	
	virtual void create_test_scene();
	virtual void create_scene() { create_test_scene(); }

	void add_tank(const btVector3& pos) { add_actor(new Tank(*this), pos); }
	void add_v150(const btVector3& pos) { add_actor(new Car(*this), pos); }
};
