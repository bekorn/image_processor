#include "common.hpp"
#include "process.hpp"


///--- Interop
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

bool exec(const char * cmd, std::string & out, i32 & exit_code) {
	// https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/popen-wpopen?view=msvc-170#example
    FILE * pipe = _popen(cmd, "r");
    if (not pipe) return false;

	char buffer[64];
	while (fgets(buffer, sizeof(buffer), pipe))
		out.append(buffer);

	out.erase(out.end() - 1); // trim the new line at the end

    int file_eof = feof(pipe);
    exit_code = _pclose(pipe);

    if (not file_eof)
	{
    	printf("[Error] Failed to read the pipe to the end.\n");
		return false;
	}

    return true;
}

struct Timer
{
	i64 begin;
	const char * tag;
	Timer(const char * tag) : begin(GetTickCount64()), tag(tag) {}
	~Timer() { printf("[Timer] %6lld ms | %s\n", GetTickCount64() - begin, tag); }
};
#define TimeScope(tag) Timer timer(tag)

u64 get_file_last_write(const char * path)
{
	WIN32_FILE_ATTRIBUTE_DATA attrb;
	if (not GetFileAttributesExA(path, GetFileExInfoStandard, &attrb))
	{
		print_err("[Error] Can't get file info of \"%s\".\n", path);
		return 0;
	}

	return ULARGE_INTEGER{attrb.ftLastWriteTime.dwLowDateTime, attrb.ftLastWriteTime.dwHighDateTime}.QuadPart;
}

const char * const dll_rel_path = "build_dll\\process_wrapper.dll";

void build_process(const char * cpp_abs_path, bool check_write_times)
{
	bool should_build = true;

	if (check_write_times)
	{
		u64 last_compile_time = get_file_last_write(dll_rel_path);
		u64 last_change_time = get_file_last_write(cpp_abs_path);
		should_build = last_change_time > last_compile_time;
	}

	if (should_build)
	{
		TimeScope("Build DLL");

		char command[1024];
		sprintf_s(command, "build_dll %s", cpp_abs_path);

		std::string out;
		i32 exit_code;
		if (not exec(command, out, exit_code) or exit_code != 0)
		{
			print_err("[Error] Failed to build DLL. ");
			print_err("Exit code: %i, Output:\n---\n%s\n---\n", exit_code, out.c_str());
			return;
		}
	}
}

void apply_process(Image const & original_img, Image & processed_img)
{
	TimeScope("Apply process");

	HMODULE dll = LoadLibraryA(dll_rel_path);
	if (not dll) exit_err("Can't load library from '%s'", dll_rel_path);

	auto init = (f_init *)GetProcAddress(dll, EXPORTED_INIT_NAME_STR);
	if (not init) exit_err("Can't find " EXPORTED_INIT_NAME_STR " in dll");

	auto process = (f_process *)GetProcAddress(dll, EXPORTED_PROCESS_NAME_STR);
	if (not process) exit_err("Can't find " EXPORTED_PROCESS_NAME_STR " in dll");

	init(original_img);

	original_img.blit_into(processed_img);
	{
		TimeScope("Run process");
		process(processed_img);
	}

	FreeLibrary(dll);
}


///--- Graphics
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/image.h>

Image load_image(const char * rel_path, bool flip_vertically = true)
{
	FILE * image_file = fopen(rel_path, "rb");
	if (not image_file) exit_err("Can't open image file");

	stbi_set_flip_vertically_on_load(flip_vertically);

	int x, y, c;
	stbi_uc * rgba_pixels = stbi_load_from_file(image_file, &x, &y, &c, 4);
	if (not rgba_pixels) exit_err("Can't load image: %s", stbi_failure_reason());
	fclose(image_file);

	return {x, y, (u8x4 *)(rgba_pixels)};
}


#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/gl.h>

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

void update_texture(GLuint tex, Image const & img)
{
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.x, img.y, GL_RGBA, GL_UNSIGNED_BYTE, img.pixels);
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
	bool apply_process = false;
	bool change_target_abs_path = false;
} Actions;

