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
#include <boost/json/src.hpp>
#include "OpenGLRenderer.h"
#include "Shaders.h"
#include "../Utils.h"

namespace json = boost::json;

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
// OpenGLShape
//~~~
OpenGLShape* create_from_json(json::object* obj, glm::mat4& trans)
{
	if (obj == nullptr) return nullptr;

	json::value* v = obj->if_contains("trans");
	if (v && v->is_array()) {
		float* p = &trans[0][0];
		for (auto& i : v->get_array()) {
			*p++ = value_to<float>(i);
		}
	}

	v = obj->if_contains("child");
	if (v && v->is_array()) {
		OpenGLShape* compound = new OpenGLShape();
		for (auto& c : v->get_array()) {
			glm::mat4 m;
			OpenGLShape* s = create_from_json(c.if_object(), m);
			compound->add_child_shape(s, m);
		}
		return compound;
	}
	
	std::vector<uv_vertex> mesh;
	std::vector<int> face_index;
	if (obj->contains("mesh")) {
		std::vector<float> m = std::move(value_to<std::vector<float>>(obj->at("mesh")));
		uv_vertex* p = (uv_vertex*)m.data();
		size_t n = m.size() / (sizeof(uv_vertex)/sizeof(float));
		for (int i = 0; i < n; i++) {		
			mesh.push_back(p[i]);
		}
	}

	if (obj->contains("face_index")) {
		face_index = std::move(value_to<std::vector<int>>(obj->at("face_index")));		
	}

	OpenGLShape* shape = new OpenGLShape(mesh, std::move(face_index));
	if (obj->contains("default_texture")) {
		shape->set_default_texture(value_to<unsigned int>(obj->at("default_texture")));
	}

	if (obj->contains("textures")) {
		std::vector<unsigned int> txtr = std::move(value_to<std::vector<unsigned int>>(obj->at("textures")));
		for (auto t : txtr) {
			shape->add_texture(t);
		}
	}
	return shape;	
}

OpenGLShape* OpenGLShape::from_json(const char* json, glm::mat4& trans)
{
	json::value val = json::parse(json);
	return create_from_json(val.if_object(), trans);	
}

OpenGLShape::OpenGLShape(const std::vector<uv_vertex>& mesh, std::vector<int> face_index) : 
	m_num_vertices((int)mesh.size()), m_default_texture(0)
{
	m_face_index = std::move(face_index);
	std::vector<glm::vec3> normal;
	const uv_vertex* p = mesh.data();
	for (int n = 0; n < m_num_vertices; n += 3) {
		glm::vec3 p1(p[n].x, p[n].y, p[n].z);
		glm::vec3 p2(p[n + 1].x, p[n + 1].y, p[n + 1].z);
		glm::vec3 p3(p[n + 2].x, p[n + 2].y, p[n + 2].z);
		// one normal vector for each vertex!!!
		glm::vec3 norm = glm::normalize(glm::cross(p2 - p1, p3 - p1));
		normal.push_back(norm);
		normal.push_back(norm);
		normal.push_back(norm);
	}
	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);

	glBindVertexArray(m_VAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	
	size_t offset = sizeof(uv_vertex) * m_num_vertices;
	size_t buffer_size = offset + normal.size() * sizeof(glm::vec3);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, offset, mesh.data());
	glBufferSubData(GL_ARRAY_BUFFER, offset, normal.size() * sizeof(glm::vec3), normal.data());

	// position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(uv_vertex), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(uv_vertex), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)offset);
	glEnableVertexAttribArray(2);
}

void OpenGLShape::add_child_shape(OpenGLShape* shape, glm::mat4 trans)
{
	child_shape child = { shape, trans };
	m_child_shapes.push_back(child);
}

void OpenGLShape::draw(unsigned int shader_program, const glm::mat4& trans, 
	const std::map<int, int>& texture_id_map) const
{
	for (const child_shape& child: m_child_shapes) {
		// combine the child transform
		child.shape->draw(shader_program, trans * child.trans, texture_id_map);
	}
	if (m_num_vertices == 0) return;

	glm::mat4 model = trans;
	glUniformMatrix4fv(glGetUniformLocation(shader_program, "model"), 1, GL_FALSE, &model[0][0]);
	
	// if no texture is set, draw wireframe
	for (int i = 0; i < m_face_index.size(); i++)
	{
		int index = m_face_index[i];
		int n = (i == m_face_index.size() - 1) ? m_num_vertices - index : m_face_index[i + 1] - index;
		int t = (i < m_textures.size()) ? m_textures[i] : m_default_texture;
		
		if (t != 0) t = texture_id_map.at(t);
		glPolygonMode(GL_FRONT_AND_BACK, t == 0 ? GL_LINE : GL_FILL);
		glActiveTexture(GL_TEXTURE0 + t);
		glBindTexture(GL_TEXTURE_2D, t);
		glUniform1i(glGetUniformLocation(shader_program, "txtr"), t);

		glBindVertexArray(m_VAO);
		glDrawArrays(GL_TRIANGLES, index, n);
	}
}

OpenGLShape::~OpenGLShape()
{
	glDeleteVertexArrays(1, &m_VAO);
	glDeleteBuffers(1, &m_VBO);
}

//~~~
// OpenGLRenderer
//~~~
void OpenGLRenderer::add_shape(int id, const char* json)
{
	glm::mat4 trans;
	OpenGLShape* shape = OpenGLShape::from_json(json, trans);
	if (shape != nullptr) {
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
			s.second->draw(m_shader_program, m_trans[s.first], m_texture_id_map);
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
