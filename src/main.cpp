///--- Common
#include <ciso646>
#include <cstdio>
#include <cstdlib>
#include <string>

using i32 = int32_t;
using u8 = uint8_t;
using u8x4 = uint8_t[4];

template<typename T> T max(T a, T b) { return a > b ? a : b; }
template<typename T> T min(T a, T b) { return a < b ? a : b; }

template<typename T>
struct unique_one
{
	T * thing;
	unique_one() : thing(nullptr) {}
	unique_one(T * && raw) : thing(raw) { raw = nullptr; }
	~unique_one() { delete thing; }

	operator bool() const { return thing != nullptr; }
	T & operator ->() const { return *thing; }
};

template<typename T>
struct unique_array
{
	T * things;
	unique_array() : things(nullptr) {}
	unique_array(T * && raw) : things(raw) { raw = nullptr; }
	~unique_array() { delete[] things; }

	operator bool() const { return things != nullptr; }
	T & operator [](int idx) const { return things[idx]; }
	operator T *() const { return things; }
};

template<typename T>
struct span {
	T * begin = nullptr;
	int size = 0;

	bool empty() const { return size == 0; }
};


///--- Interoperations
#include <stdarg.h>

// see https://c-faq.com/varargs/handoff.html for the _err functions
void exit_err(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

	exit(EXIT_FAILURE);
}

void print_err(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

bool exec(const char * cmd, std::string & out) {
    FILE * pipe = _popen(cmd, "r");
    if (not pipe) return false;

	char buffer[64];
	while (fgets(buffer, sizeof(buffer), pipe))
		out += buffer;

	out.erase(out.end() - 1); // trim the new line at the end

    _pclose(pipe);
    return true;
}


///--- Graphics
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/image.h>

struct Image
{
	unique_array<u8x4> pixels;
	i32 x, y;
};

Image load_image(const char * rel_path, bool flip_vertically = true)
{
	std::string image_path;
	image_path.reserve(1024);
	if (not exec("cd", image_path)) exit_err("Error on exec cd");

	image_path.push_back('\\'), image_path.append(rel_path);
	printf("ImagePath: %s\n", image_path.c_str());

	FILE * image_file = fopen(image_path.c_str(), "rb");
	if (not image_file) exit_err("Can't open image file");

	stbi_set_flip_vertically_on_load(flip_vertically);

	int x, y, c;
	stbi_uc * rgba_pixels = stbi_load_from_file(image_file, &x, &y, &c, 4);
	if (not rgba_pixels) exit_err("Can't load image: %s", stbi_failure_reason());
	fclose(image_file);

	return {.pixels = (u8x4 *)rgba_pixels, .x = x, .y = y};
}

struct TextureDesc
{
	int x, y;
	u8x4 * pixels = nullptr;

	GLenum min_filter = GL_LINEAR;
	GLenum mag_filter = GL_LINEAR;

	GLenum wrap_s = GL_CLAMP_TO_EDGE;
	GLenum wrap_t = GL_CLAMP_TO_EDGE;
};
GLuint create_texture(TextureDesc const & desc)
{
	GLuint id;
	glGenTextures(1, &id);

	glBindTexture(GL_TEXTURE_2D, id);
	glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA8, desc.x, desc.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, desc.pixels);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenerateMipmap(GL_TEXTURE_2D);

	return id;
}

void gl_debug_callback(
	GLenum source, GLenum type, GLuint id, GLenum severity,
	GLsizei length, const GLchar * message, const void * userParam
)
{
	const char * source_str;
	switch (source)
	{
	case GL_DEBUG_SOURCE_API:				source_str = "API";break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:		source_str = "Window System";break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:	source_str = "Shader Compiler";break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:		source_str = "Third Party";break;
	case GL_DEBUG_SOURCE_APPLICATION:		source_str = "Application";break;
	case GL_DEBUG_SOURCE_OTHER:				source_str = "Other";break;
	default: break;
	}
	const char * type_str;
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:				type_str = "Error";break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:	type_str = "Deprecated Behaviour";break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:	type_str = "Undefined Behaviour";break;
	case GL_DEBUG_TYPE_PORTABILITY:			type_str = "Portability";break;
	case GL_DEBUG_TYPE_PERFORMANCE:			type_str = "Performance";break;
	case GL_DEBUG_TYPE_MARKER:				type_str = "Marker";break;
	case GL_DEBUG_TYPE_PUSH_GROUP:			type_str = "Push Group";break;
	case GL_DEBUG_TYPE_POP_GROUP:			type_str = "Pop Group";break;
	case GL_DEBUG_TYPE_OTHER:				type_str = "Other";break;
	default: break;
	}
	const char * severity_str;
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH: 			severity_str = " !!!"; break;
	case GL_DEBUG_SEVERITY_MEDIUM: 			severity_str = " !!"; break;
	case GL_DEBUG_SEVERITY_LOW: 			severity_str = " !"; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:	severity_str = ""; break;
	default: break;
	}

	print_err("GL[%s %s%s]", source_str, type_str, severity_str);
	print_err(" %i: %s%c", id, message, (message[length - 2] != '\n' ? '\n' : '\0'));
}


