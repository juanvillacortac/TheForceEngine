#pragma once

#include <TFE_System/types.h>

enum TFE_GL_Backend
{
	TFE_GL_BACKEND_DESKTOP = 0,
	TFE_GL_BACKEND_GLES = 1,
};

extern TFE_GL_Backend tfe_gl_backend;

TFE_GL_Backend tfe_PreferGLBackend();
void tfe_SetGLBackend(TFE_GL_Backend backend);
int tfe_UseGLES();
// Handheld UI/input/port behavior: true only when TFE_HANDHELD is set (not by CPU arch).
// GL backend selection is separate — see tfe_PreferGLBackend().
int tfe_UseHandheld();
// Tank/strafe profile for handhelds without analog sticks (TFE_HANDHELD_NO_STICKS=1).
int tfe_UseHandheldNoSticks();
void tfe_InitGLBackendFromContext();
const char* tfe_GetGLSLVersionString();
const char* tfe_GetGLSLFragmentPrecision();
