#include "backend.h"
#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static struct {
	Window window;
	EGLContext context;
	EGLSurface surface;
} window;
static Display *x_display;
static EGLDisplay egl_display;
static struct callbacks callbacks;
static char redraw_requested = -1;

static void create_window (void) {
	// setup EGL
	EGLint attribs[] = {
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		//EGL_RENDERABLE_TYPE, 
	EGL_NONE};
	EGLConfig config;
	EGLint num_configs_returned;
	eglChooseConfig (egl_display, attribs, &config, 1, &num_configs_returned);
	
	// get the visual from the EGL config
	EGLint visual_id;
	eglGetConfigAttrib (egl_display, config, EGL_NATIVE_VISUAL_ID, &visual_id);
	XVisualInfo visual_template;
	visual_template.visualid = visual_id;
	int num_visuals_returned;
	XVisualInfo *visual = XGetVisualInfo (x_display, VisualIDMask, &visual_template, &num_visuals_returned);
	
	// create a window
	XSetWindowAttributes window_attributes;
	window_attributes.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | LeaveWindowMask;
	window_attributes.colormap = XCreateColormap (x_display, RootWindow(x_display,DefaultScreen(x_display)), visual->visual, AllocNone);
	window.window = XCreateWindow (
		x_display,
		RootWindow(x_display, DefaultScreen(x_display)),
		0, 0,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		0, // border width
		visual->depth, // depth
		InputOutput, // class
		visual->visual, // visual
		CWEventMask|CWColormap, // attribute mask
		&window_attributes // attributes
	);
	
	// EGL context and surface
	eglBindAPI (EGL_OPENGL_API);
	window.context = eglCreateContext (egl_display, config, EGL_NO_CONTEXT, NULL);
	window.surface = eglCreateWindowSurface (egl_display, config, window.window, NULL);
	eglMakeCurrent (egl_display, window.surface, window.surface, window.context);
	
	XFree (visual);
	
	XMapWindow (x_display, window.window);
}

void backend_init (struct callbacks *_callbacks) {
	callbacks = *_callbacks;
	x_display = XOpenDisplay (NULL);
	EGLint major_version, minor_version;
	egl_display = eglGetDisplay (x_display);
	eglInitialize (egl_display, &major_version, &minor_version);
	printf ("EGL version %i.%i supported\n", major_version, minor_version);
	create_window ();
}

EGLDisplay backend_get_egl_display (void) {
	return egl_display;
}

void backend_swap_buffers (void) {
	eglSwapBuffers (egl_display, window.surface);
}

int backend_get_fd (void) {
	return ConnectionNumber (x_display);
}

void backend_dispatch_nonblocking (void) {
	redraw_requested = 0;
	XEvent event;
	while (XPending(x_display)) {
		XNextEvent (x_display, &event);
		if (event.type == ConfigureNotify) {
			callbacks.resize (event.xconfigure.width, event.xconfigure.height);
		}
		else if (event.type == Expose) {
			redraw_requested = 1;
		}
		else if (event.type == MotionNotify) {
			callbacks.mouse_event (event.xbutton.x, event.xbutton.y, 0);
		}
		else if (event.type == ButtonPress) {
			callbacks.mouse_event (event.xbutton.x, event.xbutton.y, 1);
		}
		else if (event.type == ButtonRelease) {
			callbacks.mouse_event (event.xbutton.x, event.xbutton.y, 2);
		}
	}
	if (redraw_requested) callbacks.draw ();
	redraw_requested = -1;
}

void backend_request_redraw (void) {
	if (redraw_requested == -1) callbacks.draw ();
	else redraw_requested = 1;
}

long backend_get_timestamp (void) {
	struct timespec t;
	clock_gettime (CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
