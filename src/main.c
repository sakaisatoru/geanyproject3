/*
 * Geany プロジェクト一覧
 *
 * Copylight by Sakai Satoru 2018
 *
 * endeavor2wako@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define CONFIGFILE  "geany/geany.conf"
#define PROJECTNAME "_GEANYPROJECT_NAME"

/*
 * プロジェクトの情報
 */
typedef struct {
    gchar *name;            // プロジェクト名
    gchar *description;     // プロジェクトの説明
    gchar *prjfilename;     // プロジェクトファイルの絶対パス
    gchar *timestamp;       // プロジェクトファイルの最終更新日時
} Projectinfo;

Projectinfo *projectinfo_new ()
{
    Projectinfo *prj = g_malloc (sizeof(Projectinfo));
    if (prj != NULL) {
        prj->name = NULL;
        prj->description = NULL;
        prj->prjfilename = NULL;
        prj->timestamp = NULL;
    }
    return prj;
}

void projectinfo_free (Projectinfo *prj)
{
    if (prj != NULL) {
        g_free (prj->name);
        g_free (prj->description);
        g_free (prj->prjfilename);
        g_free (prj->timestamp);
        g_free (prj);
    }
}

void projectview_set_projectinfo (Projectinfo *prj);

/*
 * 指定したファイルからプロジェクトの情報を得る
 * 返されたProjectinfoは使用後開放すること。
 */
Projectinfo *project_read_info (gchar *file)
{
    GKeyFile *kprjconf;
    Projectinfo *prj;
    struct stat st;
    gchar buf[20];  // yyyy-mm-dd  hh:mm

    kprjconf = g_key_file_new ();
    prj = projectinfo_new ();
    if (g_key_file_load_from_file (
            kprjconf, file, G_KEY_FILE_NONE, NULL) == TRUE) {
        prj->name = g_strdup (g_key_file_get_string (kprjconf,
                                "project", "name", NULL));
        prj->description = g_strdup (g_key_file_get_string (kprjconf,
                                "project", "description", NULL));
        prj->prjfilename = g_strdup (file);
        lstat (prj->prjfilename, &st);
        strftime (buf, sizeof(buf), "%F  %R", localtime (&st.st_mtime));
        prj->timestamp = g_strdup (buf);
    }

    g_key_file_free (kprjconf);
    return prj;
}

/*
 * 指定されたディレクトリの中で [hoge].geanyファイルを探して表示関数に渡す
 */
void project_read_infofile (gchar *dir, gint level)
{
    GDir *project_dir;
    const gchar *file;
    gchar *path, *ext;
    Projectinfo *prj;
    gint i;

    /* サブディレクトリは1段しかチェックしない */
    if (level > 1) return;

    project_dir = g_dir_open (dir, 0, NULL);

    if (project_dir == NULL) {
        g_error (_("Fail to open directory."));
        return;
    }

    while ((file = g_dir_read_name (project_dir)) != NULL) {
        path = g_strdup_printf ("%s/%s", dir, file);
        if (g_file_test (path, G_FILE_TEST_IS_DIR) == TRUE) {
            // ディレクトリなのでその中に .geany がないか探す
            project_read_infofile (path, level+1);
        }
        else {
            /* 拡張子が geany なら内容を読む */
            ext = g_strrstr (file, ".geany");
            if (ext != NULL) {
                /* ファイル名の途中に.geanyが含まれている場合は扱わない */
                if (!g_strcmp0 (ext, ".geany")) {
                    prj = project_read_info (path);
                    projectview_set_projectinfo (prj);
                    projectinfo_free (prj);
                }
            }
        }
        g_free (path);
    }
    g_dir_close (project_dir);
}

/*
 * geany.conf を読んでプロジェクトの格納先を得る
 */
