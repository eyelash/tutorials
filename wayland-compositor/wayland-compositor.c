// gcc -o wayland-compositor wayland-compositor.c backend-x11.c xdg-shell.c -lwayland-server -lX11 -lEGL -lGL

#include <wayland-server.h>
#include "xdg-shell.h"
#include <linux/input.h>
#include <stdlib.h>
#include "backend.h"
#include <GL/gl.h>
#include "texture.h"
#include <stdio.h>

static struct wl_display *display;

struct surface {
	struct wl_resource *surface;
	struct wl_resource *xdg_surface;
	struct wl_resource *buffer;
	struct wl_resource *frame_callback;
	int x, y;
	struct texture texture;
	struct wl_resource *pointer;
	struct wl_list link;
};
static struct wl_list surfaces;
static struct surface *cursor = NULL;
static struct surface *moving_surface = NULL;
static struct surface *active_surface = NULL;
static struct surface *pointer_surface = NULL; // surface under the pointer
static struct wl_resource *pointer = NULL;

static int pointer_x, pointer_y;
struct wl_array pointers;

static struct wl_resource *surface_get_pointer (struct surface *surface) {
	struct wl_client *client = wl_resource_get_client (surface->surface);
	struct wl_resource **pointer;
	wl_array_for_each (pointer, &pointers) {
		if (wl_resource_get_client(*pointer) == client) return *pointer;
	}
	return NULL;
}
static void deactivate_surface (struct surface *surface) {
	struct wl_array state_array;
	wl_array_init (&state_array);
	xdg_surface_send_configure (surface->xdg_surface, 0, 0, &state_array, 0);
}
static void activate_surface (struct surface *surface) {
	wl_list_remove (&surface->link);
	wl_list_insert (&surfaces, &surface->link);
	struct wl_array state_array;
	wl_array_init (&state_array);
	int32_t *states = wl_array_add (&state_array, sizeof(int32_t));
	states[0] = XDG_SURFACE_STATE_ACTIVATED;
	xdg_surface_send_configure (surface->xdg_surface, 0, 0, &state_array, 0);
	active_surface = surface;
}

static void surface_delete (struct wl_resource *resource) {
	struct surface *surface = wl_resource_get_user_data (resource);
	wl_list_remove (&surface->link);
	if (surface == active_surface) active_surface = NULL;
	if (surface == pointer_surface) {
		pointer_surface = NULL;
		pointer = NULL;
	}
	backend_request_redraw ();
}

