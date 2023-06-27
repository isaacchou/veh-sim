/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <array>
#include <cstdio>
#include <filesystem>
#include <boost/json.hpp>
#include "GameWorld.h"

namespace json = boost::json;

class JsonFile
{
protected:
	json::value m_doc;
	json::object* m_root_obj; 

	std::filesystem::path m_filename;
	std::vector<JsonFile*> m_imports;

	json::value
	parse_file(char const* filename)
	{
		FILE* f = fopen(filename, "rb");
		if (f == 0) {
			std::string msg = "failed to open file \'";
			msg += filename;
			msg += "\'";
			std::perror(msg.c_str());
			return nullptr;
		}
		json::stream_parser p;
		json::error_code ec;

		size_t nread = 0;
		std::array<char, 4096> buf;
		do {
			nread = fread(buf.data(), sizeof(char), buf.size(), f);
			p.write(buf.data(), nread, ec);
		} while (nread == buf.size() && !ec);
		fclose(f);
		if (ec) return nullptr;
		
		p.finish(ec);
		return ec ? nullptr : p.release();
	}

public:
	JsonFile(const char* filename) : 
		m_root_obj(nullptr), m_filename(filename) {
		if (m_filename.is_relative()) {
			// make it absolute so we can set the current path 
			// for subsequent path resolution
			m_filename = std::filesystem::absolute(m_filename);
		}
	}
	virtual ~JsonFile() { 
		for (auto& i : m_imports) {
			delete i;
		}
	}

	bool parse() {
		m_doc = parse_file(m_filename.string().c_str());
		m_root_obj = m_doc.if_object();
		if (m_root_obj == nullptr) return false;

		json::value* v = m_root_obj->if_contains("imports");
		if (v && v->is_array()) {
			// the import file paths are relative to the current folder
			std::filesystem::path cwd = std::filesystem::current_path();
			std::filesystem::current_path(m_filename.parent_path());
			for (const auto& i : v->get_array()) {
				if (!i.is_string()) continue;

				JsonFile* json = new JsonFile(i.get_string().c_str());
				if (json->parse()) {
					m_imports.push_back(json);
				} else {
					// failed to prase the json file					
					delete json;
				}
			}
			std::filesystem::current_path(cwd);
		}
		return true;
	}
	const json::object& root_obj() { return *m_root_obj; }
	Shape* create_shape(GameWorld& world, const json::value* shape_obj);
};

Shape* JsonFile::create_shape(GameWorld& world, const json::value* shape_obj)
{
	if (shape_obj == nullptr) return nullptr;

	if (shape_obj->is_string()) {
		// possible macro
		const char* macro_name = shape_obj->get_string().c_str();
		auto m = root_obj().if_contains("macros");
		if (m && m->is_object() && m->get_object().if_contains(macro_name)) {
			// resolved macro in the current file
			return create_shape(world, &m->get_object().at(macro_name));
		} else {
			// cannot resolve the macro, try imports
			for (JsonFile* f : m_imports) {
				Shape* shape = f->create_shape(world, shape_obj);
				if (shape != nullptr) return shape; // resolved with the import
			}
			// cannot resolve the macro with imports
			return nullptr;
		}
	} else if (!shape_obj->is_object() || !shape_obj->as_object().contains("kind")) {
		// not a valid shape descriptor
		return NULL;
	}

	const auto obj = shape_obj->as_object();
	const json::string kind = obj.at("kind").as_string();
	if (kind == "compound") {
		CompoundShape* compound_shape = NULL;
		if (obj.contains("child")) {
			compound_shape = new CompoundShape();
			for (const auto& child : obj.at("child").as_array()) {
				if (!child.is_object()) continue;

				const json::object& child_obj = child.as_object();
				Shape* child_shape = NULL;
				if (child_obj.contains("shape")) {
					child_shape = create_shape(world, &child_obj.at("shape"));
				}
				if (child_shape != NULL && child_obj.contains("origin")) {
					// for child shape, 'origin' is required while rotation optional
					std::vector<float> p = std::move(value_to<std::vector<float>>(child_obj.at("origin")));
					std::vector<float> r{ 1.f, 0.f, 0.f, 0.f };
					if (child_obj.contains("rotation")) {
						r = std::move(value_to<std::vector<float>>(child_obj.at("rotation")));
					}
					compound_shape->add_child_shape(child_shape, 
						glm::vec3(p[0], p[1], p[2]),		// origin of child shape in the compound reference frame
						glm::vec3(r[0], r[1], r[2]), r[3]); // orientation of the child shape
				}
			}
			return compound_shape;
		}
	}
	
	if (!obj.contains("dimension")) return nullptr;
	
	Shape* shape = nullptr;
	std::vector<float> dimension = std::move(value_to<std::vector<float>>(obj.at("dimension")));
	if (kind == "ground") {
		shape = new GroundShape(dimension[0], dimension[1]);
	} else if (kind == "box") {
		shape = new BoxShape(dimension[0], dimension[1], dimension[2]);
	} else if (kind == "sphere") {
		shape = new SphereShape(dimension[0]);
	} else if (kind == "cylinder") {
		shape = new CylinderShape(dimension[0], dimension[1]);
	} else if (kind == "capsule") {
		shape = new CapsuleShape(dimension[0], dimension[1]);
	} else if (kind == "cone") {
		shape = new ConeShape(dimension[0], dimension[1]);
	} else if (kind == "pyramid") {
		shape = new PyramidShape(dimension[0], dimension[1], dimension[2]);
	} else if (kind == "wedge") {
		shape = new WedgeShapeDesc(dimension[0], dimension[1], dimension[2], dimension[3]);
	} else if (kind == "gear") {
		shape = CreateGearShape(dimension[0], dimension[1], (int)dimension[2]);
	}
	if (shape == nullptr) return nullptr;

	if (obj.contains("textures")) {
		// all texture file pathnames are relative to the current folder
		std::filesystem::path cwd = std::filesystem::current_path();
		std::filesystem::current_path(m_filename.parent_path());
		TextureMap& txtr_map = world.get_texture_map();
		for (const auto& t : obj.at("textures").as_array()) {
			const json::object& txtr = t.as_object();

			unsigned int texture = 0;
			if (txtr.contains("file")) {
				texture = txtr_map.from_file(txtr.at("file").get_string().c_str());
			} else if (txtr.contains("color")) {
				texture = txtr_map.solid_color(txtr.at("color").get_string().c_str());
			} else if (txtr.contains("checker_board")) {
				const json::array& p = txtr.at("checker_board").as_array();
				texture = txtr_map.checker_board(p[0].get_int64(), p[1].get_int64(), 
					Color(p[2].get_string().c_str()), Color(p[3].get_string().c_str()));
			} else if (txtr.contains("diagonal_stripes")) {
				const json::array& p = txtr.at("diagonal_stripes").as_array();
				texture = txtr_map.diagonal_stripes((int)p[0].get_int64(), (int)p[1].get_int64(), (int)p[2].get_int64(), 
					Color(p[3].get_string().c_str()), Color(p[4].get_string().c_str()));
			} else if (txtr.contains("vertical_stripes")) {
				const json::array& p = txtr.at("vertical_stripes").as_array();
				texture = txtr_map.vertical_stripes((int)p[0].get_int64(), 
					Color(p[1].get_string().c_str()), Color(p[2].get_string().c_str()));
			} else if (txtr.contains("horizontal_stripes")) {
				const json::array& p = txtr.at("horizontal_stripes").as_array();
				texture = txtr_map.horizontal_stripes((int)p[0].get_int64(), 
					Color(p[1].get_string().c_str()), Color(p[2].get_string().c_str()));
			}

			int repeat = txtr.contains("repeat") ? (int)txtr.at("repeat").get_int64() : 1;
			if (repeat == 0) {
				// set as default texture for all faces without a texture
				shape->set_texture(texture);
			} else {
				shape->add_texture(texture, repeat);
			}
			std::filesystem::current_path(cwd);
		}
	}
	return shape;
}

