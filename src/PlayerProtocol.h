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
#include "Interface/Renderer.h"

using boost::asio::ip::tcp;

void send_data(tcp::socket& socket, const void* data, size_t size) {
	boost::system::error_code error;
	boost::asio::write(socket, boost::asio::buffer(data, size), error);
	if (error) {
		throw boost::system::system_error(error);
	}
}

void read_data(tcp::socket& socket, void* data, size_t size) {
	boost::system::error_code error;
	boost::asio::read(socket, boost::asio::buffer(data, size),
					boost::asio::transfer_exactly(size), error);
	if (error) {
		throw boost::system::system_error(error);
	}
}

template <typename T>
void send_data(tcp::socket& socket, T val) {
	boost::array<T, 1> buf;
	buf[0] = val;
	boost::system::error_code error;
	boost::asio::write(socket, boost::asio::buffer(buf), error);
	if (error) {
		throw boost::system::system_error(error);
	}
}

template <typename T>
T read_data(tcp::socket& socket) {
	boost::array<T, 1> buf;
	boost::system::error_code error;
	boost::asio::read(socket, boost::asio::buffer(buf),
					boost::asio::transfer_exactly(sizeof(T)), error);
	if (error) {
		throw boost::system::system_error(error);
	}
	return buf[0];
}

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
	boost::asio::io_context m_io_context;
	std::vector<tcp::socket> m_sockets;
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
	}

	void send(const void* data, size_t size) { 
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

	// methods to communicate with a single player
	// always flush the broadcast (send-all) buffer before individual communications
	template <typename T> void send(int which, T val) { send(which, (const void*)&val, sizeof(T)); }
	void send(int which, const void* data, size_t size) {
		flush();
		send_data(m_sockets[which], data, size);
	}
	void read(int which, void* data, size_t size) { 
		flush();
		read_data(m_sockets[which], data, size); 
	}
	
	void flush() {
		if (m_bytes_in_buffer > 0) {
			for (auto& socket : m_sockets) {
				// send to all
				send_data(socket, m_buffer, m_bytes_in_buffer);
			}
			m_bytes_in_buffer = 0;
		}
	}

	template <typename T> void send(T val) { send((const void*)&val, sizeof(T)); }
	template <typename T> T read(int which) { T val; read(which, (void*)&val, sizeof(T)); return val; }
};

class PlayerServer : public PlayerProtocol, public Renderer
{
protected:	
	boost::asio::ip::tcp::endpoint m_endpoint;
	tcp::acceptor m_acceptor;

	glm::vec3 m_camera_pos, m_camera_target;
	bool m_camera_follow_player;

public:
	PlayerServer(int port) : m_endpoint(boost::asio::ip::tcp::v4(), port), m_acceptor(m_io_context, m_endpoint),
							 m_camera_pos(0.f, 0.f, 0.f), m_camera_target(0.f, 0.f, 1.f), m_camera_follow_player(false) {}
	virtual ~PlayerServer() {}

	void accept_player() {
		tcp::socket socket(m_io_context);
		m_acceptor.accept(socket);
		socket.set_option(boost::asio::ip::tcp::no_delay(true));
		m_sockets.push_back(std::move(socket));
		
		int player_id = (int)m_sockets.size() - 1;
		send<int>(player_id, player_id);
	}
	virtual int how_many_controllers() { return (int)m_sockets.size(); }
	virtual void setup_camera(bool follow, const glm::vec3& eye, const glm::vec3& target) {
		m_camera_follow_player = follow;
		m_camera_pos = eye;
		m_camera_target = target;
	}
	virtual void pre_connect() { send<Command>(Command::begin_setup); setup_camera(); }
	virtual void post_connect() { send<Command>(Command::end_setup); flush(); }
	virtual void begin_update() { send<Command>(Command::begin_update); }
	virtual bool end_update(float elapsed_time) {
		send<Command>(Command::end_update);
		send<float>(elapsed_time);
		flush();

		bool ret = true;
		int n = how_many_controllers();
		for (int i = 0; i < n; i++) {
			ret = ret && read<bool>(i);
		}
		return ret;
	}
	virtual void setup_camera() {
		send<Command>(Command::setup_camera);
		send<bool>(m_camera_follow_player);
		send<glm::vec3&>(m_camera_pos);
		send<glm::vec3&>(m_camera_target);
	}
	virtual void add_texture(int id, size_t width, size_t height, unsigned char* data) {
		send<Command>(Command::add_texture);
		send<int>(id);
		send<size_t>(width);
		send<size_t>(height);
		size_t row_bytes = ((((width * 3) + 3) >> 2) << 2);
		send(data, row_bytes * height);
	}
	
