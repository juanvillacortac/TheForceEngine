#ifndef TFE_BUFFER_ACCESS_INCLUDED
#define TFE_BUFFER_ACCESS_INCLUDED

// Desktop GL: samplerBuffer / isamplerBuffer / usamplerBuffer (unchanged).
// GLES Mali fallback (TFE_BUFFER_TEXTURE_2D): buffer data in a 2D texture.
// Integer and float buffers both use GL_RGBA8 byte layout (TFE_BUFFER_TEXTURE_BYTES)
// on Mali GLES — GL_*32* and float/half 2D allocations can SIGSEGV the driver.

#ifdef TFE_BUFFER_TEXTURE_2D

// Mali GLES requires explicit precision on sampler parameters (global precision does not apply).
#define TFE_DECLARE_FBUFFER(name) uniform highp sampler2D name
#define TFE_DECLARE_IBUFFER(name) uniform highp sampler2D name
#define TFE_DECLARE_UBUFFER(name) uniform highp sampler2D name

ivec2 tfe_bufCoord(int index)
{
	int x = index - (index / TFE_SHADER_BUFFER_WIDTH) * TFE_SHADER_BUFFER_WIDTH;
	return ivec2(x, index / TFE_SHADER_BUFFER_WIDTH);
}

#ifdef TFE_BUFFER_TEXTURE_BYTES

int tfe_loadInt32(highp sampler2D buf, int byteOff)
{
	int texIdx = byteOff >> 2;
	vec4 t = texelFetch(buf, tfe_bufCoord(texIdx), 0);
	int b0 = int(t.r * 255.0 + 0.5);
	int b1 = int(t.g * 255.0 + 0.5);
	int b2 = int(t.b * 255.0 + 0.5);
	int b3 = int(t.a * 255.0 + 0.5);
	return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

uint tfe_loadUint32(highp sampler2D buf, int byteOff)
{
	return uint(tfe_loadInt32(buf, byteOff));
}

float tfe_loadFloat32(highp sampler2D buf, int byteOff)
{
	return uintBitsToFloat(tfe_loadUint32(buf, byteOff));
}

vec4 tfe_fetchFBuffer(highp sampler2D buf, int index)
{
	int b = index << 4;
	return vec4(
		tfe_loadFloat32(buf, b),
		tfe_loadFloat32(buf, b + 4),
		tfe_loadFloat32(buf, b + 8),
		tfe_loadFloat32(buf, b + 12));
}

ivec4 tfe_fetchIBuffer16(highp sampler2D buf, int index)
{
	int b = index << 4;
	return ivec4(
		tfe_loadInt32(buf, b),
		tfe_loadInt32(buf, b + 4),
		tfe_loadInt32(buf, b + 8),
		tfe_loadInt32(buf, b + 12));
}

ivec4 tfe_fetchIBuffer8(highp sampler2D buf, int index)
{
	int b = index << 3;
	return ivec4(
		tfe_loadInt32(buf, b),
		tfe_loadInt32(buf, b + 4),
		0,
		0);
}

#define tfe_fetchIBuffer tfe_fetchIBuffer16

uvec4 tfe_fetchUBuffer(highp sampler2D buf, int index)
{
	return uvec4(tfe_fetchIBuffer16(buf, index));
}

#else

vec4 tfe_fetchFBuffer(highp sampler2D buf, int index)
{
	return texelFetch(buf, tfe_bufCoord(index), 0);
}

ivec4 tfe_fetchIBuffer(highp usampler2D buf, int index)
{
	return ivec4(texelFetch(buf, tfe_bufCoord(index), 0));
}

uvec4 tfe_fetchUBuffer(highp usampler2D buf, int index)
{
	return texelFetch(buf, tfe_bufCoord(index), 0);
}

#undef TFE_DECLARE_IBUFFER
#undef TFE_DECLARE_UBUFFER
#define TFE_DECLARE_IBUFFER(name) uniform highp usampler2D name
#define TFE_DECLARE_UBUFFER(name) uniform highp usampler2D name

#endif

#else

vec4 tfe_fetchFBuffer(samplerBuffer buf, int index)
{
	return texelFetch(buf, index);
}

ivec4 tfe_fetchIBuffer(isamplerBuffer buf, int index)
{
	return texelFetch(buf, index);
}

uvec4 tfe_fetchUBuffer(usamplerBuffer buf, int index)
{
	return texelFetch(buf, index);
}

#define TFE_DECLARE_FBUFFER(name) uniform samplerBuffer name
#define TFE_DECLARE_IBUFFER(name) uniform isamplerBuffer name
#define TFE_DECLARE_UBUFFER(name) uniform usamplerBuffer name

#endif

#endif