bool GameWorld::create_scene_from_file(const char* filename)
{
	try
	{
		JsonFile json(filename);
		if (!json.parse()) return false;
		if (!json.root_obj().contains("scene")) return false;

		// "scene" is an array of shape descriptors 
		for (const auto obj : json.root_obj().at("scene").as_array()) {
			// a shape descriptor is an object
			const auto shape_desc = obj.as_object();
			Shape* shape = json.create_shape(*this, shape_desc.if_contains("shape"));
			if (shape == nullptr) continue;

			// 'origin' is required 
			if (!shape_desc.contains("origin")) continue;
			std::vector<float> origin = std::move(value_to<std::vector<float>>(shape_desc.at("origin")));

			// 'rotation' is optional 
			btQuaternion rotation(btVector3(1.f, 0.f, 0.f), 0.f);
			if (shape_desc.contains("rotation")) {
				std::vector<float> r = std::move(value_to<std::vector<float>>(shape_desc.at("rotation")));
				rotation = btQuaternion(btVector3(r[0], r[1], r[2]), glm::radians(r[3]));
			}	
			// 'mass' is optional
			float mass = shape_desc.contains("mass") ? value_to<float>(shape_desc.at("mass")) : 0.f; 
			createRigidBody(*shape,
							btVector3(origin[0], origin[1], origin[2]),
							rotation, mass);
		}

		// player has to be processed before camera so we know if camera can and should follow the player
		if (json.root_obj().contains("player")) {
			const auto& player = json.root_obj().at("player").as_object();
			if (player.contains("vehicle") && player.contains("origin")) {
				std::vector<float> origin = std::move(value_to<std::vector<float>>(player.at("origin")));
				const json::string vehicle = player.at("vehicle").as_string();
				if (vehicle == "tank") {
					add_tank(btVector3(origin[0], origin[1], origin[2]));
				} else if (vehicle == "V150") {
					add_v150(btVector3(origin[0], origin[1], origin[2]));
				}
			}
		}

		if (json.root_obj().contains("camera")) {
			const auto& camera = json.root_obj().at("camera").as_object();
			if (camera.contains("eye")) {
				// 'eye' is required for camera
				std::vector<float> pos = std::move(value_to<std::vector<float>>(camera.at("eye")));
				m_camera_pos = btVector3 (pos[0], pos[1], pos[2]);
				// 'follow' is optional
				if (camera.contains("follow")) {
					m_camera_follow_player = camera.at("follow").as_bool();
				}
				// 'target' is optional
				if (camera.contains("target")) {
					std::vector<float> target = std::move(value_to<std::vector<float>>(camera.at("target")));
					m_camera_target = btVector3 (target[0], target[1], target[2]);
				} else {
					// find a reasonable default
					if (should_camera_follow_player() && m_camera_pos != btVector3(0.f, 0.f, 0.f)) {
						// look at the center of the player vehicle
						m_camera_target = btVector3(0.f, 0.f, 0.f);
					} else {
						// look in +z direction
						m_camera_target = m_camera_pos + btVector3(0.f, 0.f, 1.f);
					}
				}
			}
		}
	} catch (std::exception const& e) {
		std::string msg = "failed to create scene: ";
		msg += e.what();
		std::perror(msg.c_str());
		return false;
	}
	return true;
}
