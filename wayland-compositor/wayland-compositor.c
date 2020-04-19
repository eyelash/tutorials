#include <wayland-server.h>
#include "xdg-shell.h"
#include <stdlib.h>
#include "backend.h"
#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglmesaext.h>
#include <stdio.h>

static PFNEGLBINDWAYLANDDISPLAYWL eglBindWaylandDisplayWL = NULL;
static PFNEGLQUERYWAYLANDBUFFERWL eglQueryWaylandBufferWL = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = NULL;

#include "texture.h"

static struct wl_display *display;
static int pointer_x, pointer_y;
static struct modifier_state modifier_state;
static char redraw_needed = 0;

struct client {
	struct wl_client *client;
	struct wl_resource *pointer;
	struct wl_resource *keyboard;
	struct wl_list link;
};
static struct wl_list clients;

static struct client *get_client (struct wl_client *_client) {
	struct client *client;
	wl_list_for_each (client, &clients, link) {
		if (client->client == _client) return client;
	}
	client = calloc (1, sizeof(struct client));
	client->client = _client;
	wl_list_insert (&clients, &client->link);
	return client;
}

struct surface {
	struct wl_resource *surface;
	struct wl_resource *xdg_surface;
	struct wl_resource *xdg_toplevel;
	struct wl_resource *buffer;
	struct wl_resource *frame_callback;
	int x, y;
	struct texture texture;
	struct client *client;
	struct wl_list link;
};
static struct wl_list surfaces;
static struct surface *cursor = NULL;
static struct surface *moving_surface = NULL;
static struct surface *active_surface = NULL;
static struct surface *pointer_surface = NULL; // surface under the pointer

static void deactivate_surface (struct surface *surface) {
	if (surface->client->keyboard) wl_keyboard_send_leave (surface->client->keyboard, 0, surface->surface);
	struct wl_array state_array;
	wl_array_init (&state_array);
	xdg_toplevel_send_configure (surface->xdg_toplevel, 0, 0, &state_array);
}
static void activate_surface (struct surface *surface) {
	wl_list_remove (&surface->link);
	wl_list_insert (&surfaces, &surface->link);
	struct wl_array array;
	wl_array_init (&array);
	if (surface->client->keyboard) {
		wl_keyboard_send_enter (surface->client->keyboard, 0, surface->surface, &array);
		wl_keyboard_send_modifiers (surface->client->keyboard, 0, modifier_state.depressed, modifier_state.latched, modifier_state.locked, modifier_state.group);
	}
	int32_t *states = wl_array_add (&array, sizeof(int32_t));
	states[0] = XDG_TOPLEVEL_STATE_ACTIVATED;
	xdg_toplevel_send_configure (surface->xdg_toplevel, 0, 0, &array);
}
static void delete_surface (struct wl_resource *resource) {
	struct surface *surface = wl_resource_get_user_data (resource);
	wl_list_remove (&surface->link);
	if (surface == active_surface) active_surface = NULL;
	if (surface == pointer_surface) pointer_surface = NULL;
	free (surface);
	redraw_needed = 1;
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
	if (!surface->buffer) {
		xdg_surface_send_configure(surface->xdg_surface, 0);
		return;
	}
	EGLint texture_format;
	if (eglQueryWaylandBufferWL (backend_get_egl_display(), surface->buffer, EGL_TEXTURE_FORMAT, &texture_format)) {
		EGLint width, height;
		eglQueryWaylandBufferWL (backend_get_egl_display(), surface->buffer, EGL_WIDTH, &width);
		eglQueryWaylandBufferWL (backend_get_egl_display(), surface->buffer, EGL_WIDTH, &height);
		EGLAttrib attribs = EGL_NONE;
		EGLImage image = eglCreateImage (backend_get_egl_display(), EGL_NO_CONTEXT, EGL_WAYLAND_BUFFER_WL, surface->buffer, &attribs);
		texture_delete (&surface->texture);
		texture_create_from_egl_image (&surface->texture, width, height, image);
		eglDestroyImage (backend_get_egl_display(), image);
	}
	else {
		struct wl_shm_buffer *shm_buffer = wl_shm_buffer_get (surface->buffer);
		uint32_t width = wl_shm_buffer_get_width (shm_buffer);
		uint32_t height = wl_shm_buffer_get_height (shm_buffer);
		void *data = wl_shm_buffer_get_data (shm_buffer);
		texture_delete (&surface->texture);
		texture_create (&surface->texture, width, height, data);
	}
	wl_buffer_send_release (surface->buffer);
	redraw_needed = 1;
}
static void surface_set_buffer_transform (struct wl_client *client, struct wl_resource *resource, int32_t transform) {
	
}
static void surface_set_buffer_scale (struct wl_client *client, struct wl_resource *resource, int32_t scale) {
	
}
static struct wl_surface_interface surface_implementation = {&surface_destroy, &surface_attach, &surface_damage, &surface_frame, &surface_set_opaque_region, &surface_set_input_region, &surface_commit, &surface_set_buffer_transform, &surface_set_buffer_scale};

