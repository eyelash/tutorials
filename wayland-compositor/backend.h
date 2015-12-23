#include <EGL/egl.h>

struct callbacks {
	void (*resize) (int width, int height);
	void (*draw) (void);
	void (*mouse_motion) (int x, int y);
	void (*mouse_button) (int button, int state);
	void (*key) (int key, int state);
	void (*modifiers) (int depressed, int latched, int locked, int group);
};

void backend_init (struct callbacks *callbacks);
EGLDisplay backend_get_egl_display (void);
void backend_swap_buffers (void);
int backend_get_fd (void);
void backend_dispatch_nonblocking (void);
void backend_request_redraw (void);
void backend_get_keymap (int *fd, int *size);
long backend_get_timestamp (void);