GLuint blit_program;

void init_blit_program()
{
	blit_program = glCreateProgram();
	
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	const char * vert_src = R"VERT(#version 330
out vec2 uv;

void main()
{
//  2
//  |`.
//  |  `.
//  |----`.
//  |    | `.
//  '----'----
//  0         1
//
//  gl_VertexID = 0  =>  gl_Position = (-1, -1, 1, 1) | uv = (0, 0)
//  gl_VertexID = 1  =>  gl_Position = (+3, -1, 1, 1) | uv = (2, 0)
//  gl_VertexID = 2  =>  gl_Position = (-1, +3, 1, 1) | uv = (0, 2)

    gl_Position = vec4(
        gl_VertexID == 1 ? 3 : -1,
        gl_VertexID == 2 ? 3 : -1,
        1,
        1
    );

    uv = vec2(
        gl_VertexID == 1 ? 2 : 0,
        gl_VertexID == 2 ? 2 : 0
    );
}
	)VERT";
	glShaderSource(vert, 1, &vert_src, 0);
	glCompileShader(vert);
	
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	const char * frag_src = R"FRAG(#version 330
in vec2 uv;

uniform sampler2D tex;

out vec4 color;

void main()
{
	color = texture(tex, uv);
}
	)FRAG";
	glShaderSource(frag, 1, &frag_src, 0);
	glCompileShader(frag);

	glAttachShader(blit_program, vert);
	glAttachShader(blit_program, frag);
	glLinkProgram(blit_program);

	glUseProgram(blit_program);
	glUniform1i(glGetUniformLocation(blit_program, "tex"), 0);
}

void blit_texture(GLuint tex)
{
	glUseProgram(blit_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}


///--- State
struct {
	bool switch_texture = false;
	bool recompile_dll = false;
} Actions;

void register_actions(GLFWwindow * window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS and key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
	if (action == GLFW_PRESS and key == GLFW_KEY_SPACE) Actions.switch_texture = true;
	if (action == GLFW_PRESS and key == GLFW_KEY_ENTER) Actions.recompile_dll = true;
}

void clear_actions()
{
	Actions = {0};
}


///--- Application
int main(int argc, const char * argv[])
{
	/// Config
	bool const DOUBLE_BUFFERING = false;
	int const SWAP_INTERVAL = 30;
	bool const OPENGL_DEBUG = true;


	/// Init
	if (argc < 2) exit_err("Supply image path as the first argument");
	printf("Image: %s\n", argv[1]);

	Image const origianl_img = load_image(argv[1]);
	printf("Loaded image, resolution: %dx%d\n", origianl_img.x, origianl_img.y);

	glfwSetErrorCallback([](int err, const char * desc){ print_err("GLFW[Error] %i: %s\n", err, desc); });
	if (not glfwInit()) exit_err("GLFW Failed to init");

	glfwWindowHint(GLFW_RESIZABLE, false);
	glfwWindowHint(GLFW_DOUBLEBUFFER, DOUBLE_BUFFERING);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, OPENGL_DEBUG);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	GLFWwindow * window = glfwCreateWindow(origianl_img.x, origianl_img.y, "Image Processor", nullptr, nullptr);
	if (not window) exit_err("Window or context creation failed");

	glfwMakeContextCurrent(window);
	glfwSwapInterval(SWAP_INTERVAL);
	{
		int const glad_version = gladLoadGL(glfwGetProcAddress);
		const unsigned char * const gl_version = glGetString(GL_VERSION);
		printf("Glad: %i, OpenGL: %s\n", glad_version, gl_version);

		if (OPENGL_DEBUG) glDebugMessageCallback(gl_debug_callback, nullptr);
	}
	glfwSetKeyCallback(window, register_actions);

	GLuint empty_vao;
	glGenVertexArrays(1, &empty_vao);
	glBindVertexArray(empty_vao);

	init_blit_program();

	GLuint const origianl_tex = create_texture({
		.x = origianl_img.x, .y = origianl_img.y, .pixels = origianl_img.pixels,
	});

	GLuint const processed_tex = create_texture({
		.x = origianl_img.x, .y = origianl_img.y, .pixels = nullptr,
	});


	/// Run
	GLuint active_tex = origianl_tex;
	blit_texture(active_tex);

	while (not glfwWindowShouldClose(window))
	{
		clear_actions();
		glfwPollEvents();

		if (Actions.switch_texture) 
		{
			active_tex = (active_tex == origianl_tex ? processed_tex : origianl_tex);
			blit_texture(active_tex);
		}
		if (Actions.recompile_dll) printf("Recompiling DLL\n");


		if (DOUBLE_BUFFERING) glfwSwapBuffers(window); else glFinish();
	}


	/// Clean
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
