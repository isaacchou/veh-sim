/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include "Interface/Renderer.h"
#include "Interface/Controller.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

class websocket_session {
	beast::flat_buffer buffer_;
    websocket::stream<beast::tcp_stream> ws_;

public:
	websocket_session(tcp::socket&& socket) : ws_(std::move(socket)) {}
	~websocket_session() {}

	void close() { ws_.close(websocket::close_code::normal); }

	void send_msg(const std::string& msg) {
		boost::system::error_code error;
		ws_.write(net::buffer(msg.c_str(), msg.size()), error);
		if (error) {
			throw boost::system::system_error(error);
		}
	}
	
	std::string read_msg() {
		boost::system::error_code error;	
		size_t n = ws_.read(buffer_, error);	
		if (error) {
			throw boost::system::system_error(error);
		}
		std::string msg;
		msg.assign((const char*)buffer_.cdata().data(), n);
		buffer_.consume(n);
		return std::move(msg);
	}

	void init_handshake(const char* host, const char* port) {
		std::string host_port = host;
		host_port += ':';
		host_port += port;
		ws_.handshake(host_port, "/");
		ws_.text(true);
	}

	void accept_handshake() {
		ws_.accept();
		ws_.text(true);
	}
};

class PlayerProtocol
{
protected:
	net::io_context m_io_context;
	std::vector<websocket_session*> m_websockets;
	Controller m_controller;

	// Player protocol has two flavors, send-all and send and read individually
	// The server does mostly send-all (broadcast) with multiple clients
	// while the client only communicates with one server
	PlayerProtocol() {}	
	virtual ~PlayerProtocol() { 
		for (auto& session : m_websockets) {
			delete session;
		}
	}

	void send_all(const std::string& msg) { 
		for (auto& session : m_websockets) {
			// no buffering to avoid data fragmentation!!!
			session->send_msg(msg);
		}
	}
};

class PlayerServer : public PlayerProtocol, public Renderer
{
protected:	
	tcp::endpoint m_endpoint;
	tcp::acceptor m_acceptor;

	glm::vec3 m_camera_pos, m_camera_target;
	bool m_camera_follow_player;

public:
	PlayerServer(int port) : m_endpoint(tcp::v4(), port), m_acceptor(m_io_context, m_endpoint),
							 m_camera_pos(0.f, 0.f, 0.f), m_camera_target(0.f, 0.f, 1.f), m_camera_follow_player(false) {}
	virtual ~PlayerServer() {}

	void accept_player();
	virtual int how_many_controllers() { return (int)m_websockets.size(); }
	virtual void setup_camera(bool follow, 
		const glm::vec3& eye, const glm::vec3& target) {
		m_camera_follow_player = follow;
		m_camera_pos = eye;
		m_camera_target = target;
	}
	virtual void pre_connect();
	virtual void post_connect() {};
	virtual void begin_update() {};
	virtual bool end_update(float elapsed_time);
	virtual void setup_camera();
	virtual void add_texture(int id, size_t width, size_t height, unsigned char* data);
	virtual Controller& get_controller(int player_id);
	virtual void set_player_transform(int player_id, const glm::mat4& trans);
	virtual void add_shape(int id, const char* json);
	virtual void update_shape(int id, const glm::mat4& trans);
	virtual void remove_shape(int id);
	void disconnect();
};

class PlayerClient : public PlayerProtocol
{
protected:
	Renderer& m_renderer;
	tcp::resolver m_resolver;
	int m_player_id;

public:
	PlayerClient(Renderer& renderer) : 
		m_renderer(renderer), m_player_id(-1), m_resolver(m_io_context) {}
	virtual ~PlayerClient() {}

	void join(const char* host, const char* port);
	bool communicate();
};
