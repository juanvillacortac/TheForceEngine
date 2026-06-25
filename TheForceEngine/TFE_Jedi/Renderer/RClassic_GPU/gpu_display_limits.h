#pragma once
//////////////////////////////////////////////////////////////////////
// GPU display-list capacity — desktop vs Mali handheld.
// Full 65536-item lists allocate ~256MB for portal frusta alone; RG34XX
// cannot sustain that and SIGSEGVs inside sdisplayList_init().
//////////////////////////////////////////////////////////////////////

#if defined(TFE_RUNTIME_GL) && !defined(TFE_RUNTIME_GL_DESKTOP)
#define TFE_GPU_MAX_DISP_ITEMS 8192u
#define TFE_GPU_MAX_BUFFER_SIZE 8192u
#else
#define TFE_GPU_MAX_DISP_ITEMS 65536u
#define TFE_GPU_MAX_BUFFER_SIZE 65536u
#endif
