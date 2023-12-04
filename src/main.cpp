#include <ciso646>
#include <cstdio>
#include <cstdlib>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

struct {
	bool switch_texture = false;
	bool recompile_dll = false;
} Actions;

void register_actions(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS and key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
	if (action == GLFW_PRESS and key == GLFW_KEY_SPACE) Actions.switch_texture = true;
	if (action == GLFW_PRESS and key == GLFW_KEY_ENTER) Actions.recompile_dll = true;
}

void clear_actions()
{
	Actions = {0};
}

int main()
{
	/// init
	glfwSetErrorCallback([](int err, const char *desc){ fprintf(stderr, "GLFW Error[%i]: %s\n", err, desc); });
	if (not glfwInit()) printf("GLFW Failed to init"), exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	GLFWwindow *window = glfwCreateWindow(640, 480, "Image Processor", nullptr, nullptr);
	if (not window) fprintf(stderr, "Window or context creation failed"), exit(EXIT_FAILURE);

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	{
		int const glad_version = gladLoadGL(glfwGetProcAddress);
		const unsigned char* const gl_version = glGetString(GL_VERSION);
		printf("Glad: %i, OpenGL: %s\n", glad_version, gl_version);
	}
	glfwSetKeyCallback(window, register_actions);


	/// run
	while (not glfwWindowShouldClose(window))
	{
		clear_actions();
		glfwPollEvents();

		if (Actions.switch_texture) printf("Switching texture\n");
		if (Actions.recompile_dll) printf("Recompiling DLL\n");



		glfwSwapBuffers(window);
	}


	/// clean
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
