// gcc -o wayland-egl wayland-egl.c -lwayland-client -lwayland-egl -lEGL -lGL

#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 256
#define HEIGHT 256

static struct wl_display *display;
static struct wl_compositor *compositor = NULL;
static struct wl_shell *shell = NULL;
static EGLDisplay egl_display;
static char running = 1;

// listeners
static void registry_add_object (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	if (!strcmp(interface,"wl_compositor")) {
		compositor = wl_registry_bind (registry, name, &wl_compositor_interface, 0);
	}
	else if (!strcmp(interface,"wl_shell")) {
		shell = wl_registry_bind (registry, name, &wl_shell_interface, 0);
	}
}
static void registry_remove_object (void *data, struct wl_registry *registry, uint32_t name) {
	printf ("registry_remove_object\n");
}
static struct wl_registry_listener registry_listener = {&registry_add_object, &registry_remove_object};

static void shell_surface_ping (void *data, struct wl_shell_surface *shell_surface, uint32_t serial) {
	wl_shell_surface_pong (shell_surface, serial);
}
static void shell_surface_configure (void *data, struct wl_shell_surface *shell_surface, uint32_t edges, int32_t width, int32_t height) {
	printf ("shell_surface_configure\n");
}
static void shell_surface_popup_done (void *data, struct wl_shell_surface *shell_surface) {
	
}
static struct wl_shell_surface_listener shell_surface_listener = {&shell_surface_ping, &shell_surface_configure, &shell_surface_popup_done};

// window
struct window {
	int32_t width, height;
	EGLContext egl_context;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_egl_window *egl_window;
	EGLSurface egl_surface;
};
static struct window create_window (int32_t width, int32_t height) {
	struct window window;
	window.width = width;
	window.height = height;
	
	eglBindAPI (EGL_OPENGL_API);
	EGLint attributes[] = {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
	EGL_NONE};
	EGLConfig config;
	EGLint num_config;
	eglChooseConfig (egl_display, attributes, &config, 1, &num_config);
	window.egl_context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);
	
	window.surface = wl_compositor_create_surface (compositor);
	window.shell_surface = wl_shell_get_shell_surface (shell, window.surface);
	wl_shell_surface_add_listener (window.shell_surface, &shell_surface_listener, NULL);
	wl_shell_surface_set_toplevel (window.shell_surface);
	window.egl_window = wl_egl_window_create (window.surface, width, height);
	window.egl_surface = eglCreateWindowSurface (egl_display, config, window.egl_window, NULL);
	eglMakeCurrent (egl_display, window.egl_surface, window.egl_surface, window.egl_context);
	
	return window;
}
static void delete_window (struct window *window) {
	eglDestroySurface (egl_display, window->egl_surface);
	wl_egl_window_destroy (window->egl_window);
	wl_shell_surface_destroy (window->shell_surface);
	wl_surface_destroy (window->surface);
	eglDestroyContext (egl_display, window->egl_context);
}
static void draw_window (struct window *window) {
	glClearColor (0.0, 1.0, 0.0, 1.0);
	glClear (GL_COLOR_BUFFER_BIT);
	eglSwapBuffers (egl_display, window->egl_surface);
}

int main () {
	display = wl_display_connect (NULL);
	struct wl_registry *registry = wl_display_get_registry (display);
	wl_registry_add_listener (registry, &registry_listener, NULL);
	wl_display_dispatch (display);
	egl_display = eglGetDisplay (display);
	eglInitialize (egl_display, NULL, NULL);
	
	struct window window = create_window (WIDTH, HEIGHT);
	
	while (running) {
		wl_display_dispatch_pending (display);
		draw_window (&window);
	}
	
	delete_window (&window);
	eglTerminate (egl_display);
	wl_display_disconnect (display);
	return 0;
}
