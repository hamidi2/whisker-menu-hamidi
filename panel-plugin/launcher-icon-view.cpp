/*
 * Copyright (C) 2019-2025 Graeme Gott <graeme@gottcode.org>
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

#include "launcher-icon-view.h"

#include "icon-renderer.h"
#include "settings.h"
#include "slot.h"
#include <gdk/gdkkeysyms.h>

using namespace WhiskerMenu;

static void wm_log(const char* what)
{
	FILE* f = fopen("/tmp/wm-debug.log", "a");
	if (f) { fprintf(f, "%s\n", what); fclose(f); }
}

//-----------------------------------------------------------------------------

LauncherIconView::LauncherIconView() :
	m_icon_renderer(nullptr),
	m_icon_size(-1),
	m_authority(false),
	m_width_pending(false)
{
	// Create the view
	m_view = GTK_ICON_VIEW(gtk_icon_view_new());

	m_icon_renderer = whiskermenu_icon_renderer_new();
	g_object_set(m_icon_renderer,
			"stretch", true,
			"xalign", 0.0,
			"yalign", 0.5,
			nullptr);
	GtkCellLayout* cell_layout = GTK_CELL_LAYOUT(m_view);
	gtk_cell_layout_pack_start(cell_layout, m_icon_renderer, false);
	gtk_cell_layout_set_attributes(cell_layout, m_icon_renderer, "gicon", COLUMN_ICON, "launcher", COLUMN_LAUNCHER, nullptr);

	gtk_icon_view_set_markup_column(m_view, COLUMN_TEXT);
	// لیست دوستونه: آیکون کنار متن (افقی) به‌جای زیرِ آن
	gtk_icon_view_set_item_orientation(m_view, GTK_ORIENTATION_HORIZONTAL);
	gtk_icon_view_set_columns(m_view, 2);
	gtk_icon_view_set_row_spacing(m_view, 0);
	gtk_icon_view_set_column_spacing(m_view, 6);
	gtk_icon_view_set_spacing(m_view, 6);

	reload_icon_size();

	// Use single clicks to activate items
	gtk_icon_view_set_activate_on_single_click(m_view, true);

	// Only allow up to one selected item
	gtk_icon_view_set_selection_mode(m_view, GTK_SELECTION_SINGLE);

	// ناوبری دابل‌کامندری: Down در ته ستون ← سر ستون بعدی؛ Up در سر ستون ← ته ستون قبلی
	g_signal_connect(m_view, "key-press-event",
		G_CALLBACK(+[](GtkWidget*, GdkEventKey* event, gpointer user_data) -> gboolean
		{
			return static_cast<LauncherIconView*>(user_data)->wrap_navigation(event);
		}),
	this);

	g_object_ref_sink(m_view);

	gtk_style_context_add_class(gtk_widget_get_style_context(GTK_WIDGET(m_view)), "launchers");

	// Handle hover selection
	enable_hover_selection(GTK_WIDGET(m_view));
}

//-----------------------------------------------------------------------------

LauncherIconView::~LauncherIconView()
{
	gtk_widget_destroy(GTK_WIDGET(m_view));
	g_object_unref(m_view);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_cursor() const
{
	GtkTreePath* path = nullptr;
	gtk_icon_view_get_cursor(m_view, &path, nullptr);
	return path;
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_path_at_pos(int x, int y) const
{
	return gtk_icon_view_get_path_at_pos(m_view, x, y);
}

//-----------------------------------------------------------------------------

GtkTreePath* LauncherIconView::get_selected_path() const
{
	GtkTreePath* path = nullptr;
	GList* selection = gtk_icon_view_get_selected_items(m_view);
	if (selection)
	{
		path = gtk_tree_path_copy(static_cast<GtkTreePath*>(selection->data));
	}
	g_list_free_full(selection, reinterpret_cast<GDestroyNotify>(&gtk_tree_path_free));
	return path;
}

//-----------------------------------------------------------------------------

bool LauncherIconView::is_path_selected(GtkTreePath* path) const
{
	return gtk_icon_view_path_is_selected(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::activate_path(GtkTreePath* path)
{
	gtk_icon_view_item_activated(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::scroll_to_path(GtkTreePath* path)
{
	gtk_icon_view_scroll_to_path(m_view, path, true, 0.5f, 0.5f);
}

//-----------------------------------------------------------------------------

void LauncherIconView::select_path(GtkTreePath* path)
{
	gtk_icon_view_select_path(m_view, path);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_cursor(GtkTreePath* path)
{
	gtk_icon_view_set_cursor(m_view,path, nullptr, false);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_fixed_height_mode(bool)
{
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_selection_mode(GtkSelectionMode mode)
{
	gtk_icon_view_set_selection_mode(m_view, mode);
}

//-----------------------------------------------------------------------------

void LauncherIconView::hide_tooltips()
{
	gtk_icon_view_set_tooltip_column(m_view, -1);
}

//-----------------------------------------------------------------------------

void LauncherIconView::show_tooltips()
{
	gtk_icon_view_set_tooltip_column(m_view, COLUMN_TOOLTIP);
}

//-----------------------------------------------------------------------------

void LauncherIconView::clear_selection()
{
	gtk_icon_view_unselect_all(m_view);
}

//-----------------------------------------------------------------------------

void LauncherIconView::collapse_all()
{
}

//-----------------------------------------------------------------------------

void LauncherIconView::schedule_width_update()
{
	if (m_width_pending)
	{
		return;
	}
	m_width_pending = true;
	g_idle_add(+[](gpointer data) -> gint
	{
		auto v = static_cast<LauncherIconView*>(data);
		v->m_width_pending = false;
		v->update_item_width(v->m_authority);
		return G_SOURCE_REMOVE;
	}, this);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_width_authority()
{
	wm_log("set_width_authority");
	m_authority = true;
}

void LauncherIconView::set_model(GtkTreeModel* model)
{
	wm_log("set_model");
	m_model = model;
	gtk_icon_view_set_model(m_view, model);
	update_item_width(m_authority);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_model()
{
	m_model = nullptr;
	gtk_icon_view_set_model(m_view, nullptr);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_drag_source(GdkModifierType start_button_mask, const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_icon_view_enable_model_drag_source(m_view, start_button_mask, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherIconView::set_drag_dest(const GtkTargetEntry* targets, gint n_targets, GdkDragAction actions)
{
	gtk_icon_view_enable_model_drag_dest(m_view, targets, n_targets, actions);
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_drag_source()
{
	gtk_icon_view_unset_model_drag_source(m_view);
}

int LauncherIconView::s_item_width = 0;

//-----------------------------------------------------------------------------

void LauncherIconView::update_item_width(bool measure)
{
	wm_log(measure ? "update_item_width(measure=true)" : "update_item_width(false)");
	if (measure)
	{
		// عرض = پهن‌ترین نام + آیکون + فاصله (فقط هنگام تغییر favorites)
		GtkTreeModel* model = gtk_icon_view_get_model(m_view);
		int text_w = 0;
		if (model)
		{
			PangoLayout* layout = gtk_widget_create_pango_layout(GTK_WIDGET(m_view), nullptr);
			GtkTreeIter iter;
			if (gtk_tree_model_get_iter_first(model, &iter))
			{
				do
				{
					gchar* text = nullptr;
					gtk_tree_model_get(model, &iter, COLUMN_TEXT, &text, -1);
					if (text)
					{
						pango_layout_set_markup(layout, text, -1);
						int w = 0;
						pango_layout_get_pixel_size(layout, &w, nullptr);
						if (w > text_w)
						{
							text_w = w;
						}
						g_free(text);
					}
				} while (gtk_tree_model_iter_next(model, &iter));
			}
			g_object_unref(layout);
		}
		int icon = (m_icon_size > 1) ? m_icon_size : 0;
		s_item_width = text_w + 6; // متن + گاپ (آیکون سهم رندرر است)
		{
			FILE* f = fopen("/tmp/wm-debug.log", "a");
			if (f)
			{
				fprintf(f, "measure=%d text_w=%d icon=%d s=%d\n", measure ? 1 : 0, text_w, icon, s_item_width);
				fclose(f);
			}
		}
		if (s_item_width < 60)
		{
			s_item_width = 60;
		}
	}
	if (s_item_width > 0)
	{
		gtk_icon_view_set_item_width(m_view, 150);
		gint mn1 = 0, nt1 = 0;
		gtk_widget_get_preferred_width(GTK_WIDGET(m_view), &mn1, &nt1);
		gtk_icon_view_set_item_width(m_view, s_item_width);
		gint mn = 0, nt = 0;
		gtk_widget_get_preferred_width(GTK_WIDGET(m_view), &mn, &nt);
		int applied = gtk_icon_view_get_item_width(m_view);
		FILE* f = fopen("/tmp/wm-debug.log", "a");
		if (f)
		{
			fprintf(f, "view-check: set=%d readback=%d viewmin=%d viewnat=%d rows=%d\n",
					s_item_width, applied, mn, nt,
					m_model ? gtk_tree_model_iter_n_children(m_model, nullptr) : -1);
			gint rmn = 0, rnt = 0, tmn = 0, tnt = 0;
			gtk_cell_renderer_get_preferred_width(GTK_CELL_RENDERER(m_icon_renderer), GTK_WIDGET(m_view), &rmn, &rnt);
			gint marg = 0, sp = 0, cs = 0, ip = 0, iw = 0, cols = 0;
			g_object_get(m_view, "margin", &marg, "spacing", &sp, "column-spacing", &cs, "item-padding", &ip, "item-width", &iw, "columns", &cols, nullptr);
			fprintf(f, "probe150: min=%d nat=%d | renderer min=%d nat=%d | margin=%d spacing=%d colspacing=%d itempad=%d itemw=%d cols=%d\n", mn1, nt1, rmn, rnt, marg, sp, cs, ip, iw, cols);
			fclose(f);
		}
	}
}

//-----------------------------------------------------------------------------

void LauncherIconView::unset_drag_dest()
{
	gtk_icon_view_unset_model_drag_dest(m_view);
}

gboolean LauncherIconView::wrap_navigation(GdkEventKey* event)
{
	GtkTreeModel* model = gtk_icon_view_get_model(m_view);
	if (!model)
	{
		return FALSE;
	}
	guint keyval = 0;
	gdk_event_get_keyval(reinterpret_cast<GdkEvent*>(event), &keyval);
	const bool down = (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down);
	const bool up = (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up);
	if (!down && !up)
	{
		return FALSE;
	}

	const int n = gtk_tree_model_iter_n_children(model, nullptr);
	const int cols = 2;
	const int rows_first = (n + cols - 1) / cols;
	GtkTreePath* cursor = nullptr;
	gtk_icon_view_get_cursor(m_view, &cursor, nullptr);
	if (!cursor)
	{
		return FALSE;
	}
	const int idx = gtk_tree_path_get_indices(cursor)[0];
	int col = idx / rows_first;
	int row = idx - col * rows_first;
	int rows_in_col = (col == cols - 1) ? (n - (cols - 1) * rows_first) : rows_first;
	if (rows_in_col < 1)
	{
		rows_in_col = rows_first;
	}

	int target = -1;
	if (down)
	{
		if (row + 1 < rows_in_col)
		{
			target = col * rows_first + row + 1;
		}
		else if (col + 1 < cols)
		{
			target = (col + 1) * rows_first;
		}
	}
	else
	{
		if (row > 0)
		{
			target = col * rows_first + row - 1;
		}
		else if (col > 0)
		{
			int prev_rows = (col - 1 == cols - 1) ? (n - (cols - 1) * rows_first) : rows_first;
			target = (col - 1) * rows_first + prev_rows - 1;
		}
	}
	gtk_tree_path_free(cursor);
	if (target < 0 || target >= n)
	{
		return TRUE;
	}
	GtkTreePath* path = gtk_tree_path_new_from_indices(target, -1);
	gtk_icon_view_unselect_all(m_view);
	gtk_icon_view_select_path(m_view, path);
	gtk_icon_view_set_cursor(m_view, path, nullptr, FALSE);
	gtk_icon_view_scroll_to_path(m_view, path, TRUE, down ? 1.0 : 0.0, 0.5);
	gtk_tree_path_free(path);
	return TRUE;
}

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void LauncherIconView::reload_icon_size()
{
	// Fetch icon size
	if (m_icon_size == wm_settings->launcher_icon_size.get_size())
	{
		return;
	}
	m_icon_size = wm_settings->launcher_icon_size.get_size();

	// Configure icon renderer
	if (m_icon_size > 1)
	{
		g_object_set(m_icon_renderer, "size", m_icon_size, "visible", true, nullptr);
	}
	else
	{
		g_object_set(m_icon_renderer, "visible", false, nullptr);
	}

	// Reset padding to fix icon clipping
	gtk_icon_view_set_item_padding(m_view, 0);

	gtk_icon_view_set_item_padding(m_view, 0);
	update_item_width(false);
}

//-----------------------------------------------------------------------------
