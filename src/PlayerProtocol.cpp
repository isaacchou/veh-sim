/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <boost/json.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include "PlayerProtocol.h"

namespace json = boost::json;
namespace base64 = boost::beast::detail::base64;

json::array to_json_array(const float* v, int n)
{
	json::array a;
	for (int i = 0; i < n; i++) {
		a.emplace_back(v[i]);
	}
	return std::move(a);
}

json::array to_json_array(const glm::vec2& v)
{
	return to_json_array(&v[0], sizeof(v)/sizeof(float));
}

json::array to_json_array(const glm::vec3& v)
{
	return to_json_array(&v[0], sizeof(v)/sizeof(float));
}

json::array to_json_array(const glm::mat4& m)
{
	return to_json_array(&m[0][0], sizeof(m)/sizeof(float));
}

void from_json_array(glm::vec2& v, const json::array& a)
{
	v.x = (float)a[0].as_double();
	v.y = (float)a[1].as_double();
}

void from_json_array(glm::vec3& v, const json::array& a)
{
	v.x = (float)a[0].as_double();
	v.y = (float)a[1].as_double();
	v.z = (float)a[2].as_double();
}

void from_json_array(glm::mat4& m, const json::array& a)
{
	float* p = &m[0][0];
	int n = sizeof(m)/sizeof(float);
	for (const auto& i : a) {
		*p++ = (float)i.as_double();
	}
}

//~~~
// PlayerServer
//~~~
void PlayerServer::accept_player()
{
	tcp::socket socket(m_io_context);
	m_acceptor.accept(socket);
	socket.set_option(tcp::no_delay(true));
		
	websocket_session* session = new websocket_session(std::move(socket));
	session->accept_handshake();
	m_websockets.push_back(session);

	int player_id = (int)m_websockets.size() - 1;
	// send player ID after connection
	json::value v =
	{ 
		{"cmd", "set_player_id"},
		{"player_id", player_id}
	};
	std::string msg = json::serialize(v);
	session->send_msg(msg);
}

void PlayerServer::pre_connect()
{
	setup_camera();
}

void PlayerServer::setup_camera()
{
	json::value v = 
	{ 
		{"cmd", "setup_camera"},
		{"eye", to_json_array(m_camera_pos)},
		{"target", to_json_array(m_camera_target)},
		{"follow", m_camera_follow_player}
	};
	std::string msg = json::serialize(v);
	send_all(msg);
}

void PlayerServer::add_texture(int id, size_t width, size_t height, unsigned char* data)
{
	size_t row_bytes = ((((width * 3) + 3) >> 2) << 2);
	size_t n = row_bytes * height;
	size_t len = base64::encoded_size(n);
	char* p = new char[len];
	len = base64::encode(p, data, n);	
	json::value v = 
	{ 
		{"cmd", "add_texture"},
		{"id", id},
		{"width", width},
		{"height", height},
		{"data", std::string_view(p, len)}
	};
	std::string msg = json::serialize(v);
	send_all(msg);
	delete [] p;
}	

Controller& PlayerServer::get_controller(int player_id)
{
	auto& session = m_websockets[player_id];

	json::value v = {{"cmd", "get_controller"}};
	session->send_msg(json::serialize(v));
	//
	//--- change direction
	//
	json::value r = json::parse(session->read_msg());
	m_controller.m_keyboard.clear();
	const json::array& keyboard = r.at("keyboard").as_array();
	size_t n = keyboard.size();
	for (int i = 0; i < n; i++) {
		m_controller.m_keyboard.insert((int)keyboard.at(i).as_int64());
	}

	m_controller.m_mouse.clear();
	const json::array& mouse = r.at("mouse").as_array();
	n = mouse.size();
	for (int i = 0; i < n; i++) {
		m_controller.m_mouse.insert((int)mouse.at(i).as_int64());
	}
	from_json_array(m_controller.m_cursor_cur_pos, r.at("cursor_cur_pos").as_array());
	from_json_array(m_controller.m_cursor_last_pos, r.at("cursor_last_pos").as_array());
	from_json_array(m_controller.m_scroll_pos, r.at("cursor_scroll_pos").as_array());
	return m_controller;
}

void PlayerServer::set_player_transform(int player_id, const glm::mat4& trans) 
{
	json::value v = {
		{"cmd", "set_player_transform"},
		{"player_id", player_id},
		{"trans", to_json_array(trans)}
	};
	std::string msg = json::serialize(v);
	send_all(msg);	
}

void PlayerServer::add_shape(int id, const char* json)
{
	json::value v = {
		{"cmd", "add_shape"},
		{"shape_id", id},
		{"descriptor", json::parse(json)}
	};
	std::string msg = json::serialize(v);
	send_all(msg);
}

