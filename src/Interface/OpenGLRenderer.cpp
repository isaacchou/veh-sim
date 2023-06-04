/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <stdlib.h>
// The include file for GLAD includes the required OpenGL headers 
// behind the scenes (like GL/gl.h) so be sure to include GLAD before 
// other header files that require OpenGL (like GLFW).
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include "OpenGLRenderer.h"
#include "Shaders.h"
#include "../Utils.h"

#define FULLSCREEN_MODE 0

//~~~
// GLFWController
//~~~
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	GLFWController* controller = (GLFWController*)glfwGetWindowUserPointer(window);
	if (controller != NULL) controller->process_keyboard_input(key, scancode, action, mods);
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	GLFWController* controller = (GLFWController*)glfwGetWindowUserPointer(window);
	if (controller != NULL) controller->process_mouse_move(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	GLFWController* controller = (GLFWController*)glfwGetWindowUserPointer(window);
	if (controller != NULL) controller->process_mouse_button(button, action, mods);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	GLFWController* controller = (GLFWController*)glfwGetWindowUserPointer(window);
	if (controller != NULL) controller->process_mouse_wheel_scroll(xoffset, yoffset);
}

void GLFWController::register_callbacks(GLFWwindow* window)
{
	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// initialize the mouse movement tracking
	double x, y;
	glfwGetCursorPos(window, &x, &y);
	m_cursor_cur_pos = {(float)x, (float)y};
	m_cursor_last_pos = m_cursor_cur_pos;
}

void GLFWController::process_mouse_move(double xpos, double ypos)
{	// cursor coordinates on screen: top left is (0, 0) and bottom right (width, height) 
	m_cursor_cur_pos.x = (float)xpos;
	m_cursor_cur_pos.y = (float)ypos;
}

void GLFWController::process_mouse_button(int button, int action, int mods)
{
	if (action == GLFW_PRESS) {
		m_mouse.insert(button);
	} else if (action == GLFW_RELEASE) {
		m_mouse.erase(button);
	}
}

void GLFWController::process_mouse_wheel_scroll(double xoffset, double yoffset)
{	// cumulative until inquired
	m_scroll_pos += glm::vec2((float)xoffset, (float)yoffset);
}

void GLFWController::process_keyboard_input(int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS) {
		m_keyboard.insert(key);
	} else if (action == GLFW_RELEASE) {
		m_keyboard.erase(key);
	}
}

//~~~
// OpenGLRenderer
//~~~
Shape* OpenGLRenderer::create_shape_from_desc(const ShapeDesc& desc)
{
	if (desc.m_type == ShapeDesc::Type::Compound) {

		const CompoundShapeDesc& compound_desc = dynamic_cast<const CompoundShapeDesc&>(desc);
		
		CompoundShape* compound = new CompoundShape();
		compound->set_texture(desc.m_default_texture);
		for (auto& child : compound_desc.get_child_shape_desc()) {
			compound->add_child_shape(create_shape_from_desc(*child.m_desc), child.m_trans);
		}
		return compound;
	}
	
	SimpleShape* shape = NULL;
	switch (desc.m_type) {
		case ShapeDesc::Type::Ground:
			shape = new GroundShape(desc.m_param[0], desc.m_param[1]);
		break;
		case ShapeDesc::Type::Box:
			shape = new BoxShape(desc.m_param[0], desc.m_param[1], desc.m_param[2]);
		break;
		case ShapeDesc::Type::Sphere:
			shape = new SphereShape(desc.m_param[0]);
		break;
		case ShapeDesc::Type::Cylinder:
			shape = new CylinderShape(desc.m_param[0], desc.m_param[1]);
		break;
		case ShapeDesc::Type::Capsule:
			shape = new CapsuleShape(desc.m_param[0], desc.m_param[1]);
		break;
		case ShapeDesc::Type::Cone:
			shape = new ConeShape(desc.m_param[0], desc.m_param[1]);
		break;
		case ShapeDesc::Type::Pyramid:
		{
			const PyramidShapeDesc& pyramid_desc = dynamic_cast<const PyramidShapeDesc&>(desc);
			shape = new ConvexShape(pyramid_desc);
		}
		break;
		case ShapeDesc::Type::Wedge:
		{
			const WedgeShapeDesc& wedge_desc = dynamic_cast<const WedgeShapeDesc&>(desc);
			shape = new ConvexShape(wedge_desc);
		}
		break;
		case ShapeDesc::Type::V150:
		{
			const V150& v150_desc = dynamic_cast<const V150&>(desc);
			shape = new ConvexShape(v150_desc);
		}
		break;
		default:
			return NULL;
	}
	shape->set_texture(m_texture_id_map[desc.m_default_texture]);
	for (auto texture : desc.m_textures) {
		shape->add_texture(m_texture_id_map[texture]);
	}
	return shape;
}

