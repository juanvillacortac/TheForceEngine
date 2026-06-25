#include "tfe_gl_init.h"
#include "tfe_gles_ext.h"
#include "gl.h"
#include <TFE_System/system.h>
#include <SDL.h>
#include <cstring>

#if defined(TFE_RUNTIME_GL)
static bool tfe_ensure_gl_proc(void** pfn, const char* name)
{
	if (*pfn)
		return true;
	*pfn = (void*)SDL_GL_GetProcAddress(name);
	if (!*pfn)
	{
		TFE_System::logWrite(LOG_ERROR, "GLES", "Missing GL proc: %s", name);
		return false;
	}
	return true;
}

static bool tfe_load_gles_proc_table()
{
	struct ProcEntry { void** pfn; const char* name; };
	ProcEntry procs[] = {
		{ (void**)&glad_glCreateShader, "glCreateShader" },
		{ (void**)&glad_glShaderSource, "glShaderSource" },
		{ (void**)&glad_glCompileShader, "glCompileShader" },
		{ (void**)&glad_glGetShaderiv, "glGetShaderiv" },
		{ (void**)&glad_glGetShaderInfoLog, "glGetShaderInfoLog" },
		{ (void**)&glad_glCreateProgram, "glCreateProgram" },
		{ (void**)&glad_glAttachShader, "glAttachShader" },
		{ (void**)&glad_glBindAttribLocation, "glBindAttribLocation" },
		{ (void**)&glad_glLinkProgram, "glLinkProgram" },
		{ (void**)&glad_glGetProgramiv, "glGetProgramiv" },
		{ (void**)&glad_glGetProgramInfoLog, "glGetProgramInfoLog" },
		{ (void**)&glad_glDeleteShader, "glDeleteShader" },
		{ (void**)&glad_glDeleteProgram, "glDeleteProgram" },
		{ (void**)&glad_glUseProgram, "glUseProgram" },
		{ (void**)&glad_glGenTextures, "glGenTextures" },
		{ (void**)&glad_glBindTexture, "glBindTexture" },
		{ (void**)&glad_glTexImage2D, "glTexImage2D" },
		{ (void**)&glad_glTexSubImage2D, "glTexSubImage2D" },
		{ (void**)&glad_glTexStorage2D, "glTexStorage2D" },
		{ (void**)&glad_glTexParameteri, "glTexParameteri" },
		{ (void**)&glad_glDeleteTextures, "glDeleteTextures" },
		{ (void**)&glad_glGenBuffers, "glGenBuffers" },
		{ (void**)&glad_glBindBuffer, "glBindBuffer" },
		{ (void**)&glad_glBufferData, "glBufferData" },
		{ (void**)&glad_glBufferSubData, "glBufferSubData" },
		{ (void**)&glad_glDeleteBuffers, "glDeleteBuffers" },
		{ (void**)&glad_glGenVertexArrays, "glGenVertexArrays" },
		{ (void**)&glad_glBindVertexArray, "glBindVertexArray" },
		{ (void**)&glad_glDeleteVertexArrays, "glDeleteVertexArrays" },
		{ (void**)&glad_glEnableVertexAttribArray, "glEnableVertexAttribArray" },
		{ (void**)&glad_glDisableVertexAttribArray, "glDisableVertexAttribArray" },
		{ (void**)&glad_glVertexAttribPointer, "glVertexAttribPointer" },
		{ (void**)&glad_glDrawElements, "glDrawElements" },
		{ (void**)&glad_glGetUniformLocation, "glGetUniformLocation" },
		{ (void**)&glad_glUniform1f, "glUniform1f" },
		{ (void**)&glad_glUniform1fv, "glUniform1fv" },
		{ (void**)&glad_glUniform1i, "glUniform1i" },
		{ (void**)&glad_glUniform2fv, "glUniform2fv" },
		{ (void**)&glad_glUniform3fv, "glUniform3fv" },
		{ (void**)&glad_glUniform4fv, "glUniform4fv" },
		{ (void**)&glad_glUniform1ui, "glUniform1ui" },
		{ (void**)&glad_glUniformMatrix3fv, "glUniformMatrix3fv" },
		{ (void**)&glad_glUniformMatrix4fv, "glUniformMatrix4fv" },
		{ (void**)&glad_glGetIntegerv, "glGetIntegerv" },
		{ (void**)&glad_glGetString, "glGetString" },
		{ (void**)&glad_glGetError, "glGetError" },
		{ (void**)&glad_glClear, "glClear" },
		{ (void**)&glad_glClearColor, "glClearColor" },
		{ (void**)&glad_glClearDepthf, "glClearDepthf" },
		{ (void**)&glad_glViewport, "glViewport" },
		{ (void**)&glad_glEnable, "glEnable" },
		{ (void**)&glad_glDisable, "glDisable" },
		{ (void**)&glad_glBlendFunc, "glBlendFunc" },
		{ (void**)&glad_glBlendEquation, "glBlendEquation" },
		{ (void**)&glad_glBlendFuncSeparate, "glBlendFuncSeparate" },
		{ (void**)&glad_glDepthFunc, "glDepthFunc" },
		{ (void**)&glad_glDepthMask, "glDepthMask" },
		{ (void**)&glad_glColorMask, "glColorMask" },
		{ (void**)&glad_glStencilFunc, "glStencilFunc" },
		{ (void**)&glad_glStencilOp, "glStencilOp" },
		{ (void**)&glad_glStencilMask, "glStencilMask" },
		{ (void**)&glad_glCullFace, "glCullFace" },
		{ (void**)&glad_glPolygonOffset, "glPolygonOffset" },
		{ (void**)&glad_glScissor, "glScissor" },
		{ (void**)&glad_glActiveTexture, "glActiveTexture" },
		{ (void**)&glad_glTexImage3D, "glTexImage3D" },
		{ (void**)&glad_glTexSubImage3D, "glTexSubImage3D" },
		{ (void**)&glad_glGenFramebuffers, "glGenFramebuffers" },
		{ (void**)&glad_glBindFramebuffer, "glBindFramebuffer" },
		{ (void**)&glad_glDeleteFramebuffers, "glDeleteFramebuffers" },
		{ (void**)&glad_glFramebufferTexture2D, "glFramebufferTexture2D" },
		{ (void**)&glad_glFramebufferRenderbuffer, "glFramebufferRenderbuffer" },
		{ (void**)&glad_glGenRenderbuffers, "glGenRenderbuffers" },
		{ (void**)&glad_glBindRenderbuffer, "glBindRenderbuffer" },
		{ (void**)&glad_glRenderbufferStorage, "glRenderbufferStorage" },
		{ (void**)&glad_glDeleteRenderbuffers, "glDeleteRenderbuffers" },
		{ (void**)&glad_glDrawBuffers, "glDrawBuffers" },
		{ (void**)&glad_glCheckFramebufferStatus, "glCheckFramebufferStatus" },
		{ (void**)&glad_glBlitFramebuffer, "glBlitFramebuffer" },
		{ (void**)&glad_glClearStencil, "glClearStencil" },
		{ (void**)&glad_glDepthRangef, "glDepthRangef" },
		{ (void**)&glad_glDrawArrays, "glDrawArrays" },
	};
	for (const ProcEntry& entry : procs)
	{
		if (!tfe_ensure_gl_proc(entry.pfn, entry.name))
			return false;
	}
	return true;
}

