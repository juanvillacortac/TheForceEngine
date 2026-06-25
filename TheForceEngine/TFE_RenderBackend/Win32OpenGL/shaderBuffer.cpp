#include <TFE_RenderBackend/shaderBuffer.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_Settings/linux/tfe_gl_backend.h>
#include "tfe_gles_ext.h"
#include "tfe_gl_init.h"
#include "gl.h"
#include <memory.h>
#include "openGL_Caps.h"
#include <TFE_System/system.h>
#include <SDL.h>

namespace
{
	constexpr u32 TFE_SHADER_BUFFER_TEX_WIDTH = 1024;

	GLenum getFormat(const ShaderBufferDef& bufferDef)
	{
		if (bufferDef.channelCount == 1)
		{
			if (bufferDef.channelSize == 1)
			{
				return GL_R8;
			}
			else if (bufferDef.channelSize == 2)
			{
				return GL_R16;
			}
			else if (bufferDef.channelSize == 4)
			{
				if (bufferDef.channelType == BUF_CHANNEL_UINT)
				{
					return GL_R32UI;
				}
				else if (bufferDef.channelType == BUF_CHANNEL_INT)
				{
					return GL_R32I;
				}
				else if (bufferDef.channelType == BUF_CHANNEL_FLOAT)
				{
					return GL_R32F;
				}
			}
		}
		else if (bufferDef.channelCount == 2)
		{
			if (bufferDef.channelSize == 1)
			{
				return GL_RG8;
			}
			else if (bufferDef.channelSize == 2)
			{
				return GL_RG16;
			}
			else if (bufferDef.channelSize == 4)
			{
				if (bufferDef.channelType == BUF_CHANNEL_UINT)
				{
					return GL_RG32UI;
				}
				else if (bufferDef.channelType == BUF_CHANNEL_INT)
				{
					return GL_RG32I;
				}
				else if (bufferDef.channelType == BUF_CHANNEL_FLOAT)
				{
					return GL_RG32F;
				}
			}
		}
		else if (bufferDef.channelCount == 4)
		{
			if (bufferDef.channelSize == 1)
			{
				return GL_RGBA8;
			}
			else if (bufferDef.channelSize == 2)
			{
				return GL_RGBA16;
			}
			else if (bufferDef.channelSize == 4)
			{
				if (bufferDef.channelType == BUF_CHANNEL_UINT)
				{
					return GL_RGBA32UI;
				}
				else if (bufferDef.channelType == BUF_CHANNEL_INT)
				{
					return GL_RGBA32I;
				}
				else if (bufferDef.channelType == BUF_CHANNEL_FLOAT)
				{
					return GL_RGBA32F;
				}
			}
		}
		return GL_INVALID_ENUM;
	}

	bool shaderBuffer_isGLESContext()
	{
#if defined(TFE_RUNTIME_GL)
		if (tfe_UseGLES())
		{
			return true;
		}
		int profile = 0;
		if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile) == 0)
		{
			return profile == SDL_GL_CONTEXT_PROFILE_ES;
		}
