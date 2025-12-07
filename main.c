/*
wsimpleappfinder: a simple GTK application launcher
Copyright (C) 2025 Fuzzy Dunlop

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include <gtk/gtk.h>
#include <gtk4-layer-shell/gtk4-layer-shell.h>
#define MARGIN 4
#ifndef LINE_MAX
#define LINE_MAX 10000
#endif

struct appfinder {
	GList *apps;
	GHashTable *cache;
	GtkWidget *window;
	GtkWidget *scrolled;
	GtkWidget *menu;
	GtkWidget *search;
	GtkWidget *side;
};

void focus_change_cb(GObject *self, GParamSpec *pspec, gpointer user_data) {
	gboolean is_active;
	is_active = gtk_window_is_active(GTK_WINDOW(self));
	if (!is_active) {
		gtk_widget_set_visible(GTK_WIDGET(self), FALSE);
	}
}

void key_release_cb(GtkEventControllerKey *self, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
	if (keyval == GDK_KEY_Escape) {
		struct appfinder *ctx = user_data;
		gtk_widget_set_visible(ctx->window, FALSE);
	}
}

void search_changed_cb(GtkEditable *self, gpointer user_data) {
	struct appfinder *ctx = user_data;
	const char *query = gtk_editable_get_text(self);
	GtkWidget *button = gtk_widget_get_first_child(ctx->menu);
	while (button != NULL) {
		GtkLabel *label = GTK_LABEL(gtk_widget_get_last_child(gtk_widget_get_first_child(button)));
		gtk_widget_set_visible(button, strcasestr(gtk_label_get_text(label), query) != NULL);
		button = gtk_widget_get_next_sibling(button);
	}
}

void search_activate_cb(GtkEntry *self, gpointer user_data) {
	struct appfinder *ctx = user_data;
	GtkWidget *button = gtk_widget_get_first_child(ctx->menu);
	while (button != NULL) {
		if (gtk_widget_get_visible(button)) {
			gtk_widget_activate(button);
			return;
		}
		button = gtk_widget_get_next_sibling(button);
	}
}

void session_ctl(const char *cmd) {
	if (fork() == 0) {
		execlp("systemctl", "systemctl", cmd, (char *)0);
		_exit(1);
	}
}

void poweroff_cb(GtkButton* self, gpointer user_data) {
	session_ctl("poweroff");
}

void reboot_cb(GtkButton* self, gpointer user_data) {
	session_ctl("reboot");
}

void suspend_cb(GtkButton* self, gpointer user_data) {
	struct appfinder *ctx = user_data;
	gtk_widget_set_visible(ctx->window, FALSE);
	session_ctl("suspend");
}

void hide_widget_clk_cb(GtkButton *self, GtkWidget *w) {
	gtk_widget_set_visible(w, FALSE);
}

void launch_app_clk_cb(GtkButton *self, GAppInfo *appinfo) {
	g_app_info_launch(appinfo,
			NULL,
			G_APP_LAUNCH_CONTEXT(gdk_display_get_app_launch_context(
					gtk_widget_get_display(GTK_WIDGET(self)))),
			NULL);
}

void star_app(const char *id, struct appfinder *ctx) {
	GList *a = ctx->apps;
	while (a != NULL) {
		if (strstr(id, g_app_info_get_id(a->data)) != NULL) { // get_id returns "<id>.desktop"
			GtkWidget *button = gtk_button_new();
			gtk_widget_set_tooltip_text(button, g_app_info_get_name(a->data));
			gtk_widget_add_css_class(button, "flat");
			g_signal_connect(button, "clicked", G_CALLBACK(launch_app_clk_cb), a->data);
			g_signal_connect(button, "clicked", G_CALLBACK(hide_widget_clk_cb), ctx->window);
			GtkWidget *icon = gtk_image_new_from_gicon(g_app_info_get_icon(a->data));
			gtk_button_set_child(GTK_BUTTON(button), icon);
			gtk_box_append(GTK_BOX(ctx->side), button);
			return;
		}
		a = a->next;
	}
}

int sort_apps(gconstpointer a, gconstpointer b) {
	return strcasecmp(g_app_info_get_name(G_APP_INFO(a)), g_app_info_get_name(G_APP_INFO(b)));
}

void populate_menu(struct appfinder *ctx) {
	ctx->apps = g_list_sort(g_app_info_get_all(), sort_apps);
	GList *a = ctx->apps;
	while (a != NULL) {
		if (g_app_info_should_show(a->data)) {
			GtkWidget *button = gtk_button_new();
			gtk_widget_add_css_class(button, "flat");
			gtk_box_append(GTK_BOX(ctx->menu), button);
			g_signal_connect(button, "clicked", G_CALLBACK(launch_app_clk_cb), a->data);
			g_signal_connect(button, "clicked", G_CALLBACK(hide_widget_clk_cb), ctx->window);
			gtk_widget_set_tooltip_text(button, g_app_info_get_name(a->data));

			GtkWidget *entry = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
			gtk_button_set_child(GTK_BUTTON(button), entry);
			GtkWidget *icon = gtk_image_new_from_gicon(g_app_info_get_icon(a->data));
			gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
			gtk_box_append(GTK_BOX(entry), icon);
			GtkWidget *name_label = gtk_label_new(g_app_info_get_name(a->data));
			gtk_label_set_xalign(GTK_LABEL(name_label), 0.0);
			gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_END);
			gtk_widget_set_hexpand(name_label, TRUE);
			gtk_box_append(GTK_BOX(entry), name_label);
		}
		a = a->next;
	}
	gtk_editable_set_text(GTK_EDITABLE(ctx->search), "");
	gtk_root_set_focus(GTK_ROOT(ctx->window), ctx->search);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(ctx->scrolled), NULL);
}

void activate(GtkApplication *app, gpointer user_data) {
	struct appfinder *ctx = user_data;

	if (ctx->window == NULL) {
		GtkWidget *window = gtk_application_window_new(app);
		ctx->window = window;
		gtk_window_set_title(GTK_WINDOW(window), "App Finder");
		gtk_window_set_default_size(GTK_WINDOW(window), 0, 400);
		gtk_widget_add_css_class(window, "frame");
		g_signal_connect(window, "notify::is-active", G_CALLBACK(focus_change_cb), ctx);
		GtkEventController *kb = gtk_event_controller_key_new();
		g_signal_connect(kb, "key-pressed", G_CALLBACK(key_release_cb), ctx);
		gtk_widget_add_controller(GTK_WIDGET(window), kb);

		gtk_layer_init_for_window(GTK_WINDOW(window));
		gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_OVERLAY);
		gtk_layer_set_keyboard_mode(GTK_WINDOW(window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
		gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
		gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);

		GtkWidget *container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, MARGIN);
		gtk_widget_set_margin_start(container, MARGIN);
		gtk_widget_set_margin_top(container, MARGIN);
		gtk_window_set_child(GTK_WINDOW(window), container);

		GtkWidget *side_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_add_css_class(side_box, "frame");
		gtk_widget_set_margin_bottom(side_box, MARGIN);
		gtk_box_append(GTK_BOX(container), side_box);
		GtkWidget *side_top = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_append(GTK_BOX(side_box), side_top);
		gtk_widget_set_vexpand(side_top, true);
		ctx->side = side_top;

		GtkWidget *suspend = gtk_button_new_from_icon_name("weather-clear-night-symbolic");
		gtk_widget_add_css_class(suspend, "flat");
		gtk_widget_set_tooltip_text(suspend, "Suspend");
		g_signal_connect(suspend, "clicked", G_CALLBACK(suspend_cb), ctx);
		gtk_box_append(GTK_BOX(side_box), suspend);
		GtkWidget *reboot = gtk_button_new_from_icon_name("view-refresh-symbolic");
		gtk_widget_add_css_class(reboot, "flat");
		gtk_widget_set_tooltip_text(reboot, "Reboot");
		g_signal_connect(reboot, "clicked", G_CALLBACK(reboot_cb), ctx);
		gtk_box_append(GTK_BOX(side_box), reboot);
		GtkWidget *poweroff = gtk_button_new_from_icon_name("system-shutdown-symbolic");
		gtk_widget_add_css_class(poweroff, "flat");
		gtk_widget_set_tooltip_text(poweroff, "Shutdown");
		g_signal_connect(poweroff, "clicked", G_CALLBACK(poweroff_cb), ctx);
		gtk_box_append(GTK_BOX(side_box), poweroff);

		GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_box_append(GTK_BOX(container), main_box);
		GtkWidget *search = gtk_entry_new();
		ctx->search = search;
		gtk_widget_set_margin_end(search, MARGIN);
		gtk_entry_set_placeholder_text(GTK_ENTRY(search), "Search");
		gtk_box_append(GTK_BOX(main_box), search);
		g_signal_connect(GTK_EDITABLE(search), "changed", G_CALLBACK(search_changed_cb), ctx);
		g_signal_connect(GTK_ENTRY(search), "activate", G_CALLBACK(search_activate_cb), ctx);

		GtkWidget *scrolled = gtk_scrolled_window_new();
		ctx->scrolled = scrolled;
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_box_append(GTK_BOX(main_box), scrolled);
		GtkWidget *menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		ctx->menu = menu;
		gtk_widget_set_vexpand(menu, true);
		gtk_widget_set_size_request(menu, 200, -1);
		gtk_widget_set_margin_top(menu, MARGIN);
		gtk_widget_set_margin_end(menu, MARGIN);
		gtk_widget_set_margin_bottom(menu, MARGIN);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), menu);
		populate_menu(ctx);

		char favorites[64];
		strcpy(favorites, g_get_user_config_dir());
		strcat(favorites, "/wsimpleappfinder");
		g_mkdir_with_parents(favorites, 0700);
		strcat(favorites, "/favorites");
		FILE *fp = fopen(favorites, "r");
		if (fp != NULL) {
			char line[LINE_MAX];
			while (fgets(line, LINE_MAX, fp) != NULL) {
				star_app(line, ctx);
			}
			fclose(fp);
		}

		gtk_window_present(GTK_WINDOW(ctx->window));
	} else {
		gboolean is_active;
		g_object_get(ctx->window, "is-active", &is_active, NULL);
		if (!is_active) {
			GtkWidget *child;
			while ((child = gtk_widget_get_first_child(ctx->menu)) != NULL)
				gtk_box_remove(GTK_BOX(ctx->menu), child);
			populate_menu(ctx);
		}
		gtk_widget_set_visible(ctx->window, !is_active);
	}
}

void startup(GtkApplication *app, gpointer user_data) {
	struct appfinder *ctx = user_data;
	ctx->apps = NULL;
	ctx->window = NULL;
}

int main(int argc, char **argv) {
	struct appfinder *ctx = malloc(sizeof(struct appfinder));
	GtkApplication *app = gtk_application_new("org.wsimpleappfinder", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "startup", G_CALLBACK(startup), ctx);
	g_signal_connect(app, "activate", G_CALLBACK(activate), ctx);
	return g_application_run(G_APPLICATION(app), argc, argv);
}
