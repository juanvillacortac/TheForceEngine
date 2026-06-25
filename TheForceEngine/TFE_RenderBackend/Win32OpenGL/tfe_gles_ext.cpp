#include "tfe_gles_ext.h"
#include "gl.h"
#include <TFE_System/system.h>
#include <SDL.h>
#include <cstring>

#ifndef GL_TEXTURE_BUFFER
#define GL_TEXTURE_BUFFER 0x8C2A
#endif
#ifndef GL_MAX_TEXTURE_BUFFER_SIZE
#define GL_MAX_TEXTURE_BUFFER_SIZE 0x919E
#endif

static int s_gles_major;
static int s_gles_minor;
static bool s_has_tex_buffer;
static bool s_has_frag_tex_buffer;
static bool s_has_clip_cull;

#ifndef APIENTRY
#define APIENTRY
#endif

typedef void (APIENTRY *TFE_TexBufferProc)(GLenum, GLenum, GLuint);
static TFE_TexBufferProc s_texbuf_pfn;

static void tfe_query_version()
{
	s_gles_major = 0;
	s_gles_minor = 0;
	if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &s_gles_major) != 0
		|| SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &s_gles_minor) != 0)
	{
		GLint maj = 0, min = 0;
		glGetIntegerv(GL_MAJOR_VERSION, &maj);
		glGetIntegerv(GL_MINOR_VERSION, &min);
		s_gles_major = maj;
		s_gles_minor = min;
	}
}

static void* tfe_get_proc(const char* name)
{
	return (void*)SDL_GL_GetProcAddress(name);
}

static bool tfe_probe_clip_cull_shader()
{
	static const char* clipTest =
		"#version 300 es\n"
		"#extension GL_EXT_clip_cull_distance : enable\n"
		"precision highp float;\n"
		"void main() {\n"
		"  gl_ClipDistance[0] = 1.0;\n"
		"  gl_Position = vec4(0.0);\n"
		"}\n";

	const GLuint shader = glCreateShader(GL_VERTEX_SHADER);
	if (!shader)
	{
		return false;
	}
	glShaderSource(shader, 1, &clipTest, nullptr);
	glCompileShader(shader);
	GLint ok = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok)
	{
		GLchar log[512];
		glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
		TFE_System::logWrite(LOG_WARNING, "GLES", "Clip/cull distance probe failed: %s", log);
	}
	glDeleteShader(shader);
	return ok != 0;
}

void tfe_InitGLESExtensions()
{
	s_has_tex_buffer = false;
	s_has_frag_tex_buffer = false;
	s_has_clip_cull = false;
	s_texbuf_pfn = nullptr;

	if (!tfe_UseGLES())
		return;

	tfe_query_version();

	if (s_gles_major > 3 || (s_gles_major == 3 && s_gles_minor >= 2))
	{
		s_has_tex_buffer = true;
		s_texbuf_pfn = (TFE_TexBufferProc)tfe_get_proc("glTexBuffer");
	}
	else
	{
		s_texbuf_pfn = (TFE_TexBufferProc)tfe_get_proc("glTexBufferOES");
		if (!s_texbuf_pfn)
			s_texbuf_pfn = (TFE_TexBufferProc)tfe_get_proc("glTexBufferEXT");
		s_has_tex_buffer = s_texbuf_pfn != nullptr
			&& (SDL_GL_ExtensionSupported("GL_OES_texture_buffer")
				|| SDL_GL_ExtensionSupported("GL_EXT_texture_buffer"));
	}

	// Mali and similar drivers often omit GL_EXT_clip_cull_distance from the extension
	// string even when the shader extension compiles — probe instead of trusting SDL.
	if (s_gles_major > 3 || (s_gles_major == 3 && s_gles_minor >= 1))
	{
		s_has_clip_cull = tfe_probe_clip_cull_shader();
		if (!s_has_clip_cull)
		{
			TFE_System::logWrite(LOG_MSG, "GLES",
				"Using fragment-discard portal clipping (GL_EXT_clip_cull_distance unavailable).");
		}
	}

	// Mali and other drivers may expose texture buffers for vertex shaders but not fragment.
	if (s_has_tex_buffer)
	{
		static const char* fragTest =
			"#version 300 es\n"
			"#extension GL_EXT_texture_buffer : enable\n"
			"precision highp float;\n"
			"precision highp int;\n"
			"precision highp isamplerBuffer;\n"
			"uniform isamplerBuffer Tex;\n"
			"out vec4 Out_Color;\n"
			"void main() { Out_Color = vec4(texelFetch(Tex, 0)); }\n";

		const GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);
		if (shader)
		{
			glShaderSource(shader, 1, &fragTest, nullptr);
			glCompileShader(shader);
			GLint ok = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
			s_has_frag_tex_buffer = ok != 0;
			if (!ok)
			{
				GLchar log[512];
				glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
				TFE_System::logWrite(LOG_WARNING, "GLES", "Fragment texture buffer unsupported: %s", log);
			}
			glDeleteShader(shader);
		}
	}

		TFE_System::logWrite(LOG_MSG, "GLES",
			"Context %d.%d — textureBuffer:%d fragTextureBuffer:%d clipCull:%d buffer2D:%d",
			s_gles_major, s_gles_minor,
			s_has_tex_buffer ? 1 : 0, s_has_frag_tex_buffer ? 1 : 0, s_has_clip_cull ? 1 : 0,
			tfe_UseBufferTexture2D() ? 1 : 0);
}

int tfe_GetGLESMajorVersion()
{
	return s_gles_major;
}

int tfe_GetGLESMinorVersion()
{
	return s_gles_minor;
}

