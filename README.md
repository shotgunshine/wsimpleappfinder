# wsimpleappfinder

GTK4 application launcher for wayland compositors

![wsimpleappfinder](screenshot.png)

build with `gcc main.c -o wsimpleappfinder $( pkg-config --cflags --libs gtk4 gtk4-layer-shell-0 )`

favorites are parsed from `$XDG_CONFIG_HOME/wsimpleappfinder/favorites` in the format `app_id.desktop` separated by newlines
