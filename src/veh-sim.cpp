/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include "Simulation/GameWorld.h"
#include "Interface/OpenGLRenderer.h"
#include "PlayerProtocol.h"

int run_local(const char* scene_pathname)
{
	GameWorld game;
	if (!game.create_scene_from_file(scene_pathname))
		return 1;
	
	OpenGLRenderer renderer;
	renderer.init("veh-sim");
	// players and observers can only join after the scene creation
	// so all texture images can be sent to the local renderer
	btVector3 eye = game.get_camera_pos();
	btVector3 target = game.get_camera_target();
	renderer.setup_camera(game.should_camera_follow_player(),
						  glm::vec3(eye.x(), eye.y(), eye.z()), 
						  glm::vec3(target.x(), target.y(), target.z()));
	game.get_scene_observer().connect(&renderer);
	game.run("veh-sim");
	
	renderer.teardown();
	return 0;
}

int run_server(const std::string& server_opt, const char* scene_pathname)
{
	try
	{
		GameWorld game;
		if (!game.create_scene_from_file(scene_pathname))
			return 1;

		int n = game.how_many_players();
		if (n == 0) {
			printf("No player specified in the scene.\n");
			return 1;
		}
		int port = std::stoi(server_opt);
		PlayerServer server(port);
		printf("Listening on port: %d\n", port);
		printf("This is a %d player game. Accepting players...\n", n);
		for (int i = 0; i < n; i++) {
			server.accept_player();
			printf("Player #%d joined the game!\n", i + 1);
		}
		// players and observers can only join after the scene creation
		// so all texture images can be sent to the local renderer
		btVector3 eye = game.get_camera_pos();
		btVector3 target = game.get_camera_target();
		server.setup_camera(game.should_camera_follow_player(),
							glm::vec3(eye.x(), eye.y(), eye.z()), 
							glm::vec3(target.x(), target.y(), target.z()));
		game.get_scene_observer().connect(&server);
		game.run("veh-sim");
		server.disconnect();
	}
	catch (std::exception& e)
	{
		printf("Error: %s\n", e.what());
	}
	return 0;
}

int run_client(const std::string& server_opt)
{
	OpenGLRenderer renderer;
	renderer.init("veh-sim");

	try
	{
		PlayerClient player(renderer);
		size_t offset = server_opt.find_first_of(':');
		std::string host = server_opt.substr(0, offset);
		std::string port = server_opt.substr(offset + 1);
		player.join(host.c_str(), port.c_str());
		for (bool cont = true; cont; ) {
			cont = player.communicate();
		}
	} catch (std::exception& e) {
		printf("Error: %s\n", e.what());
	}
	renderer.teardown();
	return 0;
}

int main(int argc, char *argv[])
{	// program options:
	// <path to scene file>
	// server=<hostname>:<port> <path to scene file>
	// client=<server>:<port>
	if (argc == 2) {
		std::string arg = argv[1];
		const std::string client_opt = "join=";
		if (arg.starts_with(client_opt)) {
			return run_client(arg.substr(client_opt.size()));
		} else {
			return run_local(arg.c_str());
		}
	} else if (argc == 3) {
		std::string arg = argv[1];
		const std::string server_opt = "server=";
		if (arg.starts_with(server_opt)) {
			int ret = 0;
			do {
				ret = run_server(arg.substr(server_opt.size()), argv[2]);
			} while(ret == 0);
		}
	}
	// show usage:
	printf("Usage:\n");
	printf("Run locally: veh-sim <path to a scene json file>\n");
	printf("Run as a game server: veh-sim server=<port number> <path to a scene json file>\n");
	printf("Join a game server: veh-sim join=<server hostname or IPv4 address>:<port number>\n");
	return 1;
}