#endif
		return false;
	}

	bool shaderBuffer_glContextReady()
	{
		// ARM Mali / EGL: avoid SDL_GL_MakeCurrent when the render thread already
		// owns a context (see Mali GLES Application Dev Guide §2.6 — single pipeline).
		if (SDL_GL_GetCurrentContext() != nullptr)
		{
			return true;
		}
		return tfe_EnsureGLContextCurrent();
	}

	bool shaderBuffer_wantsPacked2D()
	{
#if defined(TFE_RUNTIME_GL)
		return shaderBuffer_isGLESContext() || (tfe_UseBufferTexture2D() != 0);
#else
		return false;
#endif
	}

	bool ensureGlProc(void** pfn, const char* name)
	{
		if (!*pfn)
		{
			*pfn = (void*)SDL_GL_GetProcAddress(name);
		}
		if (!*pfn)
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "Missing GL proc: %s", name);
			return false;
		}
		return true;
	}

	bool ensureGlTextureProcs()
	{
		if (!shaderBuffer_isGLESContext() && !tfe_UseGLES())
		{
			return true;
		}
		struct ProcEntry { void** pfn; const char* name; };
		ProcEntry procs[] = {
			{ (void**)&glad_glGenTextures, "glGenTextures" },
			{ (void**)&glad_glBindTexture, "glBindTexture" },
			{ (void**)&glad_glTexParameteri, "glTexParameteri" },
			{ (void**)&glad_glTexImage2D, "glTexImage2D" },
			{ (void**)&glad_glTexSubImage2D, "glTexSubImage2D" },
			{ (void**)&glad_glDeleteTextures, "glDeleteTextures" },
			{ (void**)&glad_glGetError, "glGetError" },
			{ (void**)&glad_glActiveTexture, "glActiveTexture" },
		};
		for (const ProcEntry& entry : procs)
		{
			if (!ensureGlProc(entry.pfn, entry.name))
			{
				return false;
			}
		}
		return true;
	}

	bool allocTexture2D(GLenum storageFormat, u32 width, u32 height, GLenum format, GLenum type)
	{
		// Mali crashes or misbehaves with glTexStorage2D for buffer-emulation textures.
		if (shaderBuffer_isGLESContext() || tfe_UseGLES())
		{
			glTexImage2D(GL_TEXTURE_2D, 0, storageFormat, (GLsizei)width, (GLsizei)height, 0, format, type, nullptr);
			return glGetError() == GL_NO_ERROR;
		}
		if (glad_glTexStorage2D)
		{
			glTexStorage2D(GL_TEXTURE_2D, 1, storageFormat, width, height);
			if (glGetError() == GL_NO_ERROR)
			{
				return true;
			}
		}

		glTexImage2D(GL_TEXTURE_2D, 0, storageFormat, width, height, 0, format, type, nullptr);
		return glGetError() == GL_NO_ERROR;
	}

	bool getUploadParams(GLenum internalFormat, GLenum& format, GLenum& type)
	{
		switch (internalFormat)
		{
			case GL_R8: format = GL_RED; type = GL_UNSIGNED_BYTE; return true;
			case GL_RG8: format = GL_RG; type = GL_UNSIGNED_BYTE; return true;
			case GL_RGBA8: format = GL_RGBA; type = GL_UNSIGNED_BYTE; return true;
			case GL_R16: format = GL_RED; type = GL_UNSIGNED_SHORT; return true;
			case GL_RG16: format = GL_RG; type = GL_UNSIGNED_SHORT; return true;
			case GL_RGBA16: format = GL_RGBA; type = GL_UNSIGNED_SHORT; return true;
			case GL_R32F: format = GL_RED; type = GL_FLOAT; return true;
			case GL_RG32F: format = GL_RG; type = GL_FLOAT; return true;
			case GL_RGBA32F: format = GL_RGBA; type = GL_FLOAT; return true;
			case GL_R32I: format = GL_RED_INTEGER; type = GL_INT; return true;
			case GL_RG32I: format = GL_RG_INTEGER; type = GL_INT; return true;
			case GL_RGBA32I: format = GL_RGBA_INTEGER; type = GL_INT; return true;
			case GL_R32UI: format = GL_RED_INTEGER; type = GL_UNSIGNED_INT; return true;
			case GL_RG32UI: format = GL_RG_INTEGER; type = GL_UNSIGNED_INT; return true;
			case GL_RGBA32UI: format = GL_RGBA_INTEGER; type = GL_UNSIGNED_INT; return true;
			default: return false;
		}
	}

	void bindTexture2DParams()
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	void uploadTexture2DData(u32 texWidth, GLenum format, GLenum type, u32 stride, u32 texelCount, const void* data)
	{
		if (!texelCount || !data) { return; }

		const u32 fullRows = texelCount / texWidth;
		const u32 partial = texelCount % texWidth;
		const u8* src = (const u8*)data;

		if (fullRows > 0)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, fullRows, format, type, src);
			src += (size_t)fullRows * texWidth * stride;
		}
		if (partial > 0)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, fullRows, partial, 1, format, type, src);
		}
	}

	void uploadRgbaByteTexture2D(u32 texWidth, u32 byteCount, const void* data)
	{
		if (!byteCount || !data) { return; }

		const u32 texels = (byteCount + 3u) / 4u;
		const u32 fullRows = texels / texWidth;
		const u32 partial = texels % texWidth;
		const u8* src = (const u8*)data;

		if (fullRows > 0)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, fullRows, GL_RGBA, GL_UNSIGNED_BYTE, src);
			src += (size_t)fullRows * texWidth * 4u;
		}
		if (partial > 0)
		{
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, fullRows, partial, 1, GL_RGBA, GL_UNSIGNED_BYTE, src);
		}
	}
}

ShaderBuffer::~ShaderBuffer()
{
	destroy();
}

