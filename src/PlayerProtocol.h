/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <algorithm>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include "Interface/Renderer.h"

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

	void send_data(const void* data, size_t size) {
		boost::system::error_code error;
		ws_.write(net::buffer(data, size), error);
		if (error) {
			throw boost::system::system_error(error);
		}
	}
	
	void read_data(void* data, size_t size) {
		if (buffer_.size() >= size) {
			memcpy(data, buffer_.cdata().data(), size);
			buffer_.consume(size);
		} else {
			boost::system::error_code error;	
			size_t n = ws_.read(buffer_, error);	
			if (error) {
				throw boost::system::system_error(error);
			}
			read_data(data, size);
		}		
	}

	template <typename T>
	void send(T val) {
		boost::array<T, 1> buf = {val};
		send_data(buf.data(), sizeof(T));
	}

	template <typename T>
	T read() {
		boost::array<T, 1> buf;
		read_data(buf.data(), sizeof(T));
		return buf[0];
	}

	void init_handshake(const char* host, const char* port) {
		std::string host_port = host;
		host_port += ':';
		host_port += port;
		ws_.handshake(host_port, "/");
		ws_.binary(true);
	}

	void accept_handshake() {
		ws_.accept();
		ws_.binary(true);
	}
};

class PlayerProtocol
{
protected:
	enum class Command {
		begin_setup,
		end_setup,
		begin_update,
		end_update,
		setup_camera,
		add_texture,
		add_shape,
		update_shape,
		remove_shape,
		set_player_transform,
		get_controller,
		end
	};
	net::io_context m_io_context;
	std::vector<websocket_session*> m_websockets;
	Controller m_controller;

	// Player protocol has two flavors, send-all and send and read individually
	// The server does mostly send-all (broadcast) with multiple clients
	// while the client only communicates with one server
	unsigned char* m_buffer;
	const size_t m_max_buffer_size;
	size_t m_bytes_in_buffer;

	PlayerProtocol() : m_max_buffer_size(1024 * 8), m_bytes_in_buffer(0) {
		m_buffer = new unsigned char[m_max_buffer_size];
	}
	
	virtual ~PlayerProtocol() { 
		delete[] m_buffer;
		for (auto& session : m_websockets) {
			delete session;
		}
	}

	void send_all(const void* data, size_t size) { 
		const unsigned char* p = (const unsigned char*)data;
		while (size > 0) {
			if (m_bytes_in_buffer == m_max_buffer_size) flush();
			
			size_t n = std::min(m_max_buffer_size - m_bytes_in_buffer, size);
			memcpy(m_buffer + m_bytes_in_buffer, p, n);
			m_bytes_in_buffer += n;
			size -= n;
			p += n;
		}
	}
	
	template <typename T> void send_all(T val) {
		send_all((const void*)&val, sizeof(T));
	}