bool tfe_EnsureGLESProcs()
{
	if (!tfe_UseGLES())
		return true;
	return tfe_load_gles_proc_table();
}
#endif

static SDL_Window* s_activeGlWindow = nullptr;
static SDL_GLContext s_activeGlContext = nullptr;

void tfe_SetActiveGLContext(SDL_Window* window, SDL_GLContext context)
{
	s_activeGlWindow = window;
	s_activeGlContext = context;
}

bool tfe_EnsureGLContextCurrent()
{
	if (!s_activeGlWindow || !s_activeGlContext)
	{
		TFE_System::logWrite(LOG_ERROR, "GLES", "No active GL context.");
		return false;
	}
	if (SDL_GL_GetCurrentContext() == s_activeGlContext)
	{
		return true;
	}
	// Mali handheld: SDL_GL_MakeCurrent after shader compile can SIGSEGV when a
	// different (but valid) context is already bound on the render thread.
	if (SDL_GL_GetCurrentContext() != nullptr)
	{
		return true;
	}
	if (SDL_GL_MakeCurrent(s_activeGlWindow, s_activeGlContext) != 0)
	{
		TFE_System::logWrite(LOG_ERROR, "GLES", "SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		return false;
	}
	return true;
}

void tfe_ApplyGLESAttributes(int major, int minor)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
}

void tfe_SetGLAttributesForBackend(TFE_GL_Backend backend)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	if (backend == TFE_GL_BACKEND_GLES)
	{
		tfe_ApplyGLESAttributes(3, 2);
	}
	else if (strcmp(SDL_GetPlatform(), "Mac OS X") == 0)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	}
}

SDL_GLContext tfe_CreateGLContext(SDL_Window* window)
{
	return SDL_GL_CreateContext(window);
}

bool tfe_LoadGraphicsAPI()
{
	int glver = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
	if (glver == 0)
	{
		const char* version = (const char*)glGetString(GL_VERSION);
		if (!version || !version[0])
		{
			TFE_System::logWrite(LOG_ERROR, "RenderBackend", "cannot initialize GL loader");
			return false;
		}
		TFE_System::logWrite(LOG_WARNING, "RenderBackend", "GLAD version probe failed; using GL_VERSION: %s", version);
	}

#if defined(TFE_RUNTIME_GL)
	tfe_InitGLBackendFromContext();
	if (tfe_UseGLES())
	{
		tfe_InitGLESExtensions();
		if (!tfe_EnsureGLESProcs())
		{
			TFE_System::logWrite(LOG_ERROR, "RenderBackend", "GLES proc table incomplete");
			return false;
		}
	}
#endif

	return true;
}
