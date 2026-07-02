#pragma once

#include "gl.h"
#include <TFE_Settings/linux/tfe_gl_backend.h>

void tfe_InitGLESExtensions();
int tfe_GetGLESMajorVersion();
int tfe_GetGLESMinorVersion();
const char* tfe_GetGLSLVersionStringForBackend();
const char* tfe_GetGLSLVertexPreamble();
const char* tfe_GetGLSLFragmentPreamble();

void tfe_BindTexBuffer(GLenum target, GLenum internalformat, GLuint buffer);
void tfe_UniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);

int tfe_GLESHasTextureBuffer();
int tfe_GLESFragmentSupportsTextureBuffer();
int tfe_UseBufferTexture2D();
const char* tfe_GetShaderBuffer2DDefines();
int tfe_GLESQueryMaxTextureBufferSize();
int tfe_GLESUseClipDiscardFallback();
int tfe_GLESBifrostSectorShadersOK();
