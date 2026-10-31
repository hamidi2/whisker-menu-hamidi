/*
 * Copyright (C) 2013-2021 Graeme Gott <graeme@gottcode.org>
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

#include "favorites-page.h"

#include "applications-page.h"
#include "image-menu-item.h"
#include "launcher.h"
#include "launcher-icon-view.h"
#include "launcher-view.h"
#include "settings.h"
#include "slot.h"
#include "window.h"

#include <algorithm>

#include <glib/gi18n-lib.h>

using namespace WhiskerMenu;

int FavoritesPage::s_measured_pitch = 0;
bool FavoritesPage::drag_in_progress = false;

//-----------------------------------------------------------------------------

FavoritesPage::FavoritesPage(Window* window) :
	Page(window, "user-bookmarks", _("Favorites"))
{
	view_created();
}

//-----------------------------------------------------------------------------

FavoritesPage::~FavoritesPage()
{
	if (m_resync_source)
	{
		g_source_remove(m_resync_source);
	}
	unset_menu_items();
}

//-----------------------------------------------------------------------------

bool FavoritesPage::contains(Launcher* launcher) const
{
	if (!launcher)
	{
		return false;
	}

	std::string desktop_id(launcher->get_desktop_id());
	return std::find(wm_settings->favorites.begin(), wm_settings->favorites.end(), desktop_id) != wm_settings->favorites.end();
}

//-----------------------------------------------------------------------------

void FavoritesPage::add(Launcher* launcher)
{
	if (!launcher || contains(launcher))
	{
		return;
	}

	// ترتیب نهایی را sort_favorites() در بازچینش تعیین می‌کند
	wm_settings->favorites.push_back(launcher->get_desktop_id());
	rebuild();
}

//-----------------------------------------------------------------------------

void FavoritesPage::remove(Launcher* launcher)
{
	if (!launcher)
	{
		return;
	}

	const std::string desktop_id(launcher->get_desktop_id());
	StringList& fav = wm_settings->favorites;
	for (int i = fav.size() - 1; i >= 0; --i)
	{
		if (fav[i] == desktop_id)
		{
			fav.erase(i);
		}
	}
	rebuild();
}

//-----------------------------------------------------------------------------

void FavoritesPage::set_menu_items()
{
	sort_favorites();

	StringList& fav = wm_settings->favorites;
	const int n = fav.size();

	// ستون‌بندی: تعداد ستون‌ها دستی (/favorites-columns = 1..4؛ پیش‌فرض 2) و
	// پر کردن تا ظرفیت عمودی ستون: آیتم به ستون بعد نمی‌رود مگر وقتی ستون جاری
	// پر شده باشد. سرریز از ظرفیت کل = ادامه در ستون آخر + scrollbar عمودی.
	// layout همیشه با «همهٔ» ستون‌های تنظیم‌شده انجام می‌شود — نمایان/غیرنمایان
	// وجود ندارد؛ عرض = ستون‌ها × عریض‌ترین آیتم + حاشیه‌ها
	m_columns = (wm_settings->favorites_columns > 0) ? wm_settings->favorites_columns : 2;
	m_columns = std::min(m_columns, std::max(1, n));

	// ظرفیت ردیف‌های هر ستون (فقط برای حالت ستونی؛ از ارتفاع واقعی/منو)
	const int capacity = (m_columns > 1) ? get_window()->favorites_capacity_rows() : n;

	m_model_slots.clear();
	m_overflow = false;

	if (m_columns == 1 || capacity < 1)
	{
		m_used_cols = 1;
		m_overflow = (n > capacity);
		for (int i = 0; i < n; ++i)
		{
			m_model_slots.push_back(i);
		}
	}
	else if (wm_settings->favorites_horizontal)
	{
		// سطری: پر کردن سطرها؛ سلول (r,c) ← آیتم r*cols + c
		m_used_cols = std::min(m_columns, std::max(1, n));
		const int rows = (n + m_columns - 1) / m_columns;
		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < m_columns; ++c)
			{
				const int f = r * m_columns + c;
				m_model_slots.push_back(f < n ? f : -1);
			}
		}
		m_overflow = rows > capacity;
	}
	else
	{
		// ستونی: پر کردن تا ظرفیت (capacity ردیف در هر ستون)؛ آیتم به ستون بعد
		// نمی‌رود مگر ستون جاری پر شده باشد. ستون آخر اگر سرریز شد، آیتم‌های
		// اضافه زیر آن ادامه می‌یابند (scrollbar عمودی، نه ستون تازه)
		int rows = 0;
		int counts[4] = {0, 0, 0, 0};
		for (int c = 0; c < m_columns; ++c)
		{
			const int remaining = n - c * capacity;
			counts[c] = (c == m_columns - 1) ? std::max(0, remaining) : std::max(0, std::min(capacity, remaining));
			rows = std::max(rows, counts[c]);
		}
		m_used_cols = 0;
		for (int c = 0; c < m_columns; ++c)
		{
			if (counts[c] > 0)
			{
				m_used_cols = c + 1;
			}
		}
		m_overflow = n > m_columns * capacity;
		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < m_used_cols; ++c)
			{
				const int f = c * capacity + r;
				m_model_slots.push_back(r < counts[c] ? f : -1);
			}
		}
	}

	{
		// پروب موقت
		GtkAllocation a;
		gtk_widget_get_allocation(get_view()->get_widget(), &a);
		m_built_capacity = capacity;
		FILE* f = fopen("/tmp/wm-geo.log", "a");
		if (f) { fprintf(f, "layout: n=%d m_columns=%d capacity=%d used=%d slots=%d s0=%d s1=%d s2=%d s3=%d s4=%d s5=%d iw=%d alloc_h=%d win_alloc=%d win_geo=%d resizing=%d\n",
				n, m_columns, capacity, m_used_cols, (int)m_model_slots.size(),
				(m_model_slots.size()>0?m_model_slots[0]:-1),(m_model_slots.size()>1?m_model_slots[1]:-1),(m_model_slots.size()>2?m_model_slots[2]:-1),(m_model_slots.size()>3?m_model_slots[3]:-1),(m_model_slots.size()>4?m_model_slots[4]:-1),(m_model_slots.size()>5?m_model_slots[5]:-1),
				LauncherIconView::s_item_width, a.height,
				gtk_widget_get_allocated_height(GTK_WIDGET(get_window()->get_widget())),
				gtk_widget_get_allocated_width(GTK_WIDGET(get_window()->get_widget())),
				(int)get_window()->is_resizing()); fclose(f); }
	}
	// ساخت مدل — هر سلول دقیقاً یک ردیف تا m_model_slots با ردیف‌ها هم‌اندیس بماند
	GtkListStore* store = gtk_list_store_new(
			LauncherView::N_COLUMNS,
			G_TYPE_ICON,
			G_TYPE_STRING,
			G_TYPE_STRING,
			G_TYPE_POINTER);
	for (const int slot : m_model_slots)
	{
		Launcher* launcher = slot >= 0 ? get_window()->get_applications()->find(fav[slot]) : nullptr;
		if (launcher)
		{
			gtk_list_store_insert_with_values(
					store, nullptr, G_MAXINT,
					LauncherView::COLUMN_ICON, launcher->get_icon(),
					LauncherView::COLUMN_TEXT, launcher->get_text(),
					LauncherView::COLUMN_TOOLTIP, launcher->get_tooltip(),
					LauncherView::COLUMN_LAUNCHER, launcher,
					-1);
		}
		else
		{
			gtk_list_store_insert_with_values(
					store, nullptr, G_MAXINT,
					LauncherView::COLUMN_TEXT, "",
					-1);
		}
	}

	GtkTreeModel* model = GTK_TREE_MODEL(store);
	// layout فقط با ستون‌های دارای آیتم — ستون خالی وجود ندارد که نه دیده شود
	// و نه با arrow-right بشود داخلش رفت (خواستهٔ کاربر)
	gtk_icon_view_set_columns(GTK_ICON_VIEW(get_view()->get_widget()), m_used_cols);
	// حین درگ: اگر سرریز واقعی نیست، vscrollbar را بلافاصله مخفی نگه دار تا
	// در گپِ بین rebuild و allocate، پهنای آن به عرض اضافه نگردد (جهش عرض)
	if (get_window()->is_resizing() && !m_overflow)
	{
		gtk_widget_hide(gtk_scrolled_window_get_vscrollbar(
				GTK_SCROLLED_WINDOW(get_widget())));
	}
	// انتخاب overlay بودن scrollbar عمودی (تغییر از تنظیمات هم زنده اعمال شود)
	g_object_set(get_widget(), "overlay-scrolling",
			wm_settings->favorites_scrollbar_overlay ? TRUE : FALSE, nullptr);
	get_view()->set_model(model);
	// سلول‌های خالی (placeholder) قابل انتخاب/هاور نباشند — hover یا arrow-right
	// نباید آبی‌شان کند (خواستهٔ کاربر). هر انتخابی روی ردیفِ placeholder
	// فوراً خالی می‌شود؛ این برای hover-selection و کیبورد هر دو کار می‌کند
	g_signal_connect(get_view()->get_widget(), "selection-changed",
		G_CALLBACK(+[](GtkIconView* view, gpointer) -> void
		{
			GList* sel = gtk_icon_view_get_selected_items(view);
			if (!sel)
			{
				return;
			}
			GtkTreeModel* m = gtk_icon_view_get_model(view);
			gboolean any_real = FALSE;
			for (GList* l = sel; l; l = l->next)
			{
				GtkTreeIter it;
				if (gtk_tree_model_get_iter(m, &it, static_cast<GtkTreePath*>(l->data)))
				{
					WhiskerMenu::Element* e = nullptr;
					gtk_tree_model_get(m, &it, LauncherView::COLUMN_LAUNCHER, &e, -1);
					if (e)
					{
						any_real = TRUE;
					}
				}
			}
			g_list_free_full(sel, reinterpret_cast<GDestroyNotify>(&gtk_tree_path_free));
			if (!any_real)
			{
				gtk_icon_view_unselect_all(view);
			}
		}),
		nullptr);
	get_view()->set_width_authority();
	get_view()->update_item_width(true);

	connect(model, "row-changed",
		[this](GtkTreeModel* tree_model, GtkTreePath* path, GtkTreeIter* iter)
		{
			on_row_changed(tree_model, path, iter);
		});

	connect(model, "row-inserted",
		[this](GtkTreeModel* tree_model, GtkTreePath* path, GtkTreeIter* iter)
		{
			on_row_inserted(tree_model, path, iter);
		});

	connect(model, "row-deleted",
		[this](GtkTreeModel*, GtkTreePath* path)
		{
			on_row_deleted(path);
		});

	g_object_unref(model);
}

//-----------------------------------------------------------------------------

void FavoritesPage::unset_menu_items()
{
	// Clear treeview
	get_view()->unset_model();
}

//-----------------------------------------------------------------------------

void FavoritesPage::extend_context_menu(GtkWidget* menu)
{
	// sort خودکار صعودی جایگزین sort دستی شده؛ آیتمی به منو اضافه نمی‌شود
	(void)menu;
}

//-----------------------------------------------------------------------------

bool FavoritesPage::remember_launcher(Launcher* launcher)
{
	return wm_settings->favorites_in_recent ? true : !contains(launcher);
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_row_changed(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter)
{
	get_view()->update_item_width(true);

	// ردیفی که بعداً پر شده (مثل درگ از صفحهٔ اپ‌ها) به فهرست اضافه شود
	Element* element = nullptr;
	gtk_tree_model_get(model, iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	Launcher* launcher = dynamic_cast<Launcher*>(element);
	if (!launcher || contains(launcher))
	{
		return;
	}
	m_pending_add = launcher;
	schedule_resync();
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_row_inserted(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter)
{
	get_view()->update_item_width(true);

	Element* element = nullptr;
	gtk_tree_model_get(model, iter, LauncherView::COLUMN_LAUNCHER, &element, -1);
	Launcher* launcher = dynamic_cast<Launcher*>(element);
	if (!launcher || contains(launcher))
	{
		return;
	}
	m_pending_add = launcher;
	schedule_resync();
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_row_deleted(GtkTreePath* path)
{
	get_view()->update_item_width(true);

	// جای مدل را با نگاشت چیدمان به جای منطقی favorites برگردان
	const int pos = gtk_tree_path_get_indices(path)[0];
	if (pos < 0 || pos >= static_cast<int>(m_model_slots.size()))
	{
		return;
	}
	const int logical = m_model_slots[pos];
	if (logical < 0 || logical >= wm_settings->favorites.size())
	{
		return;
	}
	if (m_pending_remove < 0)
	{
		m_pending_remove = logical;
	}
	schedule_resync();
}

//-----------------------------------------------------------------------------

std::vector<Launcher*> FavoritesPage::sort() const
{
	std::vector<Launcher*> items;
	items.reserve(wm_settings->favorites.size());
	for (const auto& favorite : wm_settings->favorites)
	{
		Launcher* launcher = get_window()->get_applications()->find(favorite);
		if (!launcher)
		{
			continue;
		}
		items.push_back(launcher);
	}
	std::sort(items.begin(), items.end(), &Element::less_than);
	return items;
}

//-----------------------------------------------------------------------------

void FavoritesPage::sort_favorites()
{
	// sort خودکار صعودی بر اساس نام؛ شناسه‌های بدون لانچر حذف نمی‌شوند و ته لیست می‌مانند
	std::vector<Launcher*> items;
	items.reserve(wm_settings->favorites.size());
	StringList missing("", {});
	for (const auto& favorite : wm_settings->favorites)
	{
		Launcher* launcher = get_window()->get_applications()->find(favorite);
		if (launcher)
		{
			items.push_back(launcher);
		}
		else
		{
			missing.push_back(favorite);
		}
	}
	std::sort(items.begin(), items.end(), &Element::less_than);

	StringList& fav = wm_settings->favorites;
	fav.clear();
	for (auto launcher : items)
	{
		fav.push_back(launcher->get_desktop_id());
	}
	for (const auto& id : missing)
	{
		fav.push_back(id);
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::rebuild()
{
	// عرض منو فقط به دنبال تغییر واقعی محتوا/چیدمان sync می‌شود —
	// rebuild از مسیر show صدا زده نمی‌شود
	set_menu_items();
	get_window()->sync_width();
}

//-----------------------------------------------------------------------------

void FavoritesPage::on_height_changed()
{
	// تغییر ارتفاع منو = تغییر ظرفیت ستون‌ها → بازچینش؛ این هم نوعی «تغییر
	// محتوا» است و تنها جای مجاز برای تغییر توزیع پس از resize است
	rebuild();
}

//-----------------------------------------------------------------------------

void FavoritesPage::schedule_resync()
{
	if (m_resync_source)
	{
		return; // coalesce: idle معلق با آخرین وضعیت اجرا می‌شود
	}
	if (get_window()->is_resizing())
	{
		// حین درگ: بدون تأخیر — idle با اولویت بالا قبل از paint همان فریم اجرا
		// می‌شود؛ هر motion بعدی یا به idleِ معلق می‌رسد (همان از دادهٔ جدید
		// استفاده می‌کند) یا idle تازه می‌سازد. کنسلِ بازچینشِ نیمه‌کاره همین است.
		m_resync_source = g_idle_add_full(G_PRIORITY_HIGH_IDLE, +[](gpointer data) -> gint
		{
			auto page = static_cast<FavoritesPage*>(data);
			page->m_resync_source = 0;
			page->resync_now();
			return G_SOURCE_REMOVE;
		}, this, nullptr);
	}
	else
	{
		m_resync_source = g_timeout_add(400, +[](gpointer data) -> gint
		{
			auto page = static_cast<FavoritesPage*>(data);
			page->m_resync_source = 0;
			page->resync_now();
			return G_SOURCE_REMOVE;
		}, this);
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::sync_capacity()
{
	// بعد از pump در show: ویجت realize شده و فونت/CSS تم اعمال شده —
	// حالا عرض آیتم را با حقیقت رندر measure می‌کنیم (measure زودهنگامِ
	// قبل از realize فونت درست نداشت و عرض هر آیتم چند ده پیکسل بیش از
	// واقع می‌شد؛ مثلاً «Double Commander»). اگر عرض عوض شد، pitch هم
	// تازه و در صورت نیاز بازچینش — همه هنوز مخفی و نامرئی
	get_view()->update_item_width(true);
	// بعد از pump در show (تخصیص‌ها نهایی، هنوز مخفی): pitch واقعی را از خود
	// سلول اول می‌خوانیم (تخمین فرمولی ممکن است با رندر واقعی یکی دو باشد —
	// همین اختلاف ظرفیت را یکی جابه‌جا می‌کرد). اگر ظرفیت واقعی با آخرین
	// بازچینش فرق دارد، همین‌جا (نامرئی) بازچینش می‌شود. pitch اندازه‌گیری‌شده
	// در static می‌ماند و تخمین showهای بعدی را دقیق می‌کند.
	GtkAllocation a;
	gtk_widget_get_allocation(get_view()->get_widget(), &a);
	if (a.height <= 1)
	{
		return;
	}
	GtkTreePath* path = gtk_tree_path_new_from_indices(0, -1);
	GdkRectangle rect = {0, 0, 0, 0};
	if (gtk_icon_view_get_cell_rect(GTK_ICON_VIEW(get_view()->get_widget()), path, nullptr, &rect)
			&& (rect.height > 0))
	{
		gint rs = 0;
		g_object_get(G_OBJECT(get_view()->get_widget()), "row-spacing", &rs, nullptr);
		s_measured_pitch = rect.height + rs;
	}
	gtk_tree_path_free(path);
	const int cap = get_window()->favorites_capacity_rows();
	{
		GtkAllocation wa, va;
		gtk_widget_get_allocation(GTK_WIDGET(get_window()->get_widget()), &wa);
		gtk_widget_get_allocation(get_view()->get_widget(), &va);
		FILE* f = fopen("/tmp/wm-geo.log", "a");
		if (f)
		{
			fprintf(f, "sync_cap: menu_h(set)=%d win=%d view=%d chrome_win-view=%d pitch_meas=%d cap=%d built=%d scale=%d\n",
					(int)wm_settings->menu_height, wa.height, va.height,
					wa.height - va.height, FavoritesPage::s_measured_pitch,
					cap, m_built_capacity, gtk_widget_get_scale_factor(get_view()->get_widget()));
			fclose(f);
		}
	}
	if (cap != m_built_capacity)
	{
		resync_now();
	}
}

//-----------------------------------------------------------------------------

void FavoritesPage::resync_now()
{
	// حین درگ هم بازچینش زنده است (خواستهٔ کاربر: هر تغییر همان‌جا اعمال شود)
	if (get_window()->is_resizing())
	{
		get_window()->update_min_h();
	}
	if (m_pending_add)
	{
		// ترتیب نهایی را sort_favorites() در بازچینش تعیین می‌کند
		wm_settings->favorites.push_back(m_pending_add->get_desktop_id());
		m_pending_add = nullptr;
	}
	if (m_pending_remove >= 0)
	{
		if (m_pending_remove < wm_settings->favorites.size())
		{
			wm_settings->favorites.erase(m_pending_remove);
		}
		m_pending_remove = -1;
	}
	rebuild();
	// visibility scrollbar را هوک size-allocate حسابی و همان‌فریم مدیریت
	// می‌کند؛ خارج از درگ AUTOMATIC خودش تصمیم می‌گیرد
}

//-----------------------------------------------------------------------------

void FavoritesPage::view_created()
{
	// درگ برای افزودن/برداشتن فعال می‌ماند؛ ترتیب را sort خودکار تعیین می‌کند؛
	// ستون‌ها فقط دستی‌اند
	set_reorderable(true);

	// ردیابی DnD واقعی
	g_signal_connect(get_view()->get_widget(), "drag-begin",
		G_CALLBACK(+[](GtkWidget*, GdkDragContext*, gpointer) -> void
		{
			FavoritesPage::drag_in_progress = true;
		}), nullptr);
	g_signal_connect(get_view()->get_widget(), "drag-end",
		G_CALLBACK(+[](GtkWidget*, GdkDragContext*, gpointer) -> void
		{
			FavoritesPage::drag_in_progress = false;
		}), nullptr);

	// هیچ scroll افقی‌ای مجاز نیست: عرض منو همیشه برابر عرض محتواست
	GtkWidget* sw = get_widget();
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	// scrollbar عمودی overlay یا جاگیر — انتخاب کاربر
	g_object_set(sw, "overlay-scrolling",
			wm_settings->favorites_scrollbar_overlay ? TRUE : FALSE, nullptr);
	// حداقل ارتفاع ScrolledWindow را بشکن: پیش‌فرض GTK3 min = حداقلِ فرزند
	// (کل محتوای icon view) است؛ نتیجه‌اش این بود که با کوچک کردن ارتفاع،
	// تا وقتی بازچینش دوسطونه می‌نشیند پنجره به پایین رشد می‌کرد (=پیرپیر
	// پایین). با min_content_height=1 حداقل فقط خود scrollbar است و درگ آزادانه
	// کوچک می‌شود؛ سرریز واقعی → scrollbar (AUTOMATIC)
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(sw), 1);

	// بازچینش زنده حین درگ (خواستهٔ کاربر: با هر درگ، نه فقط رها کردن):
	// تغییر تخصیص ارتفاع حین درگ = تغییر ظرفیت → schedule_resync که حین درگ
	// با idleِ اولویت‌بالا (بدون تأخیر، coalesce) اجرا می‌شود. sync مستقیم داخل
	// size-allocate تجربه شد و مدل را وسط پاس تخصیص خراب می‌کند (لیست خالی شد)
	// — idle بین allocate و paint می‌نشیند و بازچینش قبل از رندر فریم تمام است.
	// خارج از درگ ظرفیت در لحظهٔ تغییر محتوا از menu_height محاسبه می‌شود.
	connect(get_view()->get_widget(), "size-allocate",
		[this](GtkWidget*, GdkRectangle*) -> gboolean
		{
			if (get_window()->is_resizing())
			{
				// تصمیم scrollbar حسابی و همان‌فریم: سرریز = n > ستون‌ها×ظرفیت
				// (از ارتفاع تخصیص‌یافته). بدون دست‌زدن به مدل.
				GtkTreeModel* model = gtk_icon_view_get_model(GTK_ICON_VIEW(get_view()->get_widget()));
				const int n = model ? gtk_tree_model_iter_n_children(model, nullptr) : 0;
				const int cols = (wm_settings->favorites_columns > 0) ? wm_settings->favorites_columns : 2;
				const int cap = get_window()->favorites_capacity_rows();
				const bool overflow = n > cols * cap;
				GtkWidget* vsb = gtk_scrolled_window_get_vscrollbar(GTK_SCROLLED_WINDOW(get_widget()));
				if (overflow)
				{
					gtk_widget_show(vsb);
				}
				else
				{
					gtk_widget_hide(vsb);
				}
				// بازچینش فقط با تغییر واقعی ظرفیت — نه هر فریمِ درگ. rebuildِ
				// بی‌دلیل مدل را خالی-پر می‌کرد = چشمک آیتم‌ها (پیرپیر آیتم‌ها)
				if (cap != m_built_capacity)
				{
					schedule_resync();
				}
			}
			return FALSE;
		});
}
//-----------------------------------------------------------------------------

