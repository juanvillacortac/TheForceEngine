#include <TFE_Ui/ui.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_Settings/linux/tfe_gl_backend.h>
#include <TFE_RenderBackend/Win32OpenGL/tfe_gles_ext.h>
#include <TFE_System/system.h>

#include "imGUI/imgui.h"
#include "imGUI/imgui_impl_sdl2.h"
#include "imGUI/imgui_impl_opengl3.h"
#include "portable-file-dialogs.h"
#include "markdown.h"
#include <TFE_Input/input.h>
#include <SDL.h>
#include <cmath>
#include <cstdlib>

using namespace TFE_Input;

namespace TFE_Ui
{
const char* glsl_version = nullptr;
static s32 s_uiScale = 100;
static bool s_guiFrameActive;

bool isHandheld()
{
	const char* env = std::getenv("TFE_HANDHELD");
	return env && env[0] == '1';
}

static void feedHandheldGamepad()
{
	if (!isHandheld())
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
	{
		return;
	}

	io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

	auto key = [&io](ImGuiKey imguiKey, Button button)
	{
		io.AddKeyEvent(imguiKey, buttonDown(button));
	};

	key(ImGuiKey_GamepadStart, CONTROLLER_BUTTON_START);
	key(ImGuiKey_GamepadBack, CONTROLLER_BUTTON_BACK);
	key(ImGuiKey_GamepadFaceDown, CONTROLLER_BUTTON_A);
	key(ImGuiKey_GamepadFaceRight, CONTROLLER_BUTTON_B);
	key(ImGuiKey_GamepadFaceLeft, CONTROLLER_BUTTON_X);
	key(ImGuiKey_GamepadFaceUp, CONTROLLER_BUTTON_Y);
	key(ImGuiKey_GamepadDpadLeft, CONTROLLER_BUTTON_DPAD_LEFT);
	key(ImGuiKey_GamepadDpadRight, CONTROLLER_BUTTON_DPAD_RIGHT);
	key(ImGuiKey_GamepadDpadUp, CONTROLLER_BUTTON_DPAD_UP);
	key(ImGuiKey_GamepadDpadDown, CONTROLLER_BUTTON_DPAD_DOWN);
	key(ImGuiKey_GamepadL1, CONTROLLER_BUTTON_LEFTSHOULDER);
	key(ImGuiKey_GamepadR1, CONTROLLER_BUTTON_RIGHTSHOULDER);
	key(ImGuiKey_GamepadL3, CONTROLLER_BUTTON_LEFTSTICK);
	key(ImGuiKey_GamepadR3, CONTROLLER_BUTTON_RIGHTSTICK);

	const f32 deadzone = 0.2f;
	const f32 lx = getAxis(AXIS_LEFT_X);
	const f32 ly = getAxis(AXIS_LEFT_Y);
	const f32 rx = getAxis(AXIS_RIGHT_X);
	const f32 ry = getAxis(AXIS_RIGHT_Y);

	auto stick = [&io, deadzone](ImGuiKey negKey, ImGuiKey posKey, f32 value)
	{
		io.AddKeyAnalogEvent(negKey, value < -deadzone, value < -deadzone ? -value : 0.0f);
		io.AddKeyAnalogEvent(posKey, value > deadzone, value > deadzone ? value : 0.0f);
	};

	stick(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, lx);
	stick(ImGuiKey_GamepadLStickUp, ImGuiKey_GamepadLStickDown, ly);
	stick(ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, rx);
	stick(ImGuiKey_GamepadRStickUp, ImGuiKey_GamepadRStickDown, ry);

	const f32 lt = getAxis(AXIS_LEFT_TRIGGER);
	const f32 rt = getAxis(AXIS_RIGHT_TRIGGER);
	io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, lt > 0.1f, lt);
	io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, rt > 0.1f, rt);
}

void focusWindowByName(const char* name)
{
	if (ImGuiWindow* window = ImGui::FindWindowByName(name))
	{
		ImGui::FocusWindow(window);
	}
}

void handleHandheldConfigWindowFocus()
{
	if (!isHandheld())
	{
		return;
	}

	if (buttonPressed(CONTROLLER_BUTTON_LEFTSHOULDER))
	{
		focusWindowByName("##Sidebar");
	}
	else if (buttonPressed(CONTROLLER_BUTTON_RIGHTSHOULDER))
	{
		focusWindowByName("##Settings");
	}
}

