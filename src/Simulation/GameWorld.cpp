/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <array>
#include <cstdio>
#include <rapidjson/filereadstream.h>
#include "GameWorld.h"

class JsonUtil
{ 
public:
	static std::vector<float> get_float_array(const rapidjson::Value& array_obj)
	{
		std::vector<float> vals;
		if (array_obj.IsArray()) {
			for (const auto& val : array_obj.GetArray()) {
				vals.push_back(val.GetFloat());
			}
		}
		return vals;
	}

	static bool has_member(const rapidjson::Value& obj, const char* name) { 
		return obj.FindMember(name) != obj.MemberEnd();
	}

	static bool has_object(const rapidjson::Value& obj, const char* name) {
		return has_member(obj, name) && obj[name].IsObject();
	}

	static bool has_array(const rapidjson::Value& obj, const char* name) {
		return has_member(obj, name) && obj[name].IsArray();
	}

	static bool has_string(const rapidjson::Value& obj, const char* name) {
		return has_member(obj, name) && obj[name].IsString();
	}

	static bool has_int(const rapidjson::Value& obj, const char* name) {
		return has_member(obj, name) && obj[name].IsNumber();
	}
	
	static bool has_float(const rapidjson::Value& obj, const char* name) {
		return has_member(obj, name) && obj[name].IsNumber();
	}

	static bool has_bool(const rapidjson::Value& obj, const char* name) {
		return has_member(obj, name) && obj[name].IsBool();
	}
};

ShapeDesc* GameWorld::create_shape_from_json(const rapidjson::Document& doc, const rapidjson::Value& shape_obj)
{
	if (shape_obj.IsString()) {
		const char* macro_name = shape_obj.GetString();
		if (JsonUtil::has_object(doc, "macros") && 
			JsonUtil::has_object(doc["macros"], macro_name)) {
			return create_shape_from_json(doc, doc["macros"][macro_name]);
		}
		return NULL;
	} else if (!shape_obj.IsObject() || !JsonUtil::has_string(shape_obj, "kind")) {
		return NULL;
	}

	const std::string kind = shape_obj["kind"].GetString();
	if (kind == "compound") {
		CompoundShapeDesc* compound_shape = NULL;
		if (JsonUtil::has_array(shape_obj, "child")) {
			compound_shape = new CompoundShapeDesc();
			for (const auto& child : shape_obj["child"].GetArray()) {
				ShapeDesc* child_shape = NULL;
				if (JsonUtil::has_member(child, "shape")) {
					child_shape = create_shape_from_json(doc, child["shape"]);
				}
				if (child_shape != NULL && JsonUtil::has_array(child, "origin")) {
					// for child shape, 'origin' is required while rotation optional
					std::vector<float> p = JsonUtil::get_float_array(child["origin"]);
					std::vector<float> r{ 1.f, 0.f, 0.f, 0.f };
					if (JsonUtil::has_array(child, "rotation")) {
						r = JsonUtil::get_float_array(child["rotation"]);
					}
					compound_shape->add_child_shape_desc(child_shape, 
						glm::vec3(p[0], p[1], p[2]),		// origin of child shape in the compound reference frame
						glm::vec3(r[0], r[1], r[2]), r[3]); // orientation of the child shape
				}
			}
			return compound_shape;
		}
	}
	
	if (!JsonUtil::has_array(shape_obj, "dimension")) return NULL;
	
	ShapeDesc* shape = NULL;
	std::vector<float> dimension = JsonUtil::get_float_array(shape_obj["dimension"]);
	if (kind == "ground") {
		shape = new GroundShapeDesc(dimension[0], dimension[1]);
	} else if (kind == "box") {
		shape = new BoxShapeDesc(dimension[0], dimension[1], dimension[2]);
	} else if (kind == "sphere") {
		shape = new SphereShapeDesc(dimension[0]);
	} else if (kind == "cylinder") {
		shape = new CylinderShapeDesc(dimension[0], dimension[1]);
	} else if (kind == "capsule") {
		shape = new CapsuleShapeDesc(dimension[0], dimension[1]);
	} else if (kind == "cone") {
		shape = new ConeShapeDesc(dimension[0], dimension[1]);
	} else if (kind == "pyramid") {
		shape = new PyramidShapeDesc(dimension[0], dimension[1], dimension[2]);
	} else if (kind == "wedge") {
		shape = new WedgeShapeDesc(dimension[0], dimension[1], dimension[2], dimension[3]);
	} else if (kind == "gear") {
		shape = CreateGearShapeDesc(dimension[0], dimension[1], (int)dimension[2]);
	}
	if (shape == NULL) return NULL;

	if (JsonUtil::has_array(shape_obj, "textures")) {
		for (const auto& t : shape_obj["textures"].GetArray()) {
			unsigned int texture = 0;
			if (JsonUtil::has_member(t, "file")) {
				texture = get_texture_map().from_file(t["file"].GetString());
			} else if (JsonUtil::has_member(t, "color")) {
				std::string clr = t["color"].GetString();
				texture = get_texture_map().solid_color(clr.c_str());
			} else if (JsonUtil::has_member(t, "checker_board")) {
				const rapidjson::Value& p = t["checker_board"].GetArray();
				texture = get_texture_map().checker_board(p[0].GetInt(), p[1].GetInt(), 
					Color(p[2].GetString()), Color(p[3].GetString()));
			} else if (JsonUtil::has_member(t, "diagonal_stripes")) {
				const rapidjson::Value& p = t["diagonal_stripes"].GetArray();
				texture = get_texture_map().diagonal_stripes(p[0].GetInt(), p[1].GetInt(), p[2].GetInt(), 
					Color(p[3].GetString()), Color(p[4].GetString()));
			} else if (JsonUtil::has_member(t, "vertical_stripes")) {
				const rapidjson::Value& p = t["vertical_stripes"].GetArray();
				texture = get_texture_map().vertical_stripes(p[0].GetInt(), 
					Color(p[1].GetString()), Color(p[2].GetString()));
			} else if (JsonUtil::has_member(t, "horizontal_stripes")) {
				const rapidjson::Value& p = t["horizontal_stripes"].GetArray();
				texture = get_texture_map().horizontal_stripes(p[0].GetInt(), 
					Color(p[1].GetString()), Color(p[2].GetString()));
			}

			int repeat = JsonUtil::has_int(t, "repeat") ? t["repeat"].GetInt() : 1;
			if (repeat == 0) {
				// set as default texture for all faces without a texture
				shape->set_texture(texture);
			} else {
				shape->add_texture(texture, repeat);
			}
		}
	}
	return shape;
}