GKeyFile *load_geany_config (void)
{
    GKeyFile *kconf;
    gchar *conf_filename;

    conf_filename = g_build_filename (g_get_user_config_dir(), CONFIGFILE, NULL);
    kconf = g_key_file_new ();
    if (g_key_file_load_from_file (kconf, conf_filename,
            G_KEY_FILE_NONE, NULL) == FALSE) {
        g_key_file_load_from_data (
            kconf,
            "[project]\n"               \
            "session_file=\n"           \
            "project_file_path=.\n"     \
            "[geany]\n"                 \
            "pref_main_project_file_in_basedir=false\n",
            -1,
            G_KEY_FILE_NONE,
            NULL);
    }

    g_free (conf_filename);
    return kconf;
}

/*
 * UI
 */
static GtkWidget *ui;
static GtkListStore *projectlist;

void projectview_set_projectinfo (Projectinfo *prj)
{
    GtkTreeIter iter;
    if (prj == NULL) return;
    gtk_list_store_insert (projectlist, &iter, 0);
    gtk_list_store_set (projectlist, &iter,
                            0, prj->name,
                            1, prj->description,
                            2, prj->timestamp,
                            3, prj->prjfilename,
                            -1);
}

/*
 * UI に格納されたプロジェクトファイル名を引数にして geany を起動する
 * ダブル fork で このプログラム自体から切り離して起動する。
 */
static void launch_geany (GtkWidget *widget)
{
    GtkTreeSelection *selection;
    GtkTreeModel *store;
    GtkTreeIter iter;
    gchar *execprj;
    pid_t pid, pid_2;
    int status;

    if (widget != NULL) {
        // UIからプロジェクト名を得る
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(widget));
        store = gtk_tree_view_get_model (GTK_TREE_VIEW(widget));
        if (gtk_tree_selection_get_selected (selection, &store,
                                                    &iter) == TRUE) {
            gtk_tree_model_get (store, &iter, 3, &execprj, -1);
        }
    }
    else {
        execprj = NULL;
    }

    pid = fork ();
    if (pid == -1) {
        // エラー
        g_error ("起動に失敗しました。エラー番号 %d\n", errno);
        g_free (execprj);
    }
    else if (pid == 0) {
        // 子プロセス
        pid_2 = fork();
        if (pid_2 == -1) {
            // エラー
            g_error ("起動に失敗しました。エラー番号 %d\n", errno);
        }
        else if (pid_2 == 0) {
            if (execprj != NULL) {
                // 出来る限り g_free で開放したいので環境にコピーを取る
                if (!setenv (PROJECTNAME, execprj, !0)) {
                    g_free (execprj);
                    execprj = getenv (PROJECTNAME);
                }
            }
            execlp ("geany", "geany", "-i", execprj, (char*)NULL);
            // 戻ってきたら失敗
            g_error ("起動に失敗しました。エラー番号 %d\n", errno);
        }
        else {
            g_print ("孫プロセス %d を起動しました。\n", pid_2);
            g_free (execprj);
            exit (0);
        }
    }
    else {
        // 親プロセス
        g_print ("子プロセス %d を起動しました。\n", pid);
        waitpid (pid, &status, 0);
        g_free (execprj);
    }
}

static gboolean cb_button_press_event(GtkWidget *widget,
                                        GdkEventButton *event,
                                        gpointer data)
{
    // 左ボタン　ダブルクリック
    return (event->button == 1 && event->type == GDK_2BUTTON_PRESS) ?
        (launch_geany (widget), TRUE) :
        FALSE;
}

static gboolean cb_key_press_event (GtkWidget *widget,
                                    GdkEventKey *event,
                                    gpointer data)
{
    return (toupper (event->keyval) == GDK_KEY_Return) ?
        (launch_geany (widget), TRUE) :
        FALSE;
}

