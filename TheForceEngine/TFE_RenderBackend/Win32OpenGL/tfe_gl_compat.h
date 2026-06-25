#pragma once

#include <TFE_Settings/linux/tfe_gl_backend.h>
#include "gl.h"

inline void tfe_glClearDepth(f32 depth)
{
#if defined(TFE_RUNTIME_GL)
	if (tfe_UseGLES())
		glClearDepthf(depth);
	else
#endif
		glClearDepth(depth);
}

inline void tfe_glDepthRange(f32 nearVal, f32 farVal)
{
#if defined(TFE_RUNTIME_GL)
	if (tfe_UseGLES())
		glDepthRangef(nearVal, farVal);
	else
#endif
		glDepthRange((GLdouble)nearVal, (GLdouble)farVal);
}

inline void tfe_glFramebufferTexture(GLenum target, GLenum attachment, GLuint texture, GLint level)
{
#if defined(TFE_RUNTIME_GL)
	if (tfe_UseGLES())
		glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level);
	else
#endif
		glFramebufferTexture(target, attachment, texture, level);
}

inline void tfe_glPolygonMode(GLenum face, GLenum mode)
{
#if defined(TFE_RUNTIME_GL)
	if (tfe_UseGLES())
		return;
#endif
	glPolygonMode(face, mode);
}

inline void tfe_glClipDistance(GLenum plane, bool enable)
{
#if defined(TFE_RUNTIME_GL)
	if (tfe_UseGLES())
	{
		if (enable)
			glEnable(plane);
		else
			glDisable(plane);
		return;
	}
#endif
	if (enable)
		glEnable(plane);
	else
		glDisable(plane);
}
