#include "glslParser.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_RenderBackend/renderBackend.h>
#include "tfe_gles_ext.h"
#include "gl.h"
#include <assert.h>
#include <vector>
#include <string>
#include <SDL.h> 

namespace ShaderGL
{
	static const char* c_shaderAttrName[]=
	{
		"vtx_pos",   // ATTR_POS
		"vtx_nrm",   // ATTR_NRM
		"vtx_uv",    // ATTR_UV
		"vtx_uv1",   // ATTR_UV1
		"vtx_uv2",   // ATTR_UV2
		"vtx_uv3",   // ATTR_UV3
		"vtx_color", // ATTR_COLOR
	};

	static const s32 c_glslVersion[] = { 130, 330, 450 };
	static const GLchar* c_glslVersionString[] = { "#version 130\n", "#version 330\n", "#version 450\n" };
	static std::vector<char> s_buffers[2];
	static std::string s_defineString;
	static std::string s_vertexFile, s_fragmentFile;
}

bool Shader::create(const char* vertexShaderGLSL, const char* fragmentShaderGLSL, const char* defineString/* = nullptr*/, ShaderVersion version/* = SHADER_VER_COMPTABILE*/)
{
	destroy();

	// Create shaders
	m_shaderVersion = version;

	const GLchar* version_string;
	const GLchar* vert_preamble = "";
	const GLchar* frag_preamble = "";
	static std::string s_buffer2dDefineMerge;
	const char* effectiveDefines = defineString;
	if (strcmp(SDL_GetPlatform(), "Mac OS X") == 0) {
		version_string = "#version 410\n";
	} else if (tfe_UseGLES()) {
		version_string = tfe_GetGLSLVersionStringForBackend();
		vert_preamble = tfe_GetGLSLVertexPreamble();
		frag_preamble = tfe_GetGLSLFragmentPreamble();
	} else {
		version_string = ShaderGL::c_glslVersionString[m_shaderVersion];
	}

	const char* buffer2dDefines = tfe_GetShaderBuffer2DDefines();
	if (buffer2dDefines && buffer2dDefines[0])
	{
		s_buffer2dDefineMerge = buffer2dDefines;
		if (defineString && defineString[0])
		{
			s_buffer2dDefineMerge += defineString;
		}
		effectiveDefines = s_buffer2dDefineMerge.c_str();
	}

	// Mali GLES: merge preamble + defines + body into one source string so #defines
	// are visible to #included headers (bufferAccess.h) in the same translation unit.
	static std::string s_vertSourceMerge;
	static std::string s_fragSourceMerge;
	const char* vertShaderSrc = vertexShaderGLSL;
	const char* fragShaderSrc = fragmentShaderGLSL;
	if (tfe_UseGLES())
	{
		s_vertSourceMerge.clear();
		if (vert_preamble[0]) { s_vertSourceMerge += vert_preamble; }
		if (effectiveDefines && effectiveDefines[0]) { s_vertSourceMerge += effectiveDefines; }
		s_vertSourceMerge += vertexShaderGLSL;
		vertShaderSrc = s_vertSourceMerge.c_str();

		s_fragSourceMerge.clear();
		if (frag_preamble[0]) { s_fragSourceMerge += frag_preamble; }
		if (effectiveDefines && effectiveDefines[0]) { s_fragSourceMerge += effectiveDefines; }
		s_fragSourceMerge += fragmentShaderGLSL;
		fragShaderSrc = s_fragSourceMerge.c_str();
	}

	const GLchar* vert_parts[3];
	int vert_count = 0;
	vert_parts[vert_count++] = version_string;
	if (!tfe_UseGLES())
	{
		if (vert_preamble[0]) { vert_parts[vert_count++] = vert_preamble; }
		if (effectiveDefines && effectiveDefines[0]) { vert_parts[vert_count++] = effectiveDefines; }
		vert_parts[vert_count++] = vertexShaderGLSL;
	}
	else
	{
		vert_parts[vert_count++] = vertShaderSrc;
	}
	u32 vertHandle = glCreateShader(GL_VERTEX_SHADER);
	if (!vertHandle)
	{
		TFE_System::logWrite(LOG_ERROR, "Shader", "glCreateShader(vertex) failed");
		return false;
	}
	glShaderSource(vertHandle, vert_count, vert_parts, NULL);
	glCompileShader(vertHandle);

	GLint success = 0;
	glGetShaderiv(vertHandle, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLchar infoLog[2048];
		glGetShaderInfoLog(vertHandle, sizeof(infoLog), NULL, infoLog);
		TFE_System::logWrite(LOG_ERROR, "Shader", "Vertex shader compilation failed:\n%s", infoLog);
		glDeleteShader(vertHandle);
		return false;
	}

	const GLchar* frag_parts[3];
	int frag_count = 0;
	frag_parts[frag_count++] = version_string;
	if (!tfe_UseGLES())
	{
		if (frag_preamble[0]) { frag_parts[frag_count++] = frag_preamble; }
		if (effectiveDefines && effectiveDefines[0]) { frag_parts[frag_count++] = effectiveDefines; }
		frag_parts[frag_count++] = fragmentShaderGLSL;
	}
	else
	{
		frag_parts[frag_count++] = fragShaderSrc;
	}
	u32 fragHandle = glCreateShader(GL_FRAGMENT_SHADER);
	if (!fragHandle)
	{
		TFE_System::logWrite(LOG_ERROR, "Shader", "glCreateShader(fragment) failed");
		glDeleteShader(vertHandle);
		return false;
	}
	glShaderSource(fragHandle, frag_count, frag_parts, NULL);
	glCompileShader(fragHandle);
	glGetShaderiv(fragHandle, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		GLchar infoLog[2048];
		glGetShaderInfoLog(fragHandle, sizeof(infoLog), NULL, infoLog);
		TFE_System::logWrite(LOG_ERROR, "Shader", "Fragment shader compilation failed:\n%s", infoLog);
		glDeleteShader(fragHandle);
		glDeleteShader(vertHandle);
		return false;
	}

	m_gpuHandle = glCreateProgram();
	if (!m_gpuHandle)
	{
		TFE_System::logWrite(LOG_ERROR, "Shader", "glCreateProgram() failed");
		glDeleteShader(fragHandle);
		glDeleteShader(vertHandle);
		return false;
	}
	glAttachShader(m_gpuHandle, vertHandle);
	glAttachShader(m_gpuHandle, fragHandle);
	// Bind vertex attribute names to slots.
	for (u32 i = 0; i < ATTR_COUNT; i++)
	{
		glBindAttribLocation(m_gpuHandle, i, ShaderGL::c_shaderAttrName[i]);
	}

	glLinkProgram(m_gpuHandle);

	glGetProgramiv(m_gpuHandle, GL_LINK_STATUS, &success);
	if (!success)
	{
		GLchar infoLog[2048];
		glGetProgramInfoLog(m_gpuHandle, sizeof(infoLog), NULL, infoLog);
		TFE_System::logWrite(LOG_ERROR, "Shader", "Shader program linking failed:\n%s", infoLog);
		glDeleteProgram(m_gpuHandle);
		m_gpuHandle = 0;
		glDeleteShader(fragHandle);
		glDeleteShader(vertHandle);
		return false;
	}

	// Clean up shader objects
	glDeleteShader(vertHandle);
	glDeleteShader(fragHandle);

	return true;
}

