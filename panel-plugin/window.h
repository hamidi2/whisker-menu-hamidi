/*
 * Copyright (C) 2013-2025 Graeme Gott <graeme@gottcode.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef WHISKERMENU_WINDOW_H
#define WHISKERMENU_WINDOW_H

#include <vector>

#include <gtk/gtk.h>

namespace WhiskerMenu
{

class ApplicationsPage;
class CategoryButton;
class FavoritesPage;
class Page;
class Plugin;
class Profile;
class Resizer;
class RecentPage;
class SearchPage;

class Window
{
public:
	explicit Window(Plugin* plugin);
	~Window();

	Window(const Window&) = delete;
	Window(Window&&) = delete;
	Window& operator=(const Window&) = delete;
	Window& operator=(Window&&) = delete;

	enum Position
	{
		PositionAtButton,
		PositionAtCursor,
		PositionAtCenter
	};

	GtkWidget* get_widget() const
	{
		return GTK_WIDGET(m_window);
	}

	GtkEntry* get_search_entry() const
	{
		return m_search_entry;
	}

	ApplicationsPage* get_applications() const
	{
		return m_applications;
	}

	FavoritesPage* get_favorites() const
	{
		return m_favorites;
	}

	RecentPage* get_recent() const
	{
		return m_recent;
	}

	Page* get_active_page();

	void hide(bool lost_focus = false);
	void show(const Position position);
	void resize(int delta_x, int delta_y, int delta_width, int delta_height);
	void apply_target_size();
	void resize_start();
	void resize_end();
	bool is_resizing() const
	{
		return m_resizing;
	}
	void set_child_has_focus();
	void set_categories(const std::vector<CategoryButton*>& categories);
	void set_items();
	void set_loaded();
	bool set_size(int width, int height);
	void sync_width();
	void update_min_h();
	// ظرفیت ردیف‌های هر ستون favorites از ارتفاع فعلی منو (برای چیدمان تا-پر-شدن)
	int favorites_capacity_rows() const;
	void unset_items();

private:
	GtkWidget* get_active_category_button();
	gboolean on_key_press_event(GtkWidget* widget, GdkEventKey* key_event);
	gboolean on_key_press_event_after(GtkWidget* widget, GdkEventKey* key_event);
	gboolean on_map_event();
	void on_state_flags_changed(GtkWidget* widget);
	void on_screen_changed(GtkWidget* widget);
	gboolean on_draw_event(GtkWidget* widget, cairo_t* cr);
	void check_scrollbar_needed();
	void favorites_toggled();
	void recent_toggled();
	void category_toggled();
	void center_window();
	void move_window();
	void reset_default_button();
	void show_favorites();
	void show_default_page();
	void search();
	void update_layout();

private:
	Plugin* m_plugin;

	GtkWindow* m_window;

	GtkStack* m_window_stack;
	GtkSpinner* m_window_load_spinner;

	GtkBox* m_vbox;
	GtkBox* m_title_box;
	GtkBox* m_commands_box;
	GtkBox* m_search_box;
	GtkStack* m_contents_stack;
	GtkGrid* m_contents_box;
	GtkBox* m_categories_box;
	GtkStack* m_panels_stack;

	Resizer* m_resize[8];
	Position m_position;
	GdkRectangle m_workarea = {0, 0, 0, 0};
	// پایینِ مجاز منو (لبهٔ بالای پنل = انتهای workarea)؛ حین درگِ resize رشد به پایین از این عبور نمی‌کند
	int m_max_bottom = 0;
	// درگِ لبه: موشن‌ها فقط هدف را به‌روز می‌کنند؛ اعمال یک‌بار در هر چرخهٔ main-loop
	// (coalesce) — اعمالِ re-entrant داخل هندلر = پیرپیر حین حرکت تند
	int m_target_x = 0;
	int m_target_y = 0;
	int m_target_w = 0;
	int m_target_h = 0;
	guint m_size_idle_id = 0;
	// حداقل ارتفاعِ محتوای منو (یک‌بار در شروع درگ) — GTK کوچک‌تر از این را نمی‌پذیرد؛
	// بدون لحاظ آن، y دنبال موس می‌رود و پایین از پنل رد می‌شود
	int m_min_h = 10;
	// chrome واقعی آخرین فریم سازگار (win_alloc − view_alloc) — مبنای تخمین
	// (mutable: favorites_capacity_rows const است و آن را به‌روز می‌کند)
	mutable int m_last_chrome = 0;
	GdkRectangle m_monitor;

	Profile* m_profile;

	GtkWidget* m_commands_spacer;
	GtkWidget* m_commands_button[9];
	gulong m_command_slots[9];

	GtkEntry* m_search_entry;

	SearchPage* m_search_results;
	FavoritesPage* m_favorites;
	RecentPage* m_recent;
	ApplicationsPage* m_applications;

	GtkScrolledWindow* m_sidebar;
	GtkBox* m_category_buttons;
	CategoryButton* m_default_button;
	GtkSizeGroup* m_sidebar_size_group;

	GdkRectangle m_geometry;
	bool m_layout_ltr;
	bool m_layout_categories_horizontal;
	bool m_layout_categories_alternate;
	bool m_layout_search_alternate;
	bool m_layout_commands_alternate;
	bool m_layout_profile_alternate;
	int m_profile_shape;
	bool m_supports_alpha;
	bool m_child_has_focus;
	bool m_resizing;
};

}

#endif // WHISKERMENU_WINDOW_H
