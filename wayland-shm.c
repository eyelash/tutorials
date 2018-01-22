// gcc -o wayland-shm wayland-shm.c -lwayland-client

#define _GNU_SOURCE // for O_TMPFILE
#include <wayland-client.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>

#define WIDTH 256
#define HEIGHT 256

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_shell *shell = NULL;
static struct wl_shm *wl_shm = NULL;
static char running = 1;

struct window {
	int32_t width, height;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_buffer *front_buffer;
	int32_t *data;
};

// listeners
static void registry_add_object (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (!strcmp(interface,"wl_compositor")) {
		compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 1);
	}
	else if (!strcmp(interface,"wl_shell")) {
		shell = wl_registry_bind (registry, name, &wl_shell_interface, 1);
	}
	else if (!strcmp(interface,"wl_shm")) {
		wl_shm = wl_registry_bind (registry, name, &wl_shm_interface, 1);
	}
}
static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {
	
}
static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
	wl_shell_surface_pong (shell_surface, serial);
}
static void shell_surface_configure (void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height) {
	
}
static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {
	
}
static struct wl_shell_surface_listener shell_surface_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};

static void create_window (struct window *window, int32_t width, int32_t height) {
	window->width = width;
	window->height = height;
	window->surface = wl_compositor_create_surface (compositor);
	window->shell_surface = wl_shell_get_shell_surface (shell, window->surface);
	wl_shell_surface_add_listener (window->shell_surface, &shell_surface_listener, NULL);
	wl_shell_surface_set_toplevel (window->shell_surface);
	
	size_t size = width * height * 4;
	char *xdg_runtime_dir = getenv ("XDG_RUNTIME_DIR");
	int fd = open (xdg_runtime_dir, O_TMPFILE|O_RDWR|O_EXCL, 0600);
	ftruncate (fd, size);
	window->data = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	struct wl_shm_pool *pool = wl_shm_create_pool (wl_shm, fd, size);
	window->front_buffer = wl_shm_pool_create_buffer (pool, 0, width, height, width*4, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy (pool);
	close (fd);
}
static void delete_window (struct window *window) {
	wl_shell_surface_destroy (window->shell_surface);
	wl_surface_destroy (window->surface);
}
static void draw_window (struct window *window) {
	int x, y;
	for (y = 0; y < window->height; y++) {
		for (x = 0; x < window->width; x++) {
			int index = y * window->width + x;
			window->data[index] = 0xFF00FF00; // XRGB format -> green
		}
	}
	wl_surface_attach (window->surface, window->front_buffer, 0, 0);
	//wl_surface_damage (window->surface, 0, 0, window->width, window->height);
	wl_surface_commit (window->surface);
}

int main () {
	display = wl_display_connect (NULL);
	struct wl_registry *registry = wl_display_get_registry (display);
	wl_registry_add_listener (registry, &registry_listener, NULL);
	wl_display_roundtrip (display);
	
	struct window window;
	create_window (&window, WIDTH, HEIGHT);
	
	draw_window (&window);
	while (running)
		wl_display_dispatch (display);
	
	delete_window (&window);
	wl_display_disconnect (display);
	return 0;
}