bool Shader::load(const char* vertexShaderFile, const char* fragmentShaderFile, u32 defineCount/* = 0*/, ShaderDefine* defines/* = nullptr*/, ShaderVersion version/* = SHADER_VER_COMPTABILE*/)
{
	m_shaderVersion = version;
	ShaderGL::s_buffers[0].clear();
	ShaderGL::s_buffers[1].clear();

	GLSLParser::parseFile(vertexShaderFile,   ShaderGL::s_buffers[0]);
	GLSLParser::parseFile(fragmentShaderFile, ShaderGL::s_buffers[1]);

	if (ShaderGL::s_buffers[0].empty() || ShaderGL::s_buffers[1].empty())
	{
		TFE_System::logWrite(LOG_ERROR, "Shader", "Failed to parse shader files '%s' / '%s'", vertexShaderFile, fragmentShaderFile);
		return false;
	}

	ShaderGL::s_vertexFile = vertexShaderFile;
	ShaderGL::s_fragmentFile = fragmentShaderFile;

	ShaderGL::s_buffers[0].push_back(0);
	ShaderGL::s_buffers[1].push_back(0);

	// Build a string of defines.
	ShaderGL::s_defineString.clear();
	if (defineCount)
	{
		ShaderGL::s_defineString += "\r\n";
		for (u32 i = 0; i < defineCount; i++)
		{
			ShaderGL::s_defineString += "#define ";
			ShaderGL::s_defineString += defines[i].name;
			ShaderGL::s_defineString += " ";
			ShaderGL::s_defineString += defines[i].value;
			ShaderGL::s_defineString += "\r\n";
		}
		ShaderGL::s_defineString += "\r\n";
	}

	const bool result = create(ShaderGL::s_buffers[0].data(), ShaderGL::s_buffers[1].data(), ShaderGL::s_defineString.c_str(), m_shaderVersion);
	if (!result)
	{
		TFE_System::logWrite(LOG_ERROR, "Shader", "Failed to load '%s' / '%s'", vertexShaderFile, fragmentShaderFile);
	}
	return result;
}

void Shader::enableClipPlanes(s32 count)
{
	m_clipPlaneCount = count;
}

void Shader::destroy()
{
	if (m_gpuHandle)
	{
		glDeleteProgram(m_gpuHandle);
	}
	m_gpuHandle = 0;
}