	void send_shape_desc(const ShapeDesc& shape_desc, const glm::mat4& trans) {
		send<ShapeDesc::Type>(shape_desc.m_type);
		send((void*)shape_desc.m_param, 4 * sizeof(float));
		send<unsigned int>(shape_desc.m_default_texture);
		send<size_t>(shape_desc.m_textures.size());
		for (unsigned int t : shape_desc.m_textures) {
			send<unsigned int>(t);
		}
		send<const glm::mat4&>(trans);

		if (shape_desc.m_type == ShapeDesc::Type::Compound) {
			const CompoundShapeDesc& compound = dynamic_cast<const CompoundShapeDesc&>(shape_desc);
			const std::vector<ShapeDesc::child_desc>& child_desc = compound.get_child_shape_desc();
			send<size_t>(child_desc.size());
			for (auto& desc : child_desc) {
				send_shape_desc(*desc.m_desc, desc.m_trans);
			}
		}
	}
	virtual void add_shape(int id, const ShapeDesc& shape_desc, const glm::mat4& trans) {
		send<Command>(Command::add_shape);
		send<int>(id);
		send_shape_desc(shape_desc, trans);
	}
	virtual void update_shape(int id, const glm::mat4& trans) {
		send<Command>(Command::update_shape);
		send<int>(id);
		send<const glm::mat4&>(trans);
	}
	virtual void remove_shape(int id) {
		send<Command>(Command::remove_shape);
		send<int>(id);
	}
	virtual void set_player_transform(int which, const glm::mat4& trans) {
		send<Command>(Command::set_player_transform);
		send<int>(which);
		send<const glm::mat4&>(trans);
	}
	virtual Controller& get_controller(int which) {
		
		send<Command>(which, Command::get_controller);

		m_controller.m_keyboard.clear();
		size_t n = read<size_t>(which);
		for (int i = 0; i < n; i++) {
			m_controller.m_keyboard.insert(read<int>(which));
		}

		m_controller.m_mouse.clear();
		n = read<size_t>(which);
		for (int i = 0; i < n; i++) {
			m_controller.m_mouse.insert(read<int>(which));
		}
		m_controller.m_cursor_cur_pos = read<glm::vec2>(which);
		m_controller.m_cursor_last_pos = read<glm::vec2>(which);
		m_controller.m_scroll_pos = read<glm::vec2>(which);
		return m_controller;
	}
	void disconnect() {
		// tell all players to disconnect
		send<Command>(Command::end);		
		int n = how_many_controllers();
		for (int i = 0; i < n; i++) {
			// wait for all players to acknowledge
			// before closing the connection
			read<bool>(i);
			m_sockets[i].close();
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
		boost::asio::connect(socket, endpoints);
		socket.set_option(boost::asio::ip::tcp::no_delay(true));
		m_sockets.push_back(std::move(socket));
		// server assigns a player id after connection
		m_player_id = read<int>(0);
	}

	ShapeDesc* read_shape_desc(glm::mat4& m) {
		
		ShapeDesc::Type type = read<ShapeDesc::Type>(0);
		float param[4];
		read(0, (void*)param, 4 * sizeof(float));

		ShapeDesc* desc = NULL;
		CompoundShapeDesc* compound = NULL;
		switch (type)
		{
			case ShapeDesc::Type::Compound:
				compound = new CompoundShapeDesc();
				desc = compound;
			break;
			case ShapeDesc::Type::Pyramid:
				desc = new PyramidShapeDesc(param[0], param[1], param[2]);			
			break;
			case ShapeDesc::Type::Wedge:
				desc = new WedgeShapeDesc(param[0], param[1], param[2], param[3]);
			break;
			case ShapeDesc::Type::V150:
				desc = new V150(param[0]);
			break;
			default:
				desc = new ShapeDesc();
			break;
		}
		desc->m_type = type;
		desc->m_param[0] = param[0];
		desc->m_param[1] = param[1];
		desc->m_param[2] = param[2];
		desc->m_param[3] = param[3];
		desc->m_default_texture = read<unsigned int>(0);
		size_t n = read<size_t>(0);
		for (int i = 0; i < n; i++) {
			desc->m_textures.push_back(read<unsigned int>(0));
		}
		m = read<glm::mat4>(0);

		if (compound != NULL) {
			n = read<size_t>(0);
			for (int i = 0; i < n; i++) {
				glm::mat4 mm;
				compound->add_child_shape_desc(read_shape_desc(mm), mm);
			}
		}
		return desc;
	}

	bool communicate() {
		bool cont = true;
		Command cmd = read<Command>(0);
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
				send<bool>(m_renderer.end_update(read<float>(0)));
				flush();
			break;
			case Command::setup_camera:
			{
				bool follow = read<bool>(0);
				glm::vec3 eye = read<glm::vec3>(0);
				glm::vec3 target = read<glm::vec3>(0);
				m_renderer.setup_camera(follow, eye, target);
			}
			break;
			case Command::add_texture:
			{
				int id = read<int>(0);
				size_t width = read<size_t>(0);
				size_t height = read<size_t>(0);
				size_t row_bytes = ((((width * 3) + 3) >> 2) << 2);
				unsigned char* data = new unsigned char[row_bytes * height];
				read(0, data, row_bytes * height);
				m_renderer.add_texture(id, width, height, data);
				delete[] data;
			}
			break;
			case Command::add_shape:
			{
				int id = read<int>(0);
				glm::mat4 m;
				ShapeDesc* desc = read_shape_desc(m);
				m_renderer.add_shape(id, *desc, m);
				delete desc;
			}
			break;
			case Command::update_shape:
			{
				int id = read<int>(0);
				glm::mat4 m = read<glm::mat4>(0);
				m_renderer.update_shape(id, m);
			}
			break;
			case Command::remove_shape:
			{
				m_renderer.remove_shape(read<int>(0));
			}
			break;
			case Command::set_player_transform:
			{
				int which = read<int>(0);
				glm::mat4 m = read<glm::mat4>(0);
				if (which == m_player_id) {
					m_renderer.set_player_transform(0, m);
				}
			}
			break;
			case Command::get_controller:
			{
				const Controller& ctlr = m_renderer.get_controller(0);
				// use the send-all method here to take advantage of the buffering	
				send<size_t>(ctlr.m_keyboard.size());
				for (int k : ctlr.m_keyboard) { send<int>(k); }
				
				send<size_t>(ctlr.m_mouse.size());
				for (int b : ctlr.m_mouse) { send<int>(b); }

				send(ctlr.m_cursor_cur_pos);
				send(ctlr.m_cursor_last_pos);
				send(ctlr.m_scroll_pos);
				flush();
			}
			break;
			case Command::end:
				send<bool>(true);
				flush();
				m_sockets[0].close();
				cont = false;
			break;
		}
		return cont;
	}
};
