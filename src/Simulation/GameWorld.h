/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <rapidjson/document.h>
#include "PhysicsWorld.h"
#include "Actors.h" 

class GameWorld : public PhysicsWorld
{
protected:
	std::vector<Actor*> m_actors;
	btVector3 m_camera_pos;
	bool m_camera_follow_player;

	ShapeDesc* create_shape_from_macro(const rapidjson::Document& doc, const char* macro_name);
	ShapeDesc* create_shape_from_json(const rapidjson::Document& doc, const rapidjson::Value& shape_obj);

	void add_actor(Actor* actor, const btVector3& pos) { 
		actor->create(pos);
		m_actors.push_back(actor); 
	}

public:
	GameWorld() : m_camera_pos(0.f, 6.f, -20.f), m_camera_follow_player(true) {}
	virtual ~GameWorld() {
		for (auto& actor : m_actors) delete actor;
	}
	virtual int how_many_players() { return (int)m_actors.size(); }
	virtual void process_player_input(int which, Controller& ctlr) { m_actors[which]->process_player_input(ctlr); }
	virtual const btRigidBody& get_player_body(int which) { return m_actors[which]->body(); }

	btVector3& get_camera_initial_pos() { return m_camera_pos; }
	bool should_camera_follow_player() { return m_camera_follow_player && how_many_players() > 0; }
	
	bool create_scene_from_file(const char* filename);
	void add_tank(const btVector3& pos) { add_actor(new Tank(*this), pos); }
	void add_v150(const btVector3& pos) { add_actor(new Car(*this), pos); }
};