bool ShaderBuffer::create(u32 count, const ShaderBufferDef& bufferDef, bool dynamic, void* initData)
{
	if (!count) { return false; }

	TFE_System::logWrite(LOG_MSG, "ShaderBuffer", "create enter (count %u, gles %d, buf2d %d).",
		count, tfe_UseGLES(), tfe_UseBufferTexture2D());

	if (!shaderBuffer_glContextReady())
	{
		TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "GL context not current for create().");
		return false;
	}
#if defined(TFE_RUNTIME_GL)
	if (shaderBuffer_isGLESContext() && glad_glGetError)
	{
		while (glad_glGetError() != GL_NO_ERROR) {}
	}
#endif
	TFE_System::logWrite(LOG_MSG, "ShaderBuffer", "GL context OK.");

	GLenum internalFormat = getFormat(bufferDef);
	if (internalFormat == GL_INVALID_ENUM)
	{
		TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "Invalid buffer format (channels %u, size %u, type %d).",
			bufferDef.channelCount, bufferDef.channelSize, (int)bufferDef.channelType);
		return false;
	}

	m_bufferDef = bufferDef;
	m_stride  = m_bufferDef.channelCount * m_bufferDef.channelSize;
	m_count   = count;
	m_size    = m_stride * m_count;
	m_dynamic = dynamic;

	// GLES / Mali: always use RGBA8 2D packing (same as TexturePacker table — never GL_TEXTURE_BUFFER).
	if (shaderBuffer_wantsPacked2D())
	{
		if (!ensureGlTextureProcs() || !tfe_EnsureGLESProcs())
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "GLES proc table incomplete for packed 2D buffer.");
			return false;
		}
		TFE_System::logWrite(LOG_MSG, "ShaderBuffer", "GLES procs OK.");

		m_useTexture2D = true;
		m_byteTexture2D = true;
		m_tex2dWidth = TFE_SHADER_BUFFER_TEX_WIDTH;
		const u32 rgbaTexels = (m_size + 3u) / 4u;
		m_tex2dHeight = (rgbaTexels + m_tex2dWidth - 1u) / m_tex2dWidth;

		TFE_System::logWrite(LOG_MSG, "ShaderBuffer", "GLES byte-packed 2D buffer %ux%u (%u bytes).",
			m_tex2dWidth, m_tex2dHeight, m_size);

		if (!glad_glGenTextures || !glad_glBindTexture || !glad_glTexImage2D)
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "Missing GL texture procs (gen %p bind %p image %p).",
				(void*)glad_glGenTextures, (void*)glad_glBindTexture, (void*)glad_glTexImage2D);
			return false;
		}

		glGenTextures(1, &m_gpuHandle[1]);
		if (!m_gpuHandle[1])
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "glGenTextures returned 0.");
			return false;
		}

		glBindTexture(GL_TEXTURE_2D, m_gpuHandle[1]);
		bindTexture2DParams();
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)m_tex2dWidth, (GLsizei)m_tex2dHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		const GLenum texErr = glGetError();
		if (texErr != GL_NO_ERROR)
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "glTexImage2D RGBA8 failed (0x%x).", texErr);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &m_gpuHandle[1]);
			m_gpuHandle[1] = 0;
			return false;
		}

		if (initData)
		{
			uploadRgbaByteTexture2D(m_tex2dWidth, m_size, initData);
		}

		glBindTexture(GL_TEXTURE_2D, 0);
		m_gpuHandle[0] = 0;
		m_initialized = true;
		TFE_System::logWrite(LOG_MSG, "ShaderBuffer", "GLES byte-packed 2D buffer ready (tex %u).", m_gpuHandle[1]);
		return true;
	}

	m_useTexture2D = tfe_UseBufferTexture2D() != 0;
	m_byteTexture2D = false;

	if (m_useTexture2D)
	{
		if (!ensureGlTextureProcs())
		{
			return false;
		}

		GLenum storageFormat = internalFormat;
		GLenum format = GL_RGBA;
		GLenum type = GL_UNSIGNED_BYTE;
		if (!getUploadParams(internalFormat, format, type))
		{
			return false;
		}
		m_tex2dWidth = TFE_SHADER_BUFFER_TEX_WIDTH;
		m_tex2dHeight = (m_count + m_tex2dWidth - 1) / m_tex2dWidth;

		glGenTextures(1, &m_gpuHandle[1]);
		if (!m_gpuHandle[1])
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "glGenTextures returned 0.");
			return false;
		}
		glBindTexture(GL_TEXTURE_2D, m_gpuHandle[1]);
		bindTexture2DParams();
		if (!allocTexture2D(storageFormat, m_tex2dWidth, m_tex2dHeight, format, type))
		{
			TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "Failed to allocate 2D buffer texture %ux%u (fmt 0x%x).",
				m_tex2dWidth, m_tex2dHeight, storageFormat);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &m_gpuHandle[1]);
			m_gpuHandle[1] = 0;
			return false;
		}

		uploadTexture2DData(m_tex2dWidth, format, type, m_stride, m_count, initData);

		glBindTexture(GL_TEXTURE_2D, 0);
		m_gpuHandle[0] = 0;
		m_initialized = true;
		return true;
	}

	if (shaderBuffer_isGLESContext())
	{
		TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "Refusing GL_TEXTURE_BUFFER path on GLES.");
		return false;
	}

	glGenBuffers(1, &m_gpuHandle[0]);
	glBindBuffer(GL_TEXTURE_BUFFER, m_gpuHandle[0]);
	glBufferData(GL_TEXTURE_BUFFER, m_size, initData, dynamic ? GL_STREAM_DRAW : GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);

	glGenTextures(1, &m_gpuHandle[1]);
	glBindTexture(GL_TEXTURE_BUFFER, m_gpuHandle[1]);
	tfe_BindTexBuffer(GL_TEXTURE_BUFFER, internalFormat, m_gpuHandle[0]);
	glBindTexture(GL_TEXTURE_BUFFER, 0);

	m_initialized = true;
	return true;
}

