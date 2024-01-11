#include "common.hpp"
#include "process.hpp"

#pragma region Interop
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

bool build_process(const char * cpp_abs_path, bool check_write_times)
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
			return false;
		}
	}

	return true;
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

	original_img.blit_into(processed_img);

	{
		printf("// DLL Begin \\\\\n");
		init(original_img);

		TimeScope("Run process");
		process(processed_img);
		printf("\\\\  DLL End  //\n");
	}

	FreeLibrary(dll);
}

std::wstring utf8_to_utf16(std::string const & str)
{
	std::wstring wstr;
	wstr.resize(MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0));
	MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), wstr.data(), (int)wstr.size());
	return wstr;
}

class FileWatcher
{
	static constexpr size_t buffer_size = 4 << 10;
	unique_array<std::byte> buffer{new std::byte[buffer_size]};
	std::wstring file_name;
	HANDLE file_handle{INVALID_HANDLE_VALUE};
	OVERLAPPED overlapped{.hEvent = INVALID_HANDLE_VALUE};

public:
	~FileWatcher()
	{
		if (file_handle != INVALID_HANDLE_VALUE) CancelIo(file_handle), CloseHandle(file_handle);
		if (overlapped.hEvent != INVALID_HANDLE_VALUE) CloseHandle(overlapped.hEvent);
	}

	void watch(std::string const & file_abs_path)
	{
		std::wstring wfile_abs_path = utf8_to_utf16(file_abs_path);
		size_t dir_end_idx = wfile_abs_path.find_last_of(L"\\/");
		if (dir_end_idx == std::string::npos) exit_err("[Error] Invalid path");

		std::wstring dir = wfile_abs_path.substr(0, dir_end_idx);
		file_name = wfile_abs_path.substr(dir_end_idx + 1);

		if (file_handle != INVALID_HANDLE_VALUE) CancelIo(file_handle), CloseHandle(file_handle);
		file_handle = CreateFileW(
			dir.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			0,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			0
		);
		if (file_handle == INVALID_HANDLE_VALUE) exit_err("[Error] CreateFileA failed");

		if (overlapped.hEvent != INVALID_HANDLE_VALUE) CloseHandle(overlapped.hEvent);
		overlapped.hEvent = CreateEventA(0, false, false, nullptr);
		if (overlapped.hEvent == INVALID_HANDLE_VALUE) exit_err("[Error] CreateEventA failed");

		if (not ReadDirectoryChangesW(
			file_handle,
			buffer, buffer_size,
			false,
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			nullptr,
			&overlapped,
			nullptr
		)) exit_err("[Error] ReadDirectoryChangesW failed");
	}

	bool is_notified()
	{
		if (file_handle == INVALID_HANDLE_VALUE) return false;

		bool is_file_changed = false;

		// there may be many notifications, process all
		while (true)
		{
			DWORD bytes_written;
			if (not GetOverlappedResult(file_handle, &overlapped, &bytes_written, false))
			{
				if (GetLastError() != ERROR_IO_INCOMPLETE) exit_err("[Error] GetOverlappedResult failed\n");
				break;
			}

			std::byte * ptr = buffer.things;
			FILE_NOTIFY_INFORMATION * notification = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(ptr);
			while (notification->Action != 0)
			{
				std::wstring_view name(notification->FileName, notification->FileNameLength / sizeof(wchar_t));
				if (name == file_name) is_file_changed = true;

				// check for any other notifications
				if (notification->NextEntryOffset == 0) break;

				ptr += notification->NextEntryOffset;
				notification = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(ptr);
			}

			if (not ReadDirectoryChangesW(
				file_handle,
				buffer, buffer_size,
				false,
				FILE_NOTIFY_CHANGE_LAST_WRITE,
				nullptr,
				&overlapped,
				nullptr
			)) exit_err("[Error] ReadDirectoryChangesW failed");
		}

		return is_file_changed;
	}
};

#pragma endregion

#pragma region Graphics
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

#pragma endregion

#pragma region Config, State, Actions

struct {
	bool gl_debug = true;
	int target_fps = 120;
	int tex_count = 1/*Original*/ + 3/*Processed*/;
} constexpr Config;

struct {
	int active_tex_idx = 0;
	std::string target_abs_path = {};
} State;

struct {
	bool switch_texture = false;
	bool apply_process = false;
	bool change_target_abs_path = false;
} Actions;

void clear_actions()
{
	Actions = {0};
}

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS and key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
	if (action == GLFW_PRESS and key == GLFW_KEY_SPACE) Actions.apply_process = true;
	if (action == GLFW_PRESS and key >= GLFW_KEY_1 and key < GLFW_KEY_1 + Config.tex_count)
		Actions.switch_texture = true, State.active_tex_idx = key - GLFW_KEY_1;
}