// surface
static void surface_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void surface_attach (struct wl_client *client, struct wl_resource *resource, struct wl_resource *buffer, int32_t x, int32_t y) {
	struct surface *surface = wl_resource_get_user_data (resource);
	surface->buffer = buffer;
}
static void surface_damage (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static void surface_frame (struct wl_client *client, struct wl_resource *resource, uint32_t callback) {
	struct surface *surface = wl_resource_get_user_data (resource);
	surface->frame_callback = wl_resource_create (client, &wl_callback_interface, 1, callback);
}
static void surface_set_opaque_region (struct wl_client *client, struct wl_resource *resource, struct wl_resource *region) {
	
}
static void surface_set_input_region (struct wl_client *client, struct wl_resource *resource, struct wl_resource *region) {
	
}
static void surface_commit (struct wl_client *client, struct wl_resource *resource) {
	struct surface *surface = wl_resource_get_user_data (resource);
	struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (surface->buffer);
	uint32_t width = wl_shm_buffer_get_width (shm_buffer);
	uint32_t height = wl_shm_buffer_get_height (shm_buffer);
	void *data = wl_shm_buffer_get_data (shm_buffer);
	texture_delete (&surface->texture);
	texture_create (&surface->texture, width, height, data);
	wl_buffer_send_release (surface->buffer);
	backend_request_redraw ();
	if (surface->frame_callback) {
		wl_callback_send_done (surface->frame_callback, backend_get_timestamp());
		surface->frame_callback = NULL;
	}
}
static void surface_set_buffer_transform (struct wl_client *client, struct wl_resource *resource, int32_t transform) {
	
}
static void surface_set_buffer_scale (struct wl_client *client, struct wl_resource *resource, int32_t scale) {
	
}
static struct wl_surface_interface surface_interface = {&surface_destroy, &surface_attach, &surface_damage, &surface_frame, &surface_set_opaque_region, &surface_set_input_region, &surface_commit, &surface_set_buffer_transform, &surface_set_buffer_scale};

// region
static void region_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void region_add (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static void region_subtract (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static struct wl_region_interface region_interface = {&region_destroy, &region_add, &region_subtract};

// compositor
static void compositor_create_surface (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct surface *surface = calloc (1, sizeof(struct surface));
	surface->surface = wl_resource_create (client, &wl_surface_interface, 3, id);
	wl_resource_set_implementation (surface->surface, &surface_interface, surface, &surface_delete);
	wl_list_insert (&surfaces, &surface->link);
}
static void compositor_create_region (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *region = wl_resource_create (client, &wl_region_interface, 1, id);
	wl_resource_set_implementation (region, &region_interface, NULL, NULL);
}
static struct wl_compositor_interface compositor_interface = {&compositor_create_surface, &compositor_create_region};
static void compositor_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: compositor\n");
	struct wl_resource *resource = wl_resource_create (client, &wl_compositor_interface, 1, id);
	wl_resource_set_implementation (resource, &compositor_interface, NULL, NULL);
}

// shell surface
static void shell_surface_pong (struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
	
}
static void shell_surface_move (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial) {
	
}
static void shell_surface_resize (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, uint32_t edges) {
	
}	       
static void shell_surface_set_toplevel (struct wl_client *client, struct wl_resource *resource) {
	
}
static void shell_surface_set_transient (struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent, int32_t x, int32_t y, uint32_t flags) {
	
}
static void shell_surface_set_fullscreen (struct wl_client *client, struct wl_resource *resource, uint32_t method, uint32_t framerate, struct wl_resource *output) {
	
}
static void shell_surface_set_popup (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, struct wl_resource *parent, int32_t x, int32_t y, uint32_t flags) {
	
}
static void shell_surface_set_maximized (struct wl_client *client, struct wl_resource *resource, struct wl_resource *output) {
	
}
static void shell_surface_set_title (struct wl_client *client, struct wl_resource *resource, const char *title) {
	
}
static void shell_surface_set_class (struct wl_client *client, struct wl_resource *resource, const char *class_) {
	
}
static struct wl_shell_surface_interface shell_surface_interface = {&shell_surface_pong, &shell_surface_move, &shell_surface_resize, &shell_surface_set_toplevel, &shell_surface_set_transient, &shell_surface_set_fullscreen, &shell_surface_set_popup, &shell_surface_set_maximized, &shell_surface_set_title, &shell_surface_set_class};

// wl shell
static void shell_get_shell_surface (struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface) {
	struct wl_resource *shell_surface = wl_resource_create (client, &wl_shell_surface_interface, 1, id);
	wl_resource_set_implementation (shell_surface, &shell_surface_interface, NULL, NULL);
}
static struct wl_shell_interface shell_interface = {&shell_get_shell_surface};
static void shell_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: wl_shell\n");
	struct wl_resource *resource = wl_resource_create (client, &wl_shell_interface, 1, id);
	wl_resource_set_implementation (resource, &shell_interface, NULL, NULL);
}

// xdg surface
static void xdg_surface_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_surface_set_parent (struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent) {
	
}
static void xdg_surface_set_title (struct wl_client *client, struct wl_resource *resource, const char *title) {
	
}
static void xdg_surface_set_app_id (struct wl_client *client, struct wl_resource *resource, const char *app_id) {
	
}
static void xdg_surface_show_window_menu (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {
	
}
static void xdg_surface_move (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial) {
	struct surface *surface = wl_resource_get_user_data (resource);
	// during the move the surface coordinates are relative to the pointer
	surface->x = surface->x - pointer_x;
	surface->y = surface->y - pointer_y;
	moving_surface = surface;
}
static void xdg_surface_resize (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, uint32_t edges) {
	
}
static void xdg_surface_ack_configure (struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
	
}
static void xdg_surface_set_window_geometry (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static void xdg_surface_set_maximized (struct wl_client *client, struct wl_resource *resource) {
	printf ("surface requested maximize\n");
}
static void xdg_surface_unset_maximized (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_surface_set_fullscreen (struct wl_client *client, struct wl_resource *resource, struct wl_resource *output) {
	
}
static void xdg_surface_unset_fullscreen (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_surface_set_minimized (struct wl_client *client, struct wl_resource *resource) {
	
}
static struct xdg_surface_interface my_xdg_surface_interface = {xdg_surface_destroy, xdg_surface_set_parent, xdg_surface_set_title, xdg_surface_set_app_id, xdg_surface_show_window_menu, xdg_surface_move, xdg_surface_resize, xdg_surface_ack_configure, xdg_surface_set_window_geometry, xdg_surface_set_maximized, xdg_surface_unset_maximized, xdg_surface_set_fullscreen, xdg_surface_unset_fullscreen, xdg_surface_set_minimized};

// xdg shell
static void xdg_shell_use_unstable_version (struct wl_client *client, struct wl_resource *resource, int32_t version) {
	
}
static void xdg_shell_get_xdg_surface (struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *_surface) {
	struct surface *surface = wl_resource_get_user_data (_surface);
	surface->xdg_surface = wl_resource_create (client, &xdg_surface_interface, 1, id);
	wl_resource_set_implementation (surface->xdg_surface, &my_xdg_surface_interface, surface, NULL);
}
static void xdg_shell_get_xdg_popup (struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface, struct wl_resource *parent, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y, uint32_t flags) {
	
}
static void xdg_shell_pong (struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
	
}
static struct xdg_shell_interface my_xdg_shell_interface = {&xdg_shell_use_unstable_version, xdg_shell_get_xdg_surface, &xdg_shell_get_xdg_popup, &xdg_shell_pong};
static void xdg_shell_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: xdg_shell\n");
	struct wl_resource *resource = wl_resource_create (client, &xdg_shell_interface, 1, id);
	wl_resource_set_implementation (resource, &my_xdg_shell_interface, NULL, NULL);
}

// pointer
static void pointer_set_cursor (struct wl_client *client, struct wl_resource *resource, uint32_t serial, struct wl_resource *_surface, int32_t hotspot_x,
int32_t hotspot_y) {
	struct surface *surface = wl_resource_get_user_data (_surface);
	cursor = surface;
}
static void pointer_release (struct wl_client *client, struct wl_resource *resource) {
	
}
static struct wl_pointer_interface pointer_interface = {&pointer_set_cursor, &pointer_release};

// keyboard
static void keyboard_release (struct wl_client *client, struct wl_resource *resource) {
	
}
static struct wl_keyboard_interface keyboard_interface = {&keyboard_release};

// seat
static void seat_get_pointer (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource **pointer = wl_array_add (&pointers, sizeof(struct wl_resource*));
	*pointer = wl_resource_create (client, &wl_pointer_interface, 1, id);
	wl_resource_set_implementation (*pointer, &pointer_interface, NULL, NULL);
}
static void seat_get_keyboard (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}
static void seat_get_touch (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}
static struct wl_seat_interface seat_interface = {&seat_get_pointer, &seat_get_keyboard, &seat_get_touch};
static void seat_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: seat\n");
	struct wl_resource *seat = wl_resource_create (client, &wl_seat_interface, 1, id);
	wl_resource_set_implementation (seat, &seat_interface, NULL, NULL);
	wl_seat_send_capabilities (seat, WL_SEAT_CAPABILITY_POINTER);
}

// backend callbacks
static void resize (int width, int height) {
	glViewport (0, 0, width, height);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0, width, height, 0, 1, -1);
	glMatrixMode (GL_MODELVIEW);
	
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
}
static void draw (void) {
	glClearColor (0, 1, 0, 1);
	glClear (GL_COLOR_BUFFER_BIT);
	glLoadIdentity();
	
	struct surface *surface;
	wl_list_for_each_reverse (surface, &surfaces, link) {
		if (!surface->xdg_surface) continue;
		if (surface == moving_surface)
			texture_draw (&surface->texture, pointer_x + surface->x, pointer_y + surface->y);
		else
			texture_draw (&surface->texture, surface->x, surface->y);
	}
	// draw the cursor last
	if (cursor) texture_draw (&cursor->texture, pointer_x, pointer_y);
	
	glFlush ();
	backend_swap_buffers ();
}
static void mouse_event (int x, int y, int type) {
	if (type == 0) {
		pointer_x = x;
		pointer_y = y;
		if (cursor) backend_request_redraw ();
	}
	// special case: moving surface
	if (moving_surface) {
		if (type == 2) {
			moving_surface->x = x + moving_surface->x;
			moving_surface->y = y + moving_surface->y;
			moving_surface = NULL;
		}
		backend_request_redraw ();
		return;
	}
	long timestamp = backend_get_timestamp ();
	// get surface under the pointer
	struct surface *next_pointer_surface = NULL;
	struct surface *s;
	wl_list_for_each_reverse (s, &surfaces, link) {
		if (!s->xdg_surface) continue;
		if (x > s->x && y > s->y && x < s->x + s->texture.width && y < s->y + s->texture.height)
			next_pointer_surface = s;
	}
	// pointer enter and leave
	if (next_pointer_surface != pointer_surface) {
		if (pointer) wl_pointer_send_leave (pointer, 0, pointer_surface->surface);
		pointer = NULL;
		pointer_surface = next_pointer_surface;
		if (pointer_surface) {
			pointer = surface_get_pointer (pointer_surface);
			if (pointer) wl_pointer_send_enter (pointer, 0, pointer_surface->surface, x, y);
		}
	}
	if (!pointer) return;
	wl_fixed_t surface_x = wl_fixed_from_double (x - pointer_surface->x);
	wl_fixed_t surface_y = wl_fixed_from_double (y - pointer_surface->y);
	if (type == 0) {
		wl_pointer_send_motion (pointer, timestamp, surface_x, surface_y);
	}
	else if (type == 1) {
		if (pointer_surface != active_surface) {
			if (active_surface) deactivate_surface (active_surface);
			activate_surface (pointer_surface);
		}
		wl_pointer_send_button (pointer, 0, timestamp, BTN_LEFT, WL_POINTER_BUTTON_STATE_PRESSED);
	}
	else if (type == 2) {
		wl_pointer_send_button (pointer, 0, timestamp, BTN_LEFT, WL_POINTER_BUTTON_STATE_RELEASED);
	}
}
static void keyboard_event (int key) {
	
}

static int backend_readable (int fd, uint32_t mask, void *data) {
	backend_dispatch_nonblocking ();
}

int main () {
	struct callbacks callbacks = {&resize, &draw, &mouse_event, &keyboard_event};
	backend_init (&callbacks);
	wl_list_init (&surfaces);
	wl_array_init (&pointers);
	display = wl_display_create ();
	wl_display_add_socket (display, "wayland-0");
	wl_global_create (display, &wl_compositor_interface, 3, NULL, &compositor_bind);
	wl_global_create (display, &wl_shell_interface, 1, NULL, &shell_bind);
	wl_global_create (display, &xdg_shell_interface, 1, NULL, &xdg_shell_bind);
	wl_global_create (display, &wl_seat_interface, 1, NULL, &seat_bind);
	wl_display_init_shm (display);
	struct wl_event_loop *event_loop = wl_display_get_event_loop (display);
	wl_event_loop_add_fd (event_loop, backend_get_fd(), WL_EVENT_READABLE, backend_readable, NULL);
	
	wl_display_run (display);
	
	wl_display_destroy (display);
	return 0;
}
