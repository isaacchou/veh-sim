/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <map>
#include <set>
#include "Renderer.h"
#include "Shapes.h"
#include "Controller.h"
#include "TextureMaps.h"

class SceneObserver
{
protected:
	int m_next_shape_id;
	std::map<int, const Shape*> m_shapes;
	std::map<int, glm::mat4> m_trans;

	std::set<int> m_add, m_update, m_remove;

	Renderer* m_player;
	Controller m_controller;
	glm::mat4 m_player_trans;
	TextureMap m_TextureMap;

public:
	SceneObserver() : m_next_shape_id (0), m_player (NULL), m_player_trans() {}
	virtual ~SceneObserver() {}

	TextureMap& get_texture_map() { return m_TextureMap; }

	void begin_update() { if (m_player) m_player->begin_update(); }
	bool end_update(float elapsed_time) { 
		if (m_player) {
			return m_player->end_update(elapsed_time);
		}
		return true;
	}

	int add_shape(const Shape* shape, const glm::mat4& trans) {
		m_shapes.insert(std::make_pair(m_next_shape_id, shape));
		m_trans.insert(std::make_pair(m_next_shape_id, trans));
		m_add.insert(m_next_shape_id);
		return m_next_shape_id++;
	}

	void update_shape(int id, const glm::mat4& trans) {
		m_trans[id] = trans;
		m_update.insert(id);
	}

	void remove_shape(int id) {
		delete m_shapes[id];
		m_shapes.erase(id);
		m_trans.erase(id);
	
		if (m_add.find(id) == m_add.end()) {
			m_remove.insert(id);
		} else {
			// shape is added and removed in the same update cycle
			m_add.erase(id);
		}
		m_update.erase(id);
	}

	void connect(Renderer* player) {
		m_player = player;

		m_player->pre_connect();
		for (auto& i : m_TextureMap.get_image_map()) {
			m_player->add_texture(i.first, i.second.width, i.second.height, i.second.data);
		}
		for (auto& s : m_shapes) {
			const Shape& shape = *s.second;
			m_player->add_shape(s.first, shape.to_json(m_trans[s.first]).c_str());
		}
		m_player->post_connect();
	}
	
	void update() {
		if (m_player != NULL) {
			for (int i : m_add) m_player->add_shape(i, (*m_shapes[i]).to_json(m_trans[i]).c_str());
			for (int i : m_update) m_player->update_shape(i, m_trans[i]);
			for (int i : m_remove) m_player->remove_shape(i);
		}
		m_add.clear();
		m_update.clear();
		m_remove.clear();
	}

	Controller& get_controller(int which) {
		if (m_player != NULL) {
			int n = m_player->how_many_controllers();
			if (which < n) m_controller.sync_from(m_player->get_controller(which));
			else { Controller c; m_controller.sync_from(c); }
		}
		return m_controller; 
	}
	
	void set_player_transform(int which, const glm::mat4& trans) { 
		m_player->set_player_transform(which, trans);
	}
};