bool init(void* window, void* context, s32 uiScale)
{
#if defined(TFE_RUNTIME_GL)
	// ImGui ships GLES sources for #version 300 es only (320 es needs precision in its desktop fallbacks).
	if (isHandheld() || tfe_UseGLES())
		glsl_version = "#version 300 es\n";
	else
		glsl_version = (strcmp(SDL_GetPlatform(), "Mac OS X") == 0 ? "#version 410\n" : "#version 130\n");
#else
	glsl_version = strcmp(SDL_GetPlatform(), "Mac OS X") == 0 ? "#version 410" : "#version 130";
#endif
	
	s_uiScale = uiScale;
	const bool handheld = isHandheld();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	if (handheld)
	{
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_NavHighlight] = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
	}
	else
	{
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	}

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	TFE_System::logWrite(LOG_MSG, "UI", "ImGui SDL2 init...");
	ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)window, context);
	TFE_System::logWrite(LOG_MSG, "UI", "ImGui OpenGL3 init (%s)...", glsl_version ? glsl_version : "default");
	if (!ImGui_ImplOpenGL3_Init(glsl_version))
	{
		TFE_System::logWrite(LOG_ERROR, "UI", "ImGui_ImplOpenGL3_Init failed");
		return false;
	}

	if (handheld)
	{
		// Gamepad input is fed from TFE_Input each frame (see feedHandheldGamepad).
		ImGui_ImplSDL2_SetGamepadMode(ImGui_ImplSDL2_GamepadMode_Manual, nullptr, 0);
	}

	// Set the default font (13 px)
	// TODO: Allow scaled UI, so loading a different font for larger scales.
	if (handheld || s_uiScale <= 100)
	{
		io.Fonts->AddFontDefault();
	}
	else
	{
		char fp[TFE_MAX_PATH];
		sprintf(fp, "Fonts/DroidSansMono.ttf");
		TFE_Paths::mapSystemPath(fp);
		io.Fonts->AddFontFromFileTTF(fp, f32(13 * s_uiScale / 100));
	}
	
	TFE_System::logWrite(LOG_MSG, "UI", "Markdown init...");
	TFE_Markdown::init(f32(16 * s_uiScale / 100));

	if (handheld)
		return true;

	// Desktop file dialogs (zenity/kdialog); skip on PortMaster handheld.
	if (!pfd::settings::available())
	{
		TFE_System::logWrite(LOG_WARNING, "UI", "No file-dialog helper found (zenity/kdialog)");
		return false;
	}
	
	return true;
}

void shutdown()
{
	TFE_Markdown::shutdown();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void setUiScale(s32 scale)
{
	s_uiScale = scale;
}

s32 getUiScale()
{
	return s_uiScale;
}

void setUiInput(const void* inputEvent)
{
	const SDL_Event* sdlEvent = (SDL_Event*)inputEvent;
	ImGui_ImplSDL2_ProcessEvent(sdlEvent);
}

void begin()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	feedHandheldGamepad();
	ImGui::NewFrame();
	s_guiFrameActive = true; // No way to check if we're inside a frame using ImGui's APIs?
}

void render()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	s_guiFrameActive = false;
}

bool isGuiFrameActive() { return s_guiFrameActive; }

void invalidateFontAtlas()
{
	ImGui_ImplOpenGL3_DestroyFontsTexture();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// General TFE tries to keep paths consistently using forward slashes for readability, consistency and
// generally they work equally well on Linux, Mac and Windows.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// *However* some specific APIs (usually of the Win32 variety) require backslashes.
// So FileUtil::convertToOSPath() must be used with initial paths to convert to the correct slash type.
// This can be a bit of a waste but I'd rather have consistent paths through most of the application
// and restrict the ugliness to as small an area as possible.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
FileResult openFileDialog(const char* title, const char* initPath, std::vector<std::string> const &filters/* = { "All Files", "*" }*/, bool multiSelect/* = false*/)
{
	char initPathOS[TFE_MAX_PATH] = "";
	if (initPath)
	{
		FileUtil::convertToOSPath(initPath, initPathOS);
	}

	return pfd::open_file(title, initPathOS, filters, multiSelect ? pfd::opt::multiselect : pfd::opt::none).result();
}

FileResult directorySelectDialog(const char* title, const char* initPath, bool forceInitPath/* = false*/)
{
	char initPathOS[TFE_MAX_PATH] = "";
	if (initPath)
	{
		FileUtil::convertToOSPath(initPath, initPathOS);
	}

	FileResult result;
	std::string res = pfd::select_folder(title, initPathOS).result();
	result.push_back(res);

	return result;
}

FileResult saveFileDialog(const char* title, const char* initPath, std::vector<std::string> filters/* = { "All Files", "*" }*/, bool forceOverwrite/* = false*/)
{
	char initPathOS[TFE_MAX_PATH] = "";
	if (initPath)
	{
		FileUtil::convertToOSPath(initPath, initPathOS);
	}

	FileResult result;
	std::string res = pfd::save_file(title, initPathOS, filters, forceOverwrite ? pfd::opt::force_overwrite : pfd::opt::none).result();
	result.push_back(res);

	return result;
}

}