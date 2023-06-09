# Vehicle Simulation

![App screen](app_screen.png "App screen")

This simulation program lets you build a 3D world and drive a vehicle around to explore it.

## How to build

1. You need Visual Studio 2022 to build the simulation program. Get the community edition [here](https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community&channel=Release&version=VS2022&source=VSLandingPage&cid=2030&passive=false).

2. Run the [bootstrap.cmd](bootstrap.cmd) script in a command prompt window from the top of your working tree to download and build all third party libraries. A ***third_party*** directory will be created above the working directory so it can be shared among multiple projects.

3. Open [veh-sim.sln](veh-sim.sln), build solution and run.

4. Run from the Windows command prompt:

```bat
cd <top of your local working directory>
cd x64\releases
veh-sim.exe ..\..\scene_desc.json
```

5. Read the [documentation](docs/scene_desc.md) and see the [examples](docs/examples.json) on how to build your own scene.

## How to play

Vehicle movements:

* **Up arrow**: move forward
* **Down arrow**: move backward
* **Left arrow**: turn left
* **Right arrow**: turn right
* **Space**: brake
* **Enter**: fire a shell
* **Mouse movement**: look around
* **Right mouse button + mouse movement**: aim
* **Left mouse button**: fire bullets
* **ESC**: exit

Camera movements when it is not following the player vehicle:

* **W**: move forward
* **S**: move backward
* **A**: side step left
* **D**: side step right
* **Page Up**: increase elevation
* **Page Down**: decrease elevation
* **Mouse movement**: look around

## Resources

* [LearnOpenGL](https://learnopengl.com/): Very useful website for graphics programming with OpenGL, creating 2D games, and using sounds
* [Bullet 3D Physics](https://pybullet.org/): 3D physics library
* [GLFW](https://www.glfw.org/): Library for OpenGL API
* [glm](https://glm.g-truc.net/): OpenGL Mathematics library
* [stb](https://github.com/nothings/stb): Single-file public domain libraries for C/C++
* [rapidjson](https://github.com/Tencent/rapidjson): A JSON parser/generator for C++
* [JSON](https://www.json.org/): Introduction to JSON 
* [JSON lint](https://jsonlint.com/): JSON validator and reformatter

## Feedback

File an [issue](https://github.com/isaacchou/veh-sim/issues) to report a bug or request a feature, or start a [discussion](https://github.com/isaacchou/veh-sim/discussions) for questions. Please observe the [GitHub Community Code of Conduct](https://docs.github.com/en/site-policy/github-terms/github-community-code-of-conduct) while using this website.

## License

Licensed under the [MIT License](LICENSE).
