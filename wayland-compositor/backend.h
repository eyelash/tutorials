#include <EGL/egl.h>

struct callbacks {
	void (*resize) (int width, int height);
	void (*draw) (void);
	void (*mouse_event) (int x, int y, int type);
	void (*keyboard_event) (int key);
};

void backend_init (struct callbacks *callbacks);
EGLDisplay backend_get_egl_display (void);
void backend_swap_buffers (void);
int backend_get_fd (void);
void backend_dispatch_nonblocking (void);
void backend_request_redraw (void);
long backend_get_timestamp (void);
