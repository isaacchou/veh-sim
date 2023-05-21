/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include "Simulation/GameWorld.h"
#include "Interface/OpenGLRenderer.h"

int main(void)
{	
	OpenGLRenderer renderer;
	renderer.init("veh-sim");
	
	GameWorld game;
	game.create_scene();
	game.add_tank(btVector3(30.f, 1.5f, -150.f));
	game.add_v150(btVector3(-30.f, 1.5f, -150.f));

	// players and observers can only join after the scene creation
	// so all texture images can be sent to the local renderer
	game.get_scene_observer().connect(&renderer);
	game.run("veh-sim");
	
	renderer.teardown();
}
