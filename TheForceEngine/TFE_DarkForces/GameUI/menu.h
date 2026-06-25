#pragma once
//////////////////////////////////////////////////////////////////////
// General Dark Forces shared menu code.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_DarkForces/Landru/lrect.h>

namespace TFE_DarkForces
{
	enum MenuHandheldNav
	{
		MENU_NAV_UP = 0,
		MENU_NAV_DOWN,
		MENU_NAV_LEFT,
		MENU_NAV_RIGHT,
	};

	void menu_init();
	void menu_destroy();
	void menu_resetState();

	void menu_handleMousePosition();
	void menu_updateGamepadPointerState();
	void menu_moveHandheldCursor(Vec2i* cursorPos, s32 width, s32 height);
	void menu_handheldNavNewFrame();
	JBool menu_handheldNavDown(MenuHandheldNav dir);
	JBool menu_handheldNavPressed(MenuHandheldNav dir);
	JBool  menu_isHandheld();
	JBool  menu_shouldDrawCursor();
	JBool  menu_handheldActivatePressed();
	JBool  menu_handheldCancelPressed();
	JBool  menu_pointerPressed();
	JBool  menu_pointerDown();
	void menu_blitCursor(s32 x, s32 y, u8* framebuffer);
	void menu_resetCursor();
	u8*  menu_startupDisplay();
	// Upload a 768-byte VGA palette (pltt format) to the GPU, matching Landru lpalette_setVgaPalette.
	void menu_setPaletteFromPltt(const u8* pal768);

	JBool menu_openResourceArchive(const char* name);
	void  menu_closeResourceArchive();

	// 'framebuffer' can be passed in to override the default source bitmap.
	// If null, the Landru bitmap will be used.
	void menu_blitToScreen(u8* framebuffer = nullptr, JBool transparent = JFALSE, JBool swap = JTRUE);
	void menu_blitCursorScaled(s16 x, s16 y, u8* buffer);
	   
	// Variables.
	extern Vec2i s_cursorPos;
	extern s32 s_buttonPressed;
	extern JBool s_buttonHover;
}
