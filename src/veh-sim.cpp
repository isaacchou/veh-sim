/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include "Simulation/GameWorld.h"
#include "Interface/OpenGLRenderer.h"

int main(int argc, char *argv[])
{	
	if (argc != 2) {
		printf("Please specify a scene description json file: veh-sim.exe <scene_desc.json>\n");
		return 1;
	}
	
	GameWorld game;
	if (!game.create_scene_from_file(argv[1]))
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