static void cb_size_request (GtkWidget *widget,
                GtkRequisition *requisition,
                gpointer        user_data)
{
    //~ カラムの表示幅変更を受けて、表示されているテキストの改行位置を変更する。
    //~ レンダラ及びカラムはGtkWidgetでないため、幅変更のシグナルを受け取れない（シグナル
    //~ そのものが発生しないのでEventBoxで囲うのも無意味）。なので、上位のTreeViewで
    //~ 受ける。
    //~ この場合、TreeViewが複数のカラムを持っていると、どのカラムで幅変更が生じたのか直接
    //~ 知る手段がない。今回は必要なカラムは一箇所だけなので固定値で参照している。
    //~ また、幅変更の結果、行の高さが大きく狂うことがあり、仕方ないので2行に固定している。
    //~ このほか、表示幅変更の影響を受けない行（セル）があったり、どうもGtkそのものにバグが
    //~ 残っているような感じがする。

    GList *list, *p;
    GtkTreeViewColumn *column;
    gint width, height, size;
    gdouble sizepoint;

    column = gtk_tree_view_get_column (GTK_TREE_VIEW(widget), 1);
    list = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT(column));
    for (p = list; p != NULL; p = p->next) {
        if (p->data != NULL) {
            // 新しい表示幅を設定
            g_object_set (p->data, "wrap-mode", PANGO_WRAP_WORD_CHAR, NULL);
            g_object_set (p->data, "wrap-width",
                gtk_tree_view_column_get_width (column), NULL);
            //~ レンダラのサイズから行高さを得ようと目論んだが、得られる数値はどうも
            //~ ピクセル値のようである。これだと、Pangoからこのレンダラで使っている
            //~ フォントに応じた1行分のピクセル数を得ないと、行数が求まらない。
            //~ gtk_cell_renderer_get_size (p->data, widget, NULL,
                //~ NULL, NULL, &width, &height);
            //~ g_object_get (p->data,
                                //~ "size", &size,
                                //~ "size-points", &sizepoint,
                                //~ NULL);
            //~ g_message ("x:%d, y:%d", width, height);
            //~ g_message ("size:%d size-points:%f", size, sizepoint);
            // セルの高さを行数で指定する。現在、2行で固定
            //~ gtk_cell_renderer_text_set_fixed_height_from_font (p->data, -1);
        }
    }
    g_list_free (list);
}

static GtkWidget *create_projectview (void)
{
    GtkWidget *view;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    projectlist = gtk_list_store_new (4,    G_TYPE_STRING,  // 名前
                                            G_TYPE_STRING,  // 説明
                                            G_TYPE_STRING,  // 変更日時
                                            G_TYPE_STRING); // ファイル名
    view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(projectlist));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(view), TRUE);

    //~ プロジェクトの名称
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (
                _("name"), renderer, "text", 0, NULL);
    gtk_tree_view_column_set_max_width (column, 200);
    g_object_set (column, "alignment", 0.5, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, 0);
    gtk_tree_view_append_column (GTK_TREE_VIEW(view), column);

    //~ 説明文の表示。長いものは折り返す。
    renderer = gtk_cell_renderer_text_new ();
    g_object_set (renderer, "wrap-width", 300, NULL);
    g_object_set (renderer, "wrap-mode", PANGO_WRAP_WORD_CHAR, NULL);
    // ここで表示行高さを指定しないと幅を変更したりソートした時におかしくなる
    gtk_cell_renderer_text_set_fixed_height_from_font (
                                GTK_CELL_RENDERER_TEXT(renderer), 2);
    column = gtk_tree_view_column_new_with_attributes (
                _("description"), renderer, "text", 1, NULL);
    gtk_tree_view_column_set_max_width (column, 300);
    g_object_set (column, "alignment", 0.5, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, 1);
    gtk_tree_view_append_column (GTK_TREE_VIEW(view), column);

    //~ 日時
    renderer = gtk_cell_renderer_text_new ();
    g_object_set (renderer, "xalign", 0.5, NULL);
    column = gtk_tree_view_column_new_with_attributes (
                _("mtime"), renderer, "text", 2, NULL);
    //~ gtk_tree_view_column_set_max_width (column, 160);
    g_object_set (column, "alignment", 0.5, NULL);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, 2);
    gtk_tree_view_append_column (GTK_TREE_VIEW(view), column);

    //~ コールバック　
    //~ GtkWidgetでないセルレンダラやカラムではシグナルの受信に難があるため、
    //~ TreeViewで受ける。
    g_signal_connect (view, "key-press-event",
                        G_CALLBACK (cb_key_press_event), NULL);
    g_signal_connect (view, "button-press-event",
                        G_CALLBACK (cb_button_press_event), NULL);
    //~ g_signal_connect (view, "size-request",
                        //~ G_CALLBACK (cb_size_request), NULL);
    return view;
}