	void flush() {
		if (m_bytes_in_buffer > 0) {
			for (auto& session : m_websockets) {
				// send to all
				session->send_data(m_buffer, m_bytes_in_buffer);
			}
			m_bytes_in_buffer = 0;
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

	void accept_player() {
		tcp::socket socket(m_io_context);
		m_acceptor.accept(socket);
		socket.set_option(tcp::no_delay(true));
		
		websocket_session* session = new websocket_session(std::move(socket));
		session->accept_handshake();
		m_websockets.push_back(session);

		int player_id = (int)m_websockets.size() - 1;
		session->send<int>(player_id);
	}
	virtual int how_many_controllers() { return (int)m_websockets.size(); }
	virtual void setup_camera(bool follow, const glm::vec3& eye, const glm::vec3& target) {
		m_camera_follow_player = follow;
		m_camera_pos = eye;
		m_camera_target = target;
	}
	virtual void pre_connect() { send_all<Command>(Command::begin_setup); setup_camera(); }
	virtual void post_connect() { send_all<Command>(Command::end_setup); flush(); }
	virtual void begin_update() { send_all<Command>(Command::begin_update); }
	virtual bool end_update(float elapsed_time) {
		send_all<Command>(Command::end_update);
		send_all<float>(elapsed_time);
		flush();

		bool ret = true;
		for (auto& session : m_websockets) {
			ret = ret && session->read<bool>();
		}
		return ret;
	}
	virtual void setup_camera() {
		send_all<Command>(Command::setup_camera);
		send_all<bool>(m_camera_follow_player);
		send_all<glm::vec3&>(m_camera_pos);
		send_all<glm::vec3&>(m_camera_target);
	}
	virtual void add_texture(int id, size_t width, size_t height, unsigned char* data) {
		send_all<Command>(Command::add_texture);
		send_all<int>(id);
		send_all<size_t>(width);
		send_all<size_t>(height);
		size_t row_bytes = ((((width * 3) + 3) >> 2) << 2);
		send_all(data, row_bytes * height);
	}	
	virtual void add_shape(int id, const char* json) {
		send_all<Command>(Command::add_shape);
		send_all<int>(id);
		// send the null-terminated json string
		size_t n = strlen(json) + 1;
		send_all<int>((int)n);
		send_all(json, n);	
	}
	virtual void update_shape(int id, const glm::mat4& trans) {
		send_all<Command>(Command::update_shape);
		send_all<int>(id);
		send_all<const glm::mat4&>(trans);
	}
	virtual void remove_shape(int id) {
		send_all<Command>(Command::remove_shape);
		send_all<int>(id);
	}
	virtual void set_player_transform(int player_id, const glm::mat4& trans) {
		send_all<Command>(Command::set_player_transform);
		send_all<int>(player_id);
		send_all<const glm::mat4&>(trans);
	}
	virtual Controller& get_controller(int player_id) {
		flush();
		auto& session = m_websockets[player_id];
		session->send<Command>(Command::get_controller);

		m_controller.m_keyboard.clear();
		size_t n = session->read<size_t>();
		for (int i = 0; i < n; i++) {
			m_controller.m_keyboard.insert(session->read<int>());
		}

		m_controller.m_mouse.clear();
		n = session->read<size_t>();
		for (int i = 0; i < n; i++) {
			m_controller.m_mouse.insert(session->read<int>());
		}
		m_controller.m_cursor_cur_pos = session->read<glm::vec2>();
		m_controller.m_cursor_last_pos = session->read<glm::vec2>();
		m_controller.m_scroll_pos = session->read<glm::vec2>();
		return m_controller;
	}
	void disconnect() {
		// tell all players to disconnect
		send_all<Command>(Command::end);
		flush();
		
		for (auto& session : m_websockets) {
			session->close();
		}
	}
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

	void join(const char* host, const char* port) {
		tcp::resolver::results_type endpoints = m_resolver.resolve(host, port);
		tcp::socket socket(m_io_context);
		net::connect(socket, endpoints);
		socket.set_option(tcp::no_delay(true));
		websocket_session* session = new websocket_session(std::move(socket));
		session->init_handshake(host, port);
		m_websockets.push_back(session);

		// server assigns a player id after connection
		m_player_id = session->read<int>();
	}
	bool communicate() {
		bool cont = true;
		auto& session = m_websockets[0];
		Command cmd = session->read<Command>();
		switch (cmd) {
			case Command::begin_setup:
				m_renderer.pre_connect();
			break;
			case Command::end_setup:
				m_renderer.post_connect();
				flush();
			break;
			case Command::begin_update:
				m_renderer.begin_update();
			break;
			case Command::end_update:
				session->send<bool>(m_renderer.end_update(session->read<float>()));
				flush();
			break;
			case Command::setup_camera:
			{
				bool follow = session->read<bool>();
				glm::vec3 eye = session->read<glm::vec3>();
				glm::vec3 target = session->read<glm::vec3>();
				m_renderer.setup_camera(follow, eye, target);
			}
			break;
			case Command::add_texture:
			{
				int id = session->read<int>();
				size_t width = session->read<size_t>();
				size_t height = session->read<size_t>();
				size_t row_bytes = ((((width * 3) + 3) >> 2) << 2);
				unsigned char* data = new unsigned char[row_bytes * height];
				session->read_data(data, row_bytes * height);
				m_renderer.add_texture(id, width, height, data);
				delete[] data;
			}
			break;
			case Command::add_shape:
			{
				int id = session->read<int>();
				int size = session->read<int>();
				char* buf = new char [size];
				session->read_data(buf, size);
				m_renderer.add_shape(id, buf);
				delete [] buf;
			}
			break;
			case Command::update_shape:
			{
				int id = session->read<int>();
				glm::mat4 m = session->read<glm::mat4>();
				m_renderer.update_shape(id, m);
			}
			break;
			case Command::remove_shape:
			{
				m_renderer.remove_shape(session->read<int>());
			}
			break;
			case Command::set_player_transform:
			{
				int player_id = session->read<int>();
				glm::mat4 m = session->read<glm::mat4>();
				if (player_id == m_player_id) {
					m_renderer.set_player_transform(0, m);
				}
			}
			break;
			case Command::get_controller:
			{
				const Controller& ctlr = m_renderer.get_controller(0);
				session->send<size_t>(ctlr.m_keyboard.size());
				for (int k : ctlr.m_keyboard) { session->send<int>(k); }
				
				session->send<size_t>(ctlr.m_mouse.size());
				for (int b : ctlr.m_mouse) { session->send<int>(b); }

				session->send(ctlr.m_cursor_cur_pos);
				session->send(ctlr.m_cursor_last_pos);
				session->send(ctlr.m_scroll_pos);
			}
			break;
			case Command::end:
				for (auto& session : m_websockets) {
					session->close();
				}
				cont = false;
			break;
		}
		return cont;
	}
};
