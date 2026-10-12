# whisker پچ‌شده 2.10.1 (دور ۲۲-۲۳)
سه تغییر نسبت به سورس اصلی (قابل مشاهده با diff -ru):
1. panel-plugin/profile.cpp  — const gint size = 48      (آواتار 48px)
2. panel-plugin/command.cpp  — GTK_ICON_SIZE_DIALOG      (دکمه‌های هدر 48px بدون تاری)
3. panel-plugin/launcher-icon-view.cpp — item_orientation(HORIZONTAL) +
   item_width(195) + set_columns(2)                       (لیستِ دوستونه چپ)
view-mode باید 0 باشد (xfconf /plugins/plugin-1/view-mode) تا این نمای لیستی دوستونه فعال شود.

بیلبورد:  meson setup build && ninja -C build
نصب:      sudo cp build/panel-plugin/libwhiskermenu.so /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/
بکاپ دبیان: libwhiskermenu.so.distro-2.10.1 کنار همان مسیر.
آپدیت apt باینری را برمی‌گرداند → دوباره بیلد و کپی.
دپس: libxfce4panel-2.0-dev libxfce4ui-2-dev libgarcon-1-dev libxfconf-0-dev libexo-2-dev meson ninja-build