static void cb_btnopen_clicked (GtkWidget *widget, GtkWidget *view)
{
    launch_geany (view);
}

static void cb_btnblank_clicked (GtkWidget *widget, GtkWidget *view)
{
    launch_geany (NULL);
}

GtkWidget *create_main_window (GtkApplication *app)
{
    GtkWidget *window, *header;
    GtkWidget *hbox;
    GtkWidget *pv, *sw;
    GtkWidget *btn_blank, *btn_open;

    window = gtk_application_window_new (app);

    // プロジェクトの一覧
    pv = create_projectview ();
    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(sw),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw),
        GTK_SHADOW_IN);
    gtk_container_add (GTK_CONTAINER(sw), pv);

    // ボタン
    btn_blank = gtk_button_new_from_icon_name ("document-new",
                                                GTK_ICON_SIZE_BUTTON);
    btn_open = gtk_button_new_from_icon_name ("document-open",
                                                GTK_ICON_SIZE_BUTTON);
    g_signal_connect (G_OBJECT(btn_blank), "clicked",
                        G_CALLBACK(cb_btnblank_clicked), pv);
    g_signal_connect (G_OBJECT(btn_open), "clicked",
                        G_CALLBACK(cb_btnopen_clicked), pv);


    // ヘッダーバー
    header = gtk_header_bar_new ();
    gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (header), "menu:close");
    gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
    gtk_header_bar_set_title (GTK_HEADER_BAR (header), PACKAGE);
    gtk_header_bar_pack_end (GTK_HEADER_BAR (header), btn_blank);
    gtk_header_bar_pack_end (GTK_HEADER_BAR (header), btn_open);

    // まとめ
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start (GTK_BOX(hbox), sw, TRUE, TRUE, 5);
    gtk_container_add (GTK_CONTAINER(window), hbox);
    gtk_window_set_titlebar (GTK_WINDOW (window), header);
    gtk_widget_set_size_request (window, 640, 480);
    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // 既定のディレクトリからプロジェクトファイルを読み込んで ui に格納する
    GKeyFile *kconf = load_geany_config ();
    gchar *prjpath = g_key_file_get_string (kconf, "project",
                "project_file_path", NULL);
    project_read_infofile (prjpath,
            g_key_file_get_boolean (kconf, "geany",
                "pref_main_project_file_in_basedir", NULL) ? 0 : 1);
    g_key_file_free (kconf);
    g_free (prjpath);

    return window;
}

static void
cb_activate_main (GtkApplication *app, gpointer userdata)
{
    GList *windows;
    g_message ("activate.");

    windows = gtk_application_get_windows (app);
    if (windows == NULL) {
        ui = create_main_window (app);
    }

    gtk_widget_show_all (ui);
    gtk_window_present (GTK_WINDOW(ui));
}

static void
cb_startup_main (GtkApplication *app, gpointer userdata)
{
    g_message ("start up.");
}

static void
cb_shutdown_main (GtkApplication *app, gpointer userdata)
{
    g_message ("shutdown.\n");
}

static void init_locale (void)
{
    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    textdomain (GETTEXT_PACKAGE);
}

int main (gint argc, gchar **argv)
{
    GtkApplication *app;
    int status;

    init_locale ();

    app = gtk_application_new ("com.gmail.endeavor2wako.Geanyproject",
                                            G_APPLICATION_FLAGS_NONE);
    g_signal_connect (app, "activate", G_CALLBACK(cb_activate_main), NULL);
    g_signal_connect (app, "startup",  G_CALLBACK(cb_startup_main),  NULL);
    g_signal_connect (app, "shutdown", G_CALLBACK(cb_shutdown_main), NULL);
    status = g_application_run (G_APPLICATION(app), argc, argv);
    g_object_unref (app);

    return status;
}