void drop_callback(GLFWwindow* window, int path_count, const char* paths[])
{
	Actions.change_target_abs_path = true;
	State.target_abs_path = paths[0];
	printf("Set target file to \"%s\".\n", State.target_abs_path.c_str());
}

void update_window_title(GLFWwindow * window)
{
	char title[128];
	sprintf_s(
		title, "Image Processor > Image %i (%s)",
		State.active_tex_idx + 1, State.active_tex_idx == 0 ? "Original" : "Processed"
	);
	glfwSetWindowTitle(window, title);
}

#pragma endregion

int main(int argc, const char * argv[])
{
	/// Init
	if (argc < 2) exit_err("Supply image path as the first argument");
	printf("Image: %s\n", argv[1]);

	Image const original_img = load_image(argv[1]);
	printf("Loaded image, resolution: %dx%d\n", original_img.x, original_img.y);

	Image processed_img(original_img.x, original_img.y, nullptr);

	glfwSetErrorCallback([](int err, const char * desc){ print_err("GLFW[Error] %i: %s\n", err, desc); });
	if (not glfwInit()) exit_err("GLFW Failed to init");

	glfwWindowHint(GLFW_RESIZABLE, false);
	glfwWindowHint(GLFW_DOUBLEBUFFER, false);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, Config.gl_debug);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	GLFWwindow * window = glfwCreateWindow(original_img.x, original_img.y, "Image Processor", nullptr, nullptr);
	if (not window) exit_err("Window or context creation failed");
	update_window_title(window);

	glfwMakeContextCurrent(window);
	{
		int glad_version = gladLoadGL(glfwGetProcAddress);
		const unsigned char * gl_version = glGetString(GL_VERSION);
		printf("Glad: %i, OpenGL: %s\n", glad_version, gl_version);

		if (Config.gl_debug) glDebugMessageCallback(gl_debug_callback, nullptr);
	}
	glfwSetKeyCallback(window, key_callback);
	glfwSetDropCallback(window, drop_callback);

	GLuint empty_vao;
	glGenVertexArrays(1, &empty_vao);
	glBindVertexArray(empty_vao);

	init_blit_program();

	GLuint texs[Config.tex_count];
	{
		TextureDesc desc{
			.x = original_img.x, .y = original_img.y,
			.pixels = nullptr
		};
		for (int i = 0; i < Config.tex_count; ++i)
			texs[i] = create_texture(desc);
		
		update_texture(texs[0], original_img);
	}

	if (argc > 2) // an initial process is provided
	{
		State.target_abs_path = argv[2];
		Actions.change_target_abs_path = true;

		State.active_tex_idx = 1;
		Actions.switch_texture = true;
	}

	FileWatcher file_watcher;


	/// Run
	Actions.switch_texture = true;

	while (not glfwWindowShouldClose(window))
	{
		f64 const frame_begin_s = glfwGetTime();

		glfwPollEvents();
		if (file_watcher.is_notified()) Actions.apply_process = true;

		if (Actions.switch_texture) 
		{
			blit_texture(texs[State.active_tex_idx]);
			update_window_title(window);
		}

		if (Actions.apply_process)
		{
			if (State.target_abs_path.empty())
				print_err("[Error] Set a target file by dropping a file into the window.\n");
			else if (State.active_tex_idx == 0)
				print_err("[Error] Select a Processed image to store the result.\n");
			else
			{
				if (build_process(State.target_abs_path.c_str(), true))
				{
					GLuint const & processed_tex = texs[State.active_tex_idx];
					apply_process(original_img, processed_img);
					update_texture(processed_tex, processed_img);
					blit_texture(processed_tex);
				}
			}
		}

		if (Actions.change_target_abs_path)
		{
			if (State.active_tex_idx == 0)
				print_err("[Error] Select a Processed image to store the result.\n");
			else
			{
				if (build_process(State.target_abs_path.c_str(), false))
				{
					GLuint const & processed_tex = texs[State.active_tex_idx];
					apply_process(original_img, processed_img);
					update_texture(processed_tex, processed_img);
					blit_texture(processed_tex);
				}

				file_watcher.watch(State.target_abs_path);
			}
		}

		glFinish();
		clear_actions();

		f64 frame_s = glfwGetTime() - frame_begin_s;
		Sleep((DWORD)max(0., 1000 * ((1. / Config.target_fps) - frame_s)));
	}


	/// Clean
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}