// region
static void region_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void region_add (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static void region_subtract (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static struct wl_region_interface region_implementation = {&region_destroy, &region_add, &region_subtract};

// compositor
static void compositor_create_surface (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct surface *surface = calloc (1, sizeof(struct surface));
	surface->surface = wl_resource_create (client, &wl_surface_interface, 3, id);
	wl_resource_set_implementation (surface->surface, &surface_implementation, surface, &delete_surface);
	surface->client = get_client (client);
	wl_list_insert (&surfaces, &surface->link);
}
static void compositor_create_region (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *region = wl_resource_create (client, &wl_region_interface, 1, id);
	wl_resource_set_implementation (region, &region_implementation, NULL, NULL);
}
static struct wl_compositor_interface compositor_implementation = {&compositor_create_surface, &compositor_create_region};
static void compositor_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: compositor\n");
	struct wl_resource *resource = wl_resource_create (client, &wl_compositor_interface, 1, id);
	wl_resource_set_implementation (resource, &compositor_implementation, NULL, NULL);
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
static struct wl_shell_surface_interface shell_surface_implementation = {&shell_surface_pong, &shell_surface_move, &shell_surface_resize, &shell_surface_set_toplevel, &shell_surface_set_transient, &shell_surface_set_fullscreen, &shell_surface_set_popup, &shell_surface_set_maximized, &shell_surface_set_title, &shell_surface_set_class};

// wl shell
static void shell_get_shell_surface (struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface) {
	struct wl_resource *shell_surface = wl_resource_create (client, &wl_shell_surface_interface, 1, id);
	wl_resource_set_implementation (shell_surface, &shell_surface_implementation, NULL, NULL);
}
static struct wl_shell_interface shell_implementation = {&shell_get_shell_surface};
static void shell_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: wl_shell\n");
	struct wl_resource *resource = wl_resource_create (client, &wl_shell_interface, 1, id);
	wl_resource_set_implementation (resource, &shell_implementation, NULL, NULL);
}

// xdg toplevel
static void xdg_toplevel_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_toplevel_set_parent (struct wl_client *client, struct wl_resource *resource, struct wl_resource *parent) {
	
}
static void xdg_toplevel_set_title (struct wl_client *client, struct wl_resource *resource, const char *title) {
	
}
static void xdg_toplevel_set_app_id (struct wl_client *client, struct wl_resource *resource, const char *app_id) {
	
}
static void xdg_toplevel_show_window_menu (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, int32_t x, int32_t y) {
	
}
static void xdg_toplevel_move (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial) {
	struct surface *surface = wl_resource_get_user_data (resource);
	// during the move the surface coordinates are relative to the pointer
	surface->x = surface->x - pointer_x;
	surface->y = surface->y - pointer_y;
	moving_surface = surface;
}
static void xdg_toplevel_resize (struct wl_client *client, struct wl_resource *resource, struct wl_resource *seat, uint32_t serial, uint32_t edges) {
	
}
static void xdg_toplevel_set_max_size (struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height) {
	
}
static void xdg_toplevel_set_min_size (struct wl_client *client, struct wl_resource *resource, int32_t width, int32_t height) {
	
}
static void xdg_toplevel_set_maximized (struct wl_client *client, struct wl_resource *resource) {
	printf ("surface requested maximize\n");
}
static void xdg_toplevel_unset_maximized (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_toplevel_set_fullscreen (struct wl_client *client, struct wl_resource *resource, struct wl_resource *output) {
	
}
static void xdg_toplevel_unset_fullscreen (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_toplevel_set_minimized (struct wl_client *client, struct wl_resource *resource) {
	
}
static struct xdg_toplevel_interface xdg_toplevel_implementation = {&xdg_toplevel_destroy, &xdg_toplevel_set_parent, &xdg_toplevel_set_title, &xdg_toplevel_set_app_id, &xdg_toplevel_show_window_menu, &xdg_toplevel_move, &xdg_toplevel_resize, &xdg_toplevel_set_max_size, &xdg_toplevel_set_min_size, &xdg_toplevel_set_maximized, &xdg_toplevel_unset_maximized, &xdg_toplevel_set_fullscreen, &xdg_toplevel_unset_fullscreen, &xdg_toplevel_set_minimized};

// xdg surface
static void xdg_surface_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_surface_get_toplevel (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct surface *surface = wl_resource_get_user_data (resource);
	surface->xdg_toplevel = wl_resource_create (client, &xdg_toplevel_interface, 1, id);
	wl_resource_set_implementation (surface->xdg_toplevel, &xdg_toplevel_implementation, surface, NULL);
}
static void xdg_surface_get_popup (struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *parent, struct wl_resource *positioner) {
	
}
static void xdg_surface_set_window_geometry (struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y, int32_t width, int32_t height) {
	
}
static void xdg_surface_ack_configure (struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
	
}
static struct xdg_surface_interface xdg_surface_implementation = {&xdg_surface_destroy, &xdg_surface_get_toplevel, &xdg_surface_get_popup, &xdg_surface_set_window_geometry, &xdg_surface_ack_configure};

// xdg wm base
static void xdg_wm_base_destroy (struct wl_client *client, struct wl_resource *resource) {
	
}
static void xdg_wm_base_create_positioner (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}
static void xdg_wm_base_get_xdg_surface (struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *_surface) {
	struct surface *surface = wl_resource_get_user_data (_surface);
	surface->xdg_surface = wl_resource_create (client, &xdg_surface_interface, 1, id);
	wl_resource_set_implementation (surface->xdg_surface, &xdg_surface_implementation, surface, NULL);
}
static void xdg_wm_base_pong (struct wl_client *client, struct wl_resource *resource, uint32_t serial) {
	
}
static struct xdg_wm_base_interface xdg_wm_base_implementation = {&xdg_wm_base_destroy, &xdg_wm_base_create_positioner, &xdg_wm_base_get_xdg_surface, &xdg_wm_base_pong};
static void xdg_wm_base_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: xdg_wm_base\n");
	struct wl_resource *resource = wl_resource_create (client, &xdg_wm_base_interface, 1, id);
	wl_resource_set_implementation (resource, &xdg_wm_base_implementation, NULL, NULL);
}

// pointer
static void pointer_set_cursor (struct wl_client *client, struct wl_resource *resource, uint32_t serial, struct wl_resource *_surface, int32_t hotspot_x,
int32_t hotspot_y) {
	struct surface *surface = wl_resource_get_user_data (_surface);
	cursor = surface;
}
static void pointer_release (struct wl_client *client, struct wl_resource *resource) {
	
}
static struct wl_pointer_interface pointer_implementation = {&pointer_set_cursor, &pointer_release};

// keyboard
static void keyboard_release (struct wl_client *client, struct wl_resource *resource) {
	
}
static struct wl_keyboard_interface keyboard_implementation = {&keyboard_release};

// seat
static void seat_get_pointer (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *pointer = wl_resource_create (client, &wl_pointer_interface, 1, id);
	wl_resource_set_implementation (pointer, &pointer_implementation, NULL, NULL);
	get_client(client)->pointer = pointer;
}
static void seat_get_keyboard (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	struct wl_resource *keyboard = wl_resource_create (client, &wl_keyboard_interface, 1, id);
	wl_resource_set_implementation (keyboard, &keyboard_implementation, NULL, NULL);
	get_client(client)->keyboard = keyboard;
	int fd, size;
	backend_get_keymap (&fd, &size);
	wl_keyboard_send_keymap (keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, size);
	//close (fd);
}
static void seat_get_touch (struct wl_client *client, struct wl_resource *resource, uint32_t id) {
	
}
static struct wl_seat_interface seat_implementation = {&seat_get_pointer, &seat_get_keyboard, &seat_get_touch};
static void seat_bind (struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	printf ("bind: seat\n");
	struct wl_resource *seat = wl_resource_create (client, &wl_seat_interface, 1, id);
	wl_resource_set_implementation (seat, &seat_implementation, NULL, NULL);
	wl_seat_send_capabilities (seat, WL_SEAT_CAPABILITY_POINTER|WL_SEAT_CAPABILITY_KEYBOARD);
}

// backend callbacks
static void handle_resize_event (int width, int height) {
	glViewport (0, 0, width, height);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	glOrtho (0, width, height, 0, 1, -1);
	glMatrixMode (GL_MODELVIEW);
	
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_BLEND);
}
static void handle_draw_event (void) {
	redraw_needed = 1;
}
static void handle_mouse_motion_event (int x, int y) {
	pointer_x = x;
	pointer_y = y;
	if (cursor) redraw_needed = 1;
	if (moving_surface) {
		redraw_needed = 1;
		return;
	}
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
		if (pointer_surface && pointer_surface->client->pointer)
			wl_pointer_send_leave (pointer_surface->client->pointer, 0, pointer_surface->surface);
		pointer_surface = next_pointer_surface;
		if (pointer_surface && pointer_surface->client->pointer)
			wl_pointer_send_enter (pointer_surface->client->pointer, 0, pointer_surface->surface, x, y);
	}
	if (!pointer_surface || !pointer_surface->client->pointer) return;
	wl_fixed_t surface_x = wl_fixed_from_double (x - pointer_surface->x);
	wl_fixed_t surface_y = wl_fixed_from_double (y - pointer_surface->y);
	wl_pointer_send_motion (pointer_surface->client->pointer, backend_get_timestamp(), surface_x, surface_y);
}
static void handle_mouse_button_event (int button, int state) {
	if (moving_surface && state == WL_POINTER_BUTTON_STATE_RELEASED) {
		moving_surface->x = pointer_x + moving_surface->x;
		moving_surface->y = pointer_y + moving_surface->y;
		moving_surface = NULL;
	}
	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (pointer_surface != active_surface) {
			if (active_surface) deactivate_surface (active_surface);
			active_surface = pointer_surface;
			if (active_surface) activate_surface (active_surface);
		}
	}
	if (!pointer_surface || !pointer_surface->client->pointer) return;
	wl_pointer_send_button (pointer_surface->client->pointer, 0, backend_get_timestamp(), button, state);
}
static void handle_key_event (int key, int state) {
	if (!active_surface || !active_surface->client->keyboard) return;
	wl_keyboard_send_key (active_surface->client->keyboard, 0, backend_get_timestamp(), key, state);
}
static void handle_modifiers_changed (struct modifier_state new_state) {
	if (new_state.depressed == modifier_state.depressed && new_state.latched == modifier_state.latched && new_state.locked == modifier_state.locked && new_state.group == modifier_state.group) return;
	modifier_state = new_state;
	if (active_surface && active_surface->client->keyboard)
		wl_keyboard_send_modifiers (active_surface->client->keyboard, 0, modifier_state.depressed, modifier_state.latched, modifier_state.locked, modifier_state.group);
}
static struct callbacks callbacks = {&handle_resize_event, &handle_draw_event, &handle_mouse_motion_event, &handle_mouse_button_event, &handle_key_event, &handle_modifiers_changed};

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
		if (surface->frame_callback) {
			wl_callback_send_done (surface->frame_callback, backend_get_timestamp());
			wl_resource_destroy (surface->frame_callback);
			surface->frame_callback = NULL;
		}
	}
	// draw the cursor last
	if (cursor) texture_draw (&cursor->texture, pointer_x, pointer_y);
	
	glFlush ();
	backend_swap_buffers ();
}

