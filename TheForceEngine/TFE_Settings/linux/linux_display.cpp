#include "linux_display.h"

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__linux__)
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/kd.h>
#include <linux/vt.h>
#endif

static int tfe_linux_kms_display;

#if defined(__linux__)
static int tfe_linux_is_bare_vt()
{
	const char* display = std::getenv("DISPLAY");
	const char* wayland = std::getenv("WAYLAND_DISPLAY");

	if ((display && display[0]) || (wayland && wayland[0]))
		return 0;

	const char* tty = ttyname(STDERR_FILENO);
	if (!tty)
		tty = ttyname(STDOUT_FILENO);
	if (!tty || std::strncmp(tty, "/dev/tty", 8) != 0)
		return 0;
	for (const char* n = tty + 8; *n; ++n)
	{
		if (*n < '0' || *n > '9')
			return 0;
	}
	return 1;
}

static int tfe_linux_drm_usable()
{
	return access("/dev/dri/card0", R_OK | W_OK) == 0
		|| access("/dev/dri/card1", R_OK | W_OK) == 0;
}
#endif

void tfe_InitLinuxDisplayEnv()
{
#if defined(__linux__)
	const char* video_env = std::getenv("SDL_VIDEODRIVER");
	tfe_linux_kms_display = 0;

	if (video_env && video_env[0])
		return;

	if (!tfe_linux_is_bare_vt())
		return;

	tfe_linux_kms_display = 1;
	SDL_SetHint(SDL_HINT_VIDEODRIVER, "kmsdrm");

	if (!tfe_linux_drm_usable())
	{
		std::fprintf(stderr,
			"TFE: Linux VT without X11/Wayland; /dev/dri/card* is not accessible.\n"
			"Add your user to the video group, then log in again.\n");
		return;
	}

	std::fprintf(stderr, "TFE: Linux VT detected — using kmsdrm + GLES\n");
#else
	tfe_linux_kms_display = 0;
#endif
}

int tfe_IsLinuxKmsDisplay()
{
	return tfe_linux_kms_display;
}

void tfe_RestoreLinuxConsole()
{
#if defined(__linux__)
	if (!tfe_linux_kms_display)
		return;

	struct vt_mode vt;
	struct vt_stat state;
	int fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return;

	ioctl(fd, KDSETMODE, KD_TEXT);

	std::memset(&vt, 0, sizeof(vt));
	vt.mode = VT_AUTO;
	ioctl(fd, VT_SETMODE, &vt);

	if (ioctl(fd, VT_GETSTATE, &state) == 0)
		ioctl(fd, VT_ACTIVATE, state.v_active);

	close(fd);
	std::fputs("\033c", stderr);
	std::fflush(stderr);
#endif
}