struct {
	std::string target_abs_path = {};
} State;

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS and key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
	if (action == GLFW_PRESS and key == GLFW_KEY_SPACE) Actions.switch_texture = true;
	if (action == GLFW_PRESS and key == GLFW_KEY_ENTER) Actions.apply_process = true;
}

void clear_actions()
{
	Actions = {0};
}

void drop_callback(GLFWwindow* window, int path_count, const char* paths[])
{
	Actions.change_target_abs_path = true;
	State.target_abs_path = paths[0];
	printf("Set target file to \"%s\".\n", State.target_abs_path.c_str());
}


///--- Application
int main(int argc, const char * argv[])
{
	/// Config
	int const TARGET_FPS = 120;
	bool const OPENGL_DEBUG = true;


	/// Init
	if (argc < 2) exit_err("Supply image path as the first argument");
	printf("Image: %s\n", argv[1]);

	Image const original_img = load_image(argv[1]);
	printf("Loaded image, resolution: %dx%d\n", original_img.x, original_img.y);

	Image processed_img(original_img.x, original_img.y, nullptr);

	if (argc > 2)
	{
		State.target_abs_path = argv[2];
		build_process(State.target_abs_path.c_str(), false);
		apply_process(original_img, processed_img);
	}

	glfwSetErrorCallback([](int err, const char * desc){ print_err("GLFW[Error] %i: %s\n", err, desc); });
	if (not glfwInit()) exit_err("GLFW Failed to init");

	glfwWindowHint(GLFW_RESIZABLE, false);
	glfwWindowHint(GLFW_DOUBLEBUFFER, false);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, OPENGL_DEBUG);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	GLFWwindow * window = glfwCreateWindow(original_img.x, original_img.y, "Image Processor", nullptr, nullptr);
	if (not window) exit_err("Window or context creation failed");

	glfwMakeContextCurrent(window);
	{
		int const glad_version = gladLoadGL(glfwGetProcAddress);
		const unsigned char * const gl_version = glGetString(GL_VERSION);
		printf("Glad: %i, OpenGL: %s\n", glad_version, gl_version);

		if (OPENGL_DEBUG) glDebugMessageCallback(gl_debug_callback, nullptr);
	}
	glfwSetKeyCallback(window, key_callback);
	glfwSetDropCallback(window, drop_callback);

	GLuint empty_vao;
	glGenVertexArrays(1, &empty_vao);
	glBindVertexArray(empty_vao);

	init_blit_program();

	GLuint const origianl_tex = create_texture({
		.x = original_img.x, .y = original_img.y, .pixels = original_img.pixels,
	});

	GLuint const processed_tex = create_texture({
		.x = original_img.x, .y = original_img.y, .pixels = processed_img.pixels,
	});


	/// Run
	GLuint active_tex = origianl_tex;
	blit_texture(active_tex);

	while (not glfwWindowShouldClose(window))
	{
		f64 const frame_begin_s = glfwGetTime();

		clear_actions();
		glfwPollEvents();

		if (Actions.switch_texture) 
		{
			active_tex = (active_tex == origianl_tex ? processed_tex : origianl_tex);
			blit_texture(active_tex);
		}

		if (Actions.apply_process)
		{
			if (State.target_abs_path.empty())
				print_err("[Error] Set a target file by dropping a file into the window.\n");
			else
			{
				build_process(State.target_abs_path.c_str(), true);
				apply_process(original_img, processed_img);
				update_texture(processed_tex, processed_img);
				if (processed_tex == active_tex) blit_texture(processed_tex);
			}
		}

		if (Actions.change_target_abs_path)
		{
			build_process(State.target_abs_path.c_str(), false);
			apply_process(original_img, processed_img);
			update_texture(processed_tex, processed_img);
			if (processed_tex == active_tex) blit_texture(processed_tex);
		}

		glFinish();

		f64 frame_s = glfwGetTime() - frame_begin_s;
		Sleep((DWORD)max(0., 1000 * ((1. / TARGET_FPS) - frame_s)));
	}


	/// Clean
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
