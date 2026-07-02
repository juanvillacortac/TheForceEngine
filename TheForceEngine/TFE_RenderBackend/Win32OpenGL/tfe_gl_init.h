#pragma once

#include <TFE_Settings/linux/tfe_gl_backend.h>
#include <SDL.h>

void tfe_SetGLAttributesForBackend(TFE_GL_Backend backend);
void tfe_ApplyGLESAttributes(int major, int minor);
SDL_GLContext tfe_CreateGLContext(SDL_Window* window);
bool tfe_LoadGraphicsAPI();
void tfe_SetActiveGLContext(SDL_Window* window, SDL_GLContext context);
void tfe_DestroyActiveGLContext();
bool tfe_EnsureGLContextCurrent();
bool tfe_EnsureGLESProcs();