void Shader::bind()
{
	if (!m_gpuHandle)
	{
		return;
	}
	TFE_RenderBackend::bindGlobalVAO();	// for macOS GL
	glUseProgram(m_gpuHandle);
	TFE_RenderState::enableClipPlanes(m_clipPlaneCount);
}

void Shader::unbind()
{
	glUseProgram(0);
}

s32 Shader::getVariableId(const char* name)
{
	return glGetUniformLocation(m_gpuHandle, name);
}

// For debugging.
s32 Shader::getVariables()
{
	s32 length;
	s32 size;
	GLenum type;
	char name[256];

	s32 count;
	glGetProgramiv(m_gpuHandle, GL_ACTIVE_UNIFORMS, &count);
	printf("Active Uniforms: %d\n", count);

	for (s32 i = 0; i < count; i++)
	{
		glGetActiveUniform(m_gpuHandle, (GLuint)i, 256, &length, &size, &type, name);
		printf("Uniform #%d Type: %u Name: %s\n", i, type, name);
	}

	s32 attribCount;
	glGetProgramiv(m_gpuHandle, GL_ACTIVE_ATTRIBUTES, &attribCount);
	printf("Active Attributes: %d\n", attribCount);

	for (s32 i = 0; i < attribCount; i++)
	{
		glGetActiveAttrib(m_gpuHandle, (GLuint)i, 256, &length, &size, &type, name);
		printf("Attribute #%d Type: %u Name: %s\n", i, type, name);
	}

	return count;
}

void Shader::bindTextureNameToSlot(const char* texName, s32 slot)
{
	const s32 curSlot = glGetUniformLocation(m_gpuHandle, texName);
	if (curSlot < 0 || slot < 0) { return; }

	bind();
	glUniform1i(curSlot, slot);
	unbind();
}

void Shader::setVariable(s32 id, ShaderVariableType type, const f32* data)
{
	if (id < 0) { return; }

	switch (type)
	{
	case SVT_SCALAR:
		glUniform1f(id, data[0]);
		break;
	case SVT_VEC2:
		glUniform2fv(id, 1, data);
		break;
	case SVT_VEC3:
		glUniform3fv(id, 1, data);
		break;
	case SVT_VEC4:
		glUniform4fv(id, 1, data);
		break;
	case SVT_MAT3x3:
		glUniformMatrix3fv(id, 1, false, data);
		break;
	case SVT_MAT4x3:
		tfe_UniformMatrix4x3fv(id, 1, false, data);
		break;
	case SVT_MAT4x4:
		glUniformMatrix4fv(id, 1, false, data);
		break;
	default:
		TFE_System::logWrite(LOG_ERROR, "Shader", "Mismatched parameter type.");
		assert(0);
	}
}

void Shader::setVariableArray(s32 id, ShaderVariableType type, const f32* data, u32 count)
{
	if (id < 0) { return; }

	switch (type)
	{
	case SVT_SCALAR:
		glUniform1fv(id, count, data);
		break;
	case SVT_VEC2:
		glUniform2fv(id, count, data);
		break;
	case SVT_VEC3:
		glUniform3fv(id, count, data);
		break;
	case SVT_VEC4:
		glUniform4fv(id, count, data);
		break;
	case SVT_MAT3x3:
		glUniformMatrix3fv(id, count, false, data);
		break;
	case SVT_MAT4x3:
		tfe_UniformMatrix4x3fv(id, count, false, data);
		break;
	case SVT_MAT4x4:
		glUniformMatrix4fv(id, count, false, data);
		break;
	default:
		TFE_System::logWrite(LOG_ERROR, "Shader", "Mismatched parameter type.");
		assert(0);
	}
}

void Shader::setVariable(s32 id, ShaderVariableType type, const s32* data)
{
	if (id < 0) { return; }

	switch (type)
	{
	case SVT_ISCALAR:
		glUniform1i(id, *(&data[0]));
		break;
	case SVT_IVEC2:
		glUniform2iv(id, 1, data);
		break;
	case SVT_IVEC3:
		glUniform3iv(id, 1, data);
		break;
	case SVT_IVEC4:
		glUniform4iv(id, 1, data);
		break;
	default:
		TFE_System::logWrite(LOG_ERROR, "Shader", "Mismatched parameter type.");
		assert(0);
	}
}

void Shader::setVariable(s32 id, ShaderVariableType type, const u32* data)
{
	if (id < 0) { return; }

	switch (type)
	{
	case SVT_USCALAR:
		glUniform1ui(id, *(&data[0]));
		break;
	case SVT_UVEC2:
		glUniform2uiv(id, 1, data);
		break;
	case SVT_UVEC3:
		glUniform3uiv(id, 1, data);
		break;
	case SVT_UVEC4:
		glUniform4uiv(id, 1, data);
		break;
	default:
		TFE_System::logWrite(LOG_ERROR, "Shader", "Mismatched parameter type.");
		assert(0);
	}
}