void OpenGLRenderer::add_shape(int id, const ShapeDesc& shape_desc, const glm::mat4& trans)
{
	Shape* shape = create_shape_from_desc(shape_desc);
	if (shape != NULL) {
		shape->create_mesh();
		m_shapes.insert(std::make_pair(id, shape));
		m_trans.insert(std::make_pair(id, trans));
	}
}

void OpenGLRenderer::remove_shape(int id)
{
	if (m_shapes.find(id) == m_shapes.end()) {
		debug_log("removing a shape that does not exist\n");
		return;
	}
	delete m_shapes[id];
	m_shapes.erase(id);
	m_trans.erase(id);
}

void OpenGLRenderer::update_shape(int id, const glm::mat4& trans)
{
	m_trans[id] = trans;
}

static void reshape(GLFWwindow* window, int width, int height)
{	// called when the window is rezied
	glViewport(0, 0, width, height);
}

int OpenGLRenderer::init(const char* title)
{
	if (!glfwInit()) exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// create a main window in full screen mode
	int width = 2000, height = 1200;
	GLFWmonitor* monitor = NULL;
#if FULLSCREEN_MODE
	monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	width = mode->width;
	height = mode->height;
#endif
	GLFWwindow* window = glfwCreateWindow(width, height, title, monitor, NULL);
	if (window == NULL)
	{
		debug_log("Failed to create GLFW window\n");
		glfwTerminate();
		return EXIT_FAILURE;
	}
	m_window = window; // use 'void*' to avoid GLFW dependency in sub-classes
	glfwMakeContextCurrent(window);

	// glfwGetProcAddress requires a valid window or context to work
	// therefore GLAD can only be initialized after this point
	if (!gladLoadGL(glfwGetProcAddress))
	{
		debug_log("Failed to initialize GLAD\n");
		return EXIT_FAILURE;
	}

	glfwSetFramebufferSizeCallback(window, reshape);
	reshape(window, width, height);

	m_controller.register_callbacks(window);

	m_shader_program = setupShaderProgram();
	glUseProgram(m_shader_program);

	glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 600.0f);
	glUniformMatrix4fv(glGetUniformLocation(m_shader_program, "projection"), 1, GL_FALSE, &projection[0][0]);

	// a directional light vector pointing from the light source
	glm::vec3 light_direction = glm::normalize(glm::vec3(-1.f, -3.f, 0.f));
	float light_ambient = 0.6f;
	glUniform1f(glGetUniformLocation(m_shader_program, "light.ambient"), light_ambient);
	glUniform3fv(glGetUniformLocation(m_shader_program, "light.direction"), 1, &light_direction[0]);
	glEnable(GL_DEPTH_TEST);
	// Swap interval 0 which is default means to swap buffers immediately as glfwSwapBuffers()
	// is called. This allows render loop to potentially run faster than hardware is capable of 
	// displaying causing tearing effect. Setting it to 1 means to wait for 1st vsync after 
	// glfwSwapBuffers() is called to swap buffers which will also limit the frame rate to the 
	// hardware refresh rate at 60 fps or higher for gaming monitors.
	glfwSwapInterval(1);

	return EXIT_SUCCESS;
}

void OpenGLRenderer::teardown()
{
	glDeleteProgram(m_shader_program);
	m_shader_program = NULL;

	glfwTerminate();
}
void OpenGLRenderer::setup_camera(bool follow, const glm::vec3& eye, const glm::vec3& target)
{
	m_camera.setup(follow, eye, target);
}

bool OpenGLRenderer::render(float elapsed_time)
{	// OpenGL rendering
	if (m_controller.is_key_pressed(GLFW_KEY_ESCAPE)) {
		glfwPollEvents();
		return false;
	}
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// update camera and render the scene
	m_camera.process_player_input(m_controller);
	m_camera.update(m_player_trans);
	glm::mat4 view = m_camera.get_view_matrix();
	glUniformMatrix4fv(glGetUniformLocation(m_shader_program, "view"), 1, GL_FALSE, &view[0][0]);
	
	for (auto& s : m_shapes) {
		if (s.second != NULL) {
			s.second->draw(m_shader_program, m_trans[s.first]);
		}
	}
	debug_log_mute("elapsed time (sec): %f fps: %f\n", elapsed_time, 1.f / elapsed_time);
	glfwSwapBuffers((GLFWwindow*)m_window);
	glfwPollEvents();
	return true;
}

unsigned int OpenGLRenderer::create_texture(size_t width, size_t height, unsigned char* data)
{
	unsigned int texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	// set the texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	// set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	// each row has to be 4-byte aligned
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, (int)width, (int)height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	debug_log_mute("Texture #%d created\n", texture);
	return texture;
}
