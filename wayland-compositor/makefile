XDG_SHELL_PROTOCOL = $(shell pkg-config --variable=pkgdatadir wayland-protocols)/stable/xdg-shell/xdg-shell.xml

wayland-compositor: wayland-compositor.c backend-x11.c xdg-shell.c backend.h texture.h xdg-shell.h
	gcc -o $@ wayland-compositor.c backend-x11.c xdg-shell.c -lwayland-server -lX11 -lEGL -lGL -lX11-xcb -lxkbcommon-x11 -lxkbcommon

xdg-shell.c: $(XDG_SHELL_PROTOCOL)
	wayland-scanner private-code $< $@

xdg-shell.h: $(XDG_SHELL_PROTOCOL)
	wayland-scanner server-header $< $@