void PlayerServer::update_shape(int id, const glm::mat4& trans)
{
	json::value v = {
		{"cmd", "update_shape"},
		{"shape_id", id},
		{"trans", to_json_array(trans)}
	};
	std::string msg = json::serialize(v);
	send_all(msg);
}

void PlayerServer::remove_shape(int id)
{
	json::value v = {
		{"cmd", "remove_shape"},
		{"shape_id", id}
	};
	std::string msg = json::serialize(v);
	send_all(msg);
}

bool PlayerServer::end_update(float elapsed_time)
{
	json::value v = {
		{"cmd", "end_update"},
		{"elapsed_time", elapsed_time}
	};
	send_all(json::serialize(v));

	bool ret = true;
	for (auto& session : m_websockets) {
		std::string msg = session->read_msg();
		json::value r = json::parse(msg);
		ret = ret && r.at("continue").as_bool();
	}
	return ret;
}

void PlayerServer::disconnect()
{	// tell all players to disconnect
	json::value v = {{"cmd", "end"}};
	std::string msg = json::serialize(v);
	send_all(msg);
		
	for (auto& session : m_websockets) {
		session->close();
	}
}

//~~~
// PlayerClient
//~~~
void PlayerClient::join(const char* host, const char* port)
{
	tcp::resolver::results_type endpoints = m_resolver.resolve(host, port);
	tcp::socket socket(m_io_context);
	net::connect(socket, endpoints);
	socket.set_option(tcp::no_delay(true));
	websocket_session* session = new websocket_session(std::move(socket));
	session->init_handshake(host, port);
	m_websockets.push_back(session);
}

bool PlayerClient::communicate() 
{
	bool cont = true;
	auto& session = m_websockets[0];
	std::string s = session->read_msg();
	json::value msg = json::parse(s);
	const json::string cmd = msg.at("cmd").as_string();
	if (cmd == "set_player_id") {
		m_player_id = (int)msg.at("player_id").as_int64();
	} else if (cmd == "setup_camera") {
		bool follow = msg.at("follow").as_bool();
		glm::vec3 eye, target;
		from_json_array(eye, msg.at("eye").as_array());
		from_json_array(target, msg.at("target").as_array());
		m_renderer.setup_camera(follow, eye, target);
	} else if (cmd == "add_texture") {
		int id = (int)msg.at("id").as_int64();
		size_t width = (int)msg.at("width").as_int64();
		size_t height = (int)msg.at("height").as_int64();
		json::string data = msg.at("data").as_string();
		
		size_t n = data.size();
		size_t len = base64::decoded_size(n);
		unsigned char* p = new unsigned char [len];		
		base64::decode(p, data.c_str(), n);
		m_renderer.add_texture(id, width, height, p);
		delete [] p;
	} else if (cmd == "get_controller") {
		const Controller& ctlr = m_renderer.get_controller(0);
		json::array keyboard, mouse;
		for (int k : ctlr.m_keyboard) keyboard.emplace_back(k);
		for (int b : ctlr.m_mouse) mouse.emplace_back(b);
		json::value v = 
		{
			{"cmd", "set_controller"},
			{"keyboard", keyboard},
			{"mouse", mouse},
			{"cursor_cur_pos", to_json_array(ctlr.m_cursor_cur_pos)},
			{"cursor_last_pos", to_json_array(ctlr.m_cursor_last_pos)},
			{"cursor_scroll_pos", to_json_array(ctlr.m_scroll_pos)}

		};
		session->send_msg(json::serialize(v));
	}
	else if (cmd == "set_player_transform") {
		int player_id = (int)msg.at("player_id").as_int64();
		glm::mat4 m;
		from_json_array(m, msg.at("trans").as_array());
		if (player_id == m_player_id) {
			m_renderer.set_player_transform(0, m);
		}
	} else if (cmd == "add_shape") {
		int shape_id = (int)msg.at("shape_id").get_int64();
		json::value desc = msg.at("descriptor");
		m_renderer.add_shape(shape_id, json::serialize(desc).c_str());
	} else if (cmd == "update_shape") {
		int shape_id = (int)msg.at("shape_id").get_int64();
		glm::mat4 m;
		from_json_array(m, msg.at("trans").as_array());
		m_renderer.update_shape(shape_id, m);
	} else if (cmd == "remove_shape") {
		int shape_id = (int)msg.at("shape_id").get_int64();
		m_renderer.remove_shape(shape_id);
	} else if (cmd == "end_update") {
		float elapsed_time = (float)msg.at("elapsed_time").as_double();
		bool ret = m_renderer.end_update(elapsed_time);
		json::value r = {{"continue", ret}};
		session->send_msg(json::serialize(r));
	} else if (cmd == "end") {
		for (auto& session : m_websockets) {
			session->close();
		}
		cont = false;
	}
	return cont;
}