bool GameWorld::create_scene_from_file(const char* filename)
{
	std::array<char, 4096> buf;
	FILE* f = std::fopen(filename, "rb");
	if (f == 0) {
		std::string msg = "failed to open scene file \'";
		msg += filename;
		msg += "\'";
		std::perror(msg.c_str());
		return false;
	}
	rapidjson::FileReadStream stream(f, buf.data(), buf.size());
	rapidjson::Document doc;
	doc.ParseStream(stream);
	fclose(f);

	if (JsonUtil::has_array(doc, "scene")) {
		for (const auto& obj : doc["scene"].GetArray()) {
			if (!obj.IsObject()) continue;
					
			if (JsonUtil::has_member(obj, "shape")) {
				ShapeDesc* shape = create_shape_from_json(doc, obj["shape"]);
				if (shape == NULL) continue;
				// 'origin' is required 
				if (!JsonUtil::has_array(obj, "origin")) continue;
				std::vector<float> origin = JsonUtil::get_float_array(obj["origin"]);

				// 'rotation' is optional 
				btQuaternion rotation(btVector3(1.f, 0.f, 0.f), 0.f);
				if (JsonUtil::has_array(obj, "rotation")) {
					std::vector<float> r = JsonUtil::get_float_array(obj["rotation"]);
					rotation = btQuaternion(btVector3(r[0], r[1], r[2]), glm::radians(r[3]));
				}	
				// 'mass' is optional
				float mass = JsonUtil::has_float(obj, "mass") ? obj["mass"].GetFloat() : 0.f; 
				createRigidBody(shape,
								btVector3(origin[0], origin[1], origin[2]),
								rotation, mass);
			}
		}
	}

	// player has to be processed before camera so we know if camera can and should follow the player
	if (JsonUtil::has_object(doc, "player")) {
		const auto& player = doc["player"];
		if (JsonUtil::has_string(player, "vehicle") && JsonUtil::has_array(player, "origin")) {
			std::vector<float> origin = JsonUtil::get_float_array(player["origin"]);
			const std::string vehicle = player["vehicle"].GetString();
			if (vehicle == "tank") {
				add_tank(btVector3(origin[0], origin[1], origin[2]));
			} else if (vehicle == "V150") {
				add_v150(btVector3(origin[0], origin[1], origin[2]));
			}
		}
	}

	if (JsonUtil::has_object(doc, "camera")) {
		const auto& camera = doc["camera"];
		if (JsonUtil::has_array(camera, "eye")) {
			// 'eye' is required for camera
			std::vector<float> pos = JsonUtil::get_float_array(camera["eye"]);
			m_camera_pos = btVector3 (pos[0], pos[1], pos[2]);
			// 'follow' is optional
			if (JsonUtil::has_bool(camera, "follow")) {
				m_camera_follow_player = camera["follow"].GetBool();
			}
			// 'target' is optional
			if (JsonUtil::has_array(camera, "target")) {
				std::vector<float> target = JsonUtil::get_float_array(camera["target"]);
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
	return true;
}