static void main_loop (void) {
	struct wl_event_loop *event_loop = wl_display_get_event_loop (display);
	int wayland_fd = wl_event_loop_get_fd (event_loop);
	while (1) {
		wl_event_loop_dispatch (event_loop, 0);
		backend_dispatch_nonblocking ();
		wl_display_flush_clients (display);
		if (redraw_needed) {
			draw ();
			redraw_needed = 0;
		}
		else {
			backend_wait_for_events (wayland_fd);
		}
	}
}

int main () {
	backend_init (&callbacks);
	eglBindWaylandDisplayWL = (PFNEGLBINDWAYLANDDISPLAYWL) eglGetProcAddress ("eglBindWaylandDisplayWL");
	eglQueryWaylandBufferWL = (PFNEGLQUERYWAYLANDBUFFERWL) eglGetProcAddress ("eglQueryWaylandBufferWL");
	glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress ("glEGLImageTargetTexture2DOES");
	wl_list_init (&clients);
	wl_list_init (&surfaces);
	display = wl_display_create ();
	wl_display_add_socket_auto (display);
	wl_global_create (display, &wl_compositor_interface, 3, NULL, &compositor_bind);
	wl_global_create (display, &wl_shell_interface, 1, NULL, &shell_bind);
	wl_global_create (display, &xdg_wm_base_interface, 1, NULL, &xdg_wm_base_bind);
	wl_global_create (display, &wl_seat_interface, 1, NULL, &seat_bind);
	eglBindWaylandDisplayWL (backend_get_egl_display(), display);
	wl_display_init_shm (display);
	
	main_loop ();
	
	wl_display_destroy (display);
	return 0;
}
