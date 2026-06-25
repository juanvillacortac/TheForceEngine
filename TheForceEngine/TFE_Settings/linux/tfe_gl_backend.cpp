#include "tfe_gl_backend.h"
#include "linux_display.h"

#include <SDL.h>
#include <cstdlib>
#include <cstring>
#if defined(__linux__)
#include <strings.h>
#endif

TFE_GL_Backend tfe_gl_backend = TFE_GL_BACKEND_DESKTOP;

static int tfe_parse_force_env(const char* name)
{
	const char* env = std::getenv(name);
	if (!env || !env[0])
		return 0;
	if (!std::strcmp(env, "1") || !strcasecmp(env, "true") || !strcasecmp(env, "yes") || !strcasecmp(env, "on"))
		return 1;
	return 0;
}

static int tfe_parse_handheld_env()
{
	if (tfe_parse_force_env("TFE_HANDHELD"))
		return 1;
	const char* env = std::getenv("TFE_HANDHELD");
	if (!env || !env[0])
		return 0;
	return std::strcmp(env, "0") != 0;
}

TFE_GL_Backend tfe_PreferGLBackend()
{
#if defined(TFE_RUNTIME_GL)
	if (tfe_parse_force_env("TFE_FORCE_GLES"))
		return TFE_GL_BACKEND_GLES;
	if (tfe_parse_force_env("TFE_FORCE_GL"))
		return TFE_GL_BACKEND_DESKTOP;
	if (tfe_IsLinuxKmsDisplay() || tfe_parse_handheld_env())
		return TFE_GL_BACKEND_GLES;
#if defined(__aarch64__)
	return TFE_GL_BACKEND_GLES;
#else
	return TFE_GL_BACKEND_DESKTOP;
#endif
#else
	return TFE_GL_BACKEND_DESKTOP;
#endif
}

void tfe_SetGLBackend(TFE_GL_Backend backend)
{
	tfe_gl_backend = backend;
}

int tfe_UseGLES()
{
#if defined(TFE_RUNTIME_GL)
	return tfe_gl_backend == TFE_GL_BACKEND_GLES;
#else
	return 0;
#endif
}

int tfe_UseHandheld()
{
	return tfe_parse_handheld_env();
}

void tfe_InitGLBackendFromContext()
{
#if defined(TFE_RUNTIME_GL)
	int profile = 0;
	if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile) == 0
		&& profile == SDL_GL_CONTEXT_PROFILE_ES)
	{
		tfe_gl_backend = TFE_GL_BACKEND_GLES;
	}
	else
	{
		tfe_gl_backend = TFE_GL_BACKEND_DESKTOP;
	}
#endif
}

const char* tfe_GetGLSLVersionString()
{
	if (tfe_UseGLES())
		return "#version 300 es\n";
	if (strcmp(SDL_GetPlatform(), "Mac OS X") == 0)
		return "#version 410\n";
	return "#version 330\n";
}

const char* tfe_GetGLSLFragmentPrecision()
{
	return tfe_UseGLES() ? "precision mediump float;\n" : "";
}