void ShaderBuffer::destroy()
{
	if (m_initialized)
	{
		if (m_gpuHandle[1]) { glDeleteTextures(1, &m_gpuHandle[1]); }
		if (m_gpuHandle[0]) { glDeleteBuffers(1, &m_gpuHandle[0]); }
	}
	m_gpuHandle[0] = 0;
	m_gpuHandle[1] = 0;
	m_initialized = false;
	m_useTexture2D = false;
	m_byteTexture2D = false;
	m_tex2dWidth = 0;
	m_tex2dHeight = 0;
}

void ShaderBuffer::update(const void* buffer, size_t size)
{
	if (!tfe_EnsureGLContextCurrent())
	{
		TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "GL context not current for update().");
		return;
	}
	if (shaderBuffer_isGLESContext() && !tfe_EnsureGLESProcs())
	{
		TFE_System::logWrite(LOG_ERROR, "ShaderBuffer", "GLES proc table incomplete for update().");
		return;
	}

	if (m_useTexture2D)
	{
		glBindTexture(GL_TEXTURE_2D, m_gpuHandle[1]);
		if (m_byteTexture2D)
		{
			uploadRgbaByteTexture2D(m_tex2dWidth, (u32)size, buffer);
		}
		else
		{
			GLenum internalFormat = getFormat(m_bufferDef);
			GLenum format = GL_RGBA;
			GLenum type = GL_UNSIGNED_BYTE;
			if (!getUploadParams(internalFormat, format, type))
			{
				glBindTexture(GL_TEXTURE_2D, 0);
				return;
			}
			const u32 texels = (u32)(size / m_stride);
			uploadTexture2DData(m_tex2dWidth, format, type, m_stride, texels, buffer);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		return;
	}

	glBindBuffer(GL_TEXTURE_BUFFER, m_gpuHandle[0]);
	glBufferData(GL_TEXTURE_BUFFER, size, buffer, m_dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
	glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void ShaderBuffer::bind(s32 bindPoint) const
{
	if (bindPoint < 0) { return; }
	glActiveTexture(GL_TEXTURE0 + bindPoint);
	if (m_useTexture2D)
	{
		glBindTexture(GL_TEXTURE_2D, m_gpuHandle[1]);
	}
	else
	{
		glBindTexture(GL_TEXTURE_BUFFER, m_gpuHandle[1]);
	}
}

void ShaderBuffer::unbind(s32 bindPoint) const
{
	if (bindPoint < 0) { return; }
	glActiveTexture(GL_TEXTURE0 + bindPoint);
	if (m_useTexture2D)
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	else
	{
		glBindTexture(GL_TEXTURE_BUFFER, 0);
	}
}

s32 ShaderBuffer::getMaxSize()
{
	if (tfe_UseBufferTexture2D())
	{
		GLint maxTexSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
		return maxTexSize * TFE_SHADER_BUFFER_TEX_WIDTH;
	}
	return OpenGL_Caps::getMaxTextureBufferSize();
}
