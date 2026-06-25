#include <cstring>

#include "menu.h"
#include "delt.h"
#include "uiDraw.h"
#include <TFE_DarkForces/agent.h>
#include <TFE_DarkForces/util.h>
#include <TFE_DarkForces/Landru/lcanvas.h>
#include <TFE_DarkForces/Landru/ldraw.h>
#include <TFE_Archive/archive.h>
#include <TFE_Archive/lfdArchive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/screenDraw.h>
#include <TFE_Input/replay.h>
#include <cstdlib>
#include <cmath>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
#define MENU_CONV_6bitTo8bit(x) (((x)<<2) | ((x)>>4))

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static LfdArchive s_archive;

	Vec2i s_cursorPosAccum;
	Vec2i s_cursorPos;
	s32 s_buttonPressed = -1;
	JBool s_buttonHover = JFALSE;
	static JBool s_gamepadClickPressed = JFALSE;
	static JBool s_gamepadClickDown = JFALSE;
	static f32 s_navStickX = 0.0f;
	static f32 s_navStickY = 0.0f;
	static f32 s_prevNavStickX = 0.0f;
	static f32 s_prevNavStickY = 0.0f;
	static const f32 NAV_STICK_THRESHOLD = 0.5f;

	static JBool navStickCrossed(f32 value, f32 prev, JBool positive)
	{
		if (positive)
		{
			return value >= NAV_STICK_THRESHOLD && prev < NAV_STICK_THRESHOLD;
		}
		return value <= -NAV_STICK_THRESHOLD && prev > -NAV_STICK_THRESHOLD;
	}

	static JBool navStickHeld(f32 value, JBool positive)
	{
		return positive ? value >= NAV_STICK_THRESHOLD : value <= -NAV_STICK_THRESHOLD;
	}
		
	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	JBool menu_isHandheld()
	{
		const char* env = std::getenv("TFE_HANDHELD");
		return env && env[0] == '1';
	}

	JBool menu_shouldDrawCursor()
	{
		return !menu_isHandheld();
	}

	JBool menu_handheldActivatePressed()
	{
		return menu_isHandheld() && TFE_Input::buttonPressed(CONTROLLER_BUTTON_A) ? JTRUE : JFALSE;
	}

	JBool menu_handheldCancelPressed()
	{
		return menu_isHandheld() && TFE_Input::buttonPressed(CONTROLLER_BUTTON_B) ? JTRUE : JFALSE;
	}

	void menu_updateGamepadPointerState()
	{
		if (!menu_isHandheld())
		{
			s_gamepadClickPressed = JFALSE;
			s_gamepadClickDown = JFALSE;
			return;
		}
		s_gamepadClickPressed = TFE_Input::buttonPressed(CONTROLLER_BUTTON_A) ? JTRUE : JFALSE;
		s_gamepadClickDown = TFE_Input::buttonDown(CONTROLLER_BUTTON_A) ? JTRUE : JFALSE;
	}

	JBool menu_pointerPressed()
	{
		if (menu_isHandheld())
		{
			return JFALSE;
		}
		return TFE_Input::mousePressed(MBUTTON_LEFT) || s_gamepadClickPressed;
	}

	JBool menu_pointerDown()
	{
		if (menu_isHandheld())
		{
			return JFALSE;
		}
		return TFE_Input::mouseDown(MBUTTON_LEFT) || s_gamepadClickDown;
	}

	void menu_handheldNavNewFrame()
	{
		if (!menu_isHandheld())
		{
			return;
		}

		s_prevNavStickX = s_navStickX;
		s_prevNavStickY = s_navStickY;
		s_navStickX = TFE_Input::getAxis(AXIS_LEFT_X);
		s_navStickY = TFE_Input::getAxis(AXIS_LEFT_Y);
	}

	JBool menu_handheldNavDown(MenuHandheldNav dir)
	{
		if (!menu_isHandheld())
		{
			return JFALSE;
		}

		switch (dir)
		{
		case MENU_NAV_UP:
			return TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_UP) || navStickHeld(s_navStickY, false) ? JTRUE : JFALSE;
		case MENU_NAV_DOWN:
			return TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_DOWN) || navStickHeld(s_navStickY, true) ? JTRUE : JFALSE;
		case MENU_NAV_LEFT:
			return TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_LEFT) || navStickHeld(s_navStickX, false) ? JTRUE : JFALSE;
		case MENU_NAV_RIGHT:
			return TFE_Input::buttonDown(CONTROLLER_BUTTON_DPAD_RIGHT) || navStickHeld(s_navStickX, true) ? JTRUE : JFALSE;
		default:
			return JFALSE;
		}
	}

	JBool menu_handheldNavPressed(MenuHandheldNav dir)
	{
		if (!menu_isHandheld())
		{
			return JFALSE;
		}

		switch (dir)
		{
		case MENU_NAV_UP:
			return TFE_Input::buttonPressed(CONTROLLER_BUTTON_DPAD_UP) || navStickCrossed(s_navStickY, s_prevNavStickY, false) ? JTRUE : JFALSE;
		case MENU_NAV_DOWN:
			return TFE_Input::buttonPressed(CONTROLLER_BUTTON_DPAD_DOWN) || navStickCrossed(s_navStickY, s_prevNavStickY, true) ? JTRUE : JFALSE;
		case MENU_NAV_LEFT:
			return TFE_Input::buttonPressed(CONTROLLER_BUTTON_DPAD_LEFT) || navStickCrossed(s_navStickX, s_prevNavStickX, false) ? JTRUE : JFALSE;
		case MENU_NAV_RIGHT:
			return TFE_Input::buttonPressed(CONTROLLER_BUTTON_DPAD_RIGHT) || navStickCrossed(s_navStickX, s_prevNavStickX, true) ? JTRUE : JFALSE;
		default:
			return JFALSE;
		}
	}

	void menu_moveHandheldCursor(Vec2i* cursorPos, s32 width, s32 height)
	{
		if (!menu_isHandheld() || !cursorPos)
		{
			return;
		}

		menu_updateGamepadPointerState();

		const s32 speed = 5;
		const f32 ax = TFE_Input::getAxis(AXIS_RIGHT_X);
		const f32 ay = TFE_Input::getAxis(AXIS_RIGHT_Y);
		if (std::fabs(ax) > 0.2f)
		{
			cursorPos->x += s32(ax * f32(speed) * 2.0f);
		}
		if (std::fabs(ay) > 0.2f)
		{
			cursorPos->z -= s32(ay * f32(speed) * 2.0f);
		}

		cursorPos->x = clamp(cursorPos->x, 0, width - 3);
		cursorPos->z = clamp(cursorPos->z, 0, height - 3);
	}
	void menu_init()
	{
	}

	void menu_destroy()
	{
		menu_resetState();
	}

	void menu_resetState()
	{
		delt_resetState();
	}
	
	void menu_handleMousePosition()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		LRect bounds;
		lcanvas_getBounds(&bounds);
		s32 width  = bounds.right  - bounds.left;
		s32 height = bounds.bottom - bounds.top;

		menu_updateGamepadPointerState();

		// Handheld has no mouse; keep the virtual cursor position across frames.
		if (menu_isHandheld())
		{
			menu_handheldNavNewFrame();
			if (TFE_Input::isDemoPlayback())
			{
				s_cursorPos = TFE_Input::getPDAPosition();
			}
			return;
		}

		s32 mx, my;
		TFE_Input::getMousePos(&mx, &my);
		s_cursorPosAccum = { 12*mx/10, my };	// Account for 320x200 in 4:3 scaling.
		
		// Load the replay PDA positions. 
		if (TFE_Input::isDemoPlayback())
		{
			Vec2i pdap = TFE_Input::getPDAPosition();
			s_cursorPos = pdap;
		}
		else
		{
			if (displayInfo.width >= displayInfo.height)
			{
				s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
				s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
			}
			else
			{
				s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)width / (s32)displayInfo.width, 0, (s32)width - 3);
				s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)width / (s32)displayInfo.width, 0, (s32)height - 3);
			}
		}

		// Store the PDA positions for replay.
		if (TFE_Input::isRecording())
		{
			TFE_Input::storePDAPosition(s_cursorPos);
		}
	}

	void menu_resetCursor()
	{
		// Reset the cursor.
		u32 width, height;
		vfb_getResolution(&width, &height);

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		s_cursorPosAccum = { (s32)displayInfo.width >> 1, (s32)displayInfo.height >> 1 };
		s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
		s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
	}

	void menu_setPaletteFromPltt(const u8* pal768)
	{
		u32 palette[256];
		u32* outColor = palette;
		const u8* srcColor = pal768;
		for (s32 i = 0; i < 256; i++, outColor++, srcColor += 3)
		{
			*outColor = MENU_CONV_6bitTo8bit(srcColor[0]) | (MENU_CONV_6bitTo8bit(srcColor[1]) << 8u) | (MENU_CONV_6bitTo8bit(srcColor[2]) << 16u) | (0xffu << 24u);
		}
		// Match lpalette_setVgaPalette — double upload avoids GLES palette/framebuffer desync.
		vfb_setPalette(palette);
		vfb_setPalette(palette);
		TFE_Jedi::renderer_setSourcePalette(palette);
	}

	u8* menu_startupDisplay()
	{
		vfb_setMode(VFB_TEXTURE);
		vfb_setResolution(320, 200);
		return vfb_getCpuBuffer();
	}

	void menu_blitCursor(s32 x, s32 y, u8* framebuffer)
	{
		if (!menu_shouldDrawCursor())
		{
			return;
		}
		blitDeltaFrame(&s_cursor, x, y, framebuffer);
	}

	JBool menu_openResourceArchive(const char* name)
	{
		FilePath lfdPath;
		if (!TFE_Paths::getFilePath(name, &lfdPath))
		{
			return JFALSE;
		}
		// Load the mission briefing text.
		if (!s_archive.open(lfdPath.path))
		{
			return JFALSE;
		}
		TFE_Paths::addLocalArchive(&s_archive);
		return JTRUE;
	}

	void menu_closeResourceArchive()
	{
		s_archive.close();
		TFE_Paths::removeLastArchive();
	}

	void menu_blitToScreen(u8* framebuffer/*=nullptr*/, JBool transparent/*=JFALSE*/, JBool swap/*=JTRUE*/)
	{
		u32 outWidth, outHeight;
		vfb_getResolution(&outWidth, &outHeight);

		// If there is no override, the default behavior is to use the Landru bitmap.
		if (!framebuffer)
		{
			framebuffer = ldraw_getBitmap();
		}

		if (outWidth == 320 && outHeight == 200)
		{
			if (transparent)
			{
				TFE_Jedi::ScreenImage canvas =
				{
					320,
					200,
					framebuffer,
					transparent,
					JFALSE,
				};
				ScreenRect* uiRect = vfb_getScreenRect(VFB_RECT_UI);
				blitTextureToScreen(&canvas, (DrawRect*)uiRect, 0, 0, vfb_getCpuBuffer());
			}
			else
			{
				// This is a straight copy - best for performance since the GPU can do the upscale.
				memcpy(vfb_getCpuBuffer(), framebuffer, 320 * 200);
			}
		}
		else
		{
			// This version requires a software upscale, this is handy when some parts need to be higher resolution or
			// to avoid switching virtual framebuffers during play.
			TFE_Jedi::ScreenImage canvas =
			{
				320,
				200,
				framebuffer,
				transparent,
				JFALSE,
			};
			ScreenRect* uiRect = vfb_getScreenRect(VFB_RECT_UI);
			fixed16_16 xScale = vfb_getXScale();
			fixed16_16 yScale = vfb_getYScale();

			s32 virtualWidth = floor16(mul16(intToFixed16(320), xScale));
			s32 offset = max(0, ((uiRect->right - uiRect->left + 1) - virtualWidth) / 2);
			
			if (!transparent)
			{
				memset(vfb_getCpuBuffer(), 0, outWidth * outHeight);
			}
			blitTextureToScreenScaled(&canvas, (DrawRect*)uiRect, offset, 0, xScale, yScale, vfb_getCpuBuffer());
		}
		if (swap) { vfb_swap(); }
	}

	void menu_blitCursorScaled(s16 x, s16 y, u8* buffer)
	{
		if (!menu_shouldDrawCursor())
		{
			return;
		}
		ScreenRect* uiRect = vfb_getScreenRect(VFB_RECT_UI);
		fixed16_16 xScale = vfb_getXScale();
		fixed16_16 yScale = vfb_getYScale();

		s32 virtualWidth = floor16(mul16(intToFixed16(320), xScale));
		s32 offset = max(0, ((uiRect->right - uiRect->left + 1) - virtualWidth) / 2);

		x = floor16(mul16(intToFixed16(x), xScale)) + offset;
		y = floor16(mul16(intToFixed16(y), yScale));

		blitDeltaFrameScaled(&s_cursor, x, y, xScale, yScale, buffer);
	}
}