const char* tfe_GetGLSLVersionStringForBackend()
{
	if (!tfe_UseGLES())
		return nullptr;
	// Mali and other embedded drivers are more reliable with 300 es even on GLES 3.2 contexts.
	return "#version 300 es\n";
}

// Mali and other strict GLES drivers need default float/int precision. uint and sampler
// types cannot take global precision qualifiers in GLSL ES (Mali error S0028).
static const char* tfe_GetGLSLBufferSamplerPrecision()
{
	if (!tfe_UseGLES() || !s_has_tex_buffer)
		return "";
	// GLES ES 3.0 treats buffer samplers as reserved until GL_EXT_texture_buffer is enabled,
	// and Mali requires default precision for those opaque sampler types.
	return
		"precision highp samplerBuffer;\n"
		"precision highp isamplerBuffer;\n"
		"precision highp usamplerBuffer;\n";
}

// 2D texture buffer emulation: sampler2DArray used by world/HUD shaders.
static const char* tfe_GetGLSLSampler2DBufferPrecision()
{
	if (!tfe_UseGLES() || !tfe_UseBufferTexture2D())
		return "";
	return "precision highp sampler2DArray;\n";
}

static const char* tfe_GetGLSLVertexPrecisionPreamble()
{
	return
		"precision highp float;\n"
		"precision highp int;\n";
}

static const char* tfe_GetGLSLFragmentPrecisionPreamble()
{
	return
		"precision highp float;\n"
		"precision highp int;\n";
}

const char* tfe_GetGLSLVertexPreamble()
{
	static char buf[512];
	if (!tfe_UseGLES())
		return "";
	buf[0] = 0;
	if (s_has_tex_buffer && s_has_frag_tex_buffer && !tfe_UseBufferTexture2D())
		strcat(buf, "#extension GL_EXT_texture_buffer : enable\n");
	if (s_has_clip_cull)
	{
		strcat(buf, "#extension GL_EXT_clip_cull_distance : enable\n");
		strcat(buf, "#define TFE_HAS_CLIP_CULL_DISTANCE 1\n");
	}
	else
	{
		strcat(buf, "#define TFE_CLIP_DISCARD_FALLBACK 1\n");
	}
	strcat(buf, tfe_GetGLSLVertexPrecisionPreamble());
	if (tfe_UseBufferTexture2D())
		strcat(buf, tfe_GetGLSLSampler2DBufferPrecision());
	if (s_has_tex_buffer && s_has_frag_tex_buffer && !tfe_UseBufferTexture2D())
		strcat(buf, tfe_GetGLSLBufferSamplerPrecision());
	return buf;
}

const char* tfe_GetGLSLFragmentPreamble()
{
	static char buf[512];
	if (!tfe_UseGLES())
		return "";
	buf[0] = 0;
	if (s_has_tex_buffer && s_has_frag_tex_buffer && !tfe_UseBufferTexture2D())
		strcat(buf, "#extension GL_EXT_texture_buffer : enable\n");
	if (!s_has_clip_cull)
		strcat(buf, "#define TFE_CLIP_DISCARD_FALLBACK 1\n");
	strcat(buf, tfe_GetGLSLFragmentPrecisionPreamble());
	if (tfe_UseBufferTexture2D())
		strcat(buf, tfe_GetGLSLSampler2DBufferPrecision());
	if (s_has_tex_buffer && s_has_frag_tex_buffer && !tfe_UseBufferTexture2D())
		strcat(buf, tfe_GetGLSLBufferSamplerPrecision());
	return buf;
}

void tfe_BindTexBuffer(GLenum target, GLenum internalformat, GLuint buffer)
{
	if (tfe_UseGLES())
	{
		if (!s_texbuf_pfn)
		{
			TFE_System::logWrite(LOG_ERROR, "GLES", "glTexBuffer unavailable on this GLES driver.");
			return;
		}
		s_texbuf_pfn(target, internalformat, buffer);
		return;
	}
	(glad_glTexBuffer)(target, internalformat, buffer);
}

void tfe_UniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
	if (!tfe_UseGLES())
	{
		(glad_glUniformMatrix4x3fv)(location, count, transpose, value);
		return;
	}

	for (GLsizei i = 0; i < count; i++)
	{
		const GLfloat* m = value + i * 12;
		GLfloat m4[16] = {
			m[0], m[1], m[2], 0.0f,
			m[3], m[4], m[5], 0.0f,
			m[6], m[7], m[8], 0.0f,
			m[9], m[10], m[11], 1.0f,
		};
		glUniformMatrix4fv(location + i, 1, GL_FALSE, m4);
	}
}

int tfe_GLESHasTextureBuffer()
{
	return s_has_tex_buffer ? 1 : 0;
}

int tfe_GLESFragmentSupportsTextureBuffer()
{
	return s_has_frag_tex_buffer ? 1 : 0;
}

int tfe_UseBufferTexture2D()
{
	return tfe_UseGLES() && !s_has_frag_tex_buffer;
}

const char* tfe_GetShaderBuffer2DDefines()
{
	if (!tfe_UseBufferTexture2D())
		return "";
	return "#define TFE_BUFFER_TEXTURE_2D 1\n"
		"#define TFE_BUFFER_TEXTURE_BYTES 1\n"
		"#define TFE_SHADER_BUFFER_WIDTH 1024\n"
		"#define TFE_GLES_SMOOTH_LIGHTRAMP 1\n"
		"#define TFE_GLES_LIGHT_DITHER 1\n";
}

int tfe_GLESQueryMaxTextureBufferSize()
{
	GLint size = 0;
	if (!tfe_UseGLES() || !s_has_tex_buffer)
		return 0;
	glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &size);
	return size;
}

int tfe_GLESUseClipDiscardFallback()
{
	return tfe_UseGLES() && !s_has_clip_cull;
}
