// gcc -o libinput libinput.c -linput -ludev -lxkbcommon

#include <libinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <xkbcommon/xkbcommon.h>
#include <stdio.h>

#define EXIT(msg) { fputs (msg, stderr); exit (EXIT_FAILURE); }

static struct xkb_context *xkb_context;
static struct xkb_keymap *keymap = NULL;
static struct xkb_state *xkb_state = NULL;
static double x = 0, y = 0;

static void process_event (struct libinput_event *event) {
	int type = libinput_event_get_type (event);
	if (type == LIBINPUT_EVENT_POINTER_MOTION) {
		struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event (event);
		x += libinput_event_pointer_get_dx (pointer_event);
		y += libinput_event_pointer_get_dy (pointer_event);
		printf ("pointer motion event: %d %d\n", (int)x, (int)y);
	}
	else if (type == LIBINPUT_EVENT_POINTER_BUTTON) {
		struct libinput_event_pointer *pointer_event = libinput_event_get_pointer_event (event);
		printf ("pointer button event\n");
	}
	else if (type == LIBINPUT_EVENT_KEYBOARD_KEY) {
		struct libinput_event_keyboard *keyboard_event = libinput_event_get_keyboard_event (event);
		uint32_t key = libinput_event_keyboard_get_key (keyboard_event);
		int state = libinput_event_keyboard_get_key_state (keyboard_event);
		xkb_state_update_key (xkb_state, key+8, state);
		if (state == LIBINPUT_KEY_STATE_PRESSED) {
			uint32_t utf32 = xkb_state_key_get_utf32 (xkb_state, key+8);
			if (utf32) {
				if (utf32 >= 0x21 && utf32 <= 0x7E) {
					printf ("the key %c was pressed\n", (char)utf32);
				}
				else {
					printf ("the key U+%04X was pressed\n", utf32);
				}
			}
		}
	}
	libinput_event_destroy (event);
}

static int open_restricted (const char *path, int flags, void *user_data) {
	return open (path, flags);
}
static void close_restricted (int fd, void *user_data) {
	close (fd);
}
static struct libinput_interface interface = {&open_restricted, &close_restricted};

int main () {
	struct udev *udev = udev_new ();
	if (!udev) EXIT ("udev error\n");
	struct libinput *libinput = libinput_udev_create_context (&interface, NULL, udev);
	if (!libinput) EXIT ("libinput error\n");
	if (libinput_udev_assign_seat (libinput, "seat0") == -1)
		EXIT ("cannot assign seat\n");
	
	xkb_context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
	keymap = xkb_keymap_new_from_names (xkb_context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!keymap) EXIT ("keymap error\n");
	xkb_state = xkb_state_new (keymap);
	
	while (1) {
		struct pollfd fd = {libinput_get_fd(libinput), POLLIN, 0};
		poll (&fd, 1, -1);
		libinput_dispatch (libinput);
		struct libinput_event *event;
		while (event = libinput_get_event(libinput)) {
			process_event (event);
		}
	}
	
	libinput_unref (libinput);
	udev_unref (udev);
	return 0;
}
