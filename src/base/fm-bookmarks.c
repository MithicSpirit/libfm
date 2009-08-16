/*
 *      fm-gtk-bookmarks.c
 *      
 *      Copyright 2009 PCMan <pcman@debian>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "fm-bookmarks.h"
#include <stdio.h>
#include <string.h>

enum
{
    CHANGED,
    N_SIGNALS
};

static FmBookmarks*	fm_bookmarks_new (void);

static void fm_bookmarks_finalize  			(GObject *object);
static GList* load_bookmarks(const char* fpath);
static void free_item(FmBookmarkItem* item);
static char* get_bookmarks_file();

G_DEFINE_TYPE(FmBookmarks, fm_bookmarks, G_TYPE_OBJECT);

static FmBookmarks* singleton = NULL;
static guint signals[N_SIGNALS];

static void fm_bookmarks_class_init(FmBookmarksClass *klass)
{
	GObjectClass *g_object_class;
	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = fm_bookmarks_finalize;

    signals[CHANGED] =
        g_signal_new("changed",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(FmBookmarksClass, changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0 );
}

static void fm_bookmarks_finalize(GObject *object)
{
	FmBookmarks *self;

	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_FM_BOOKMARKS(object));

	self = FM_BOOKMARKS(object);

    g_list_foreach(self->items, (GFunc)free_item, NULL);
    g_list_free(self->items);

    g_object_unref(self->mon);

	G_OBJECT_CLASS(fm_bookmarks_parent_class)->finalize(object);
}

char* get_bookmarks_file()
{
    return g_build_filename(g_get_home_dir(), ".gtk-bookmarks", NULL);
}

void free_item(FmBookmarkItem* item)
{
    g_debug("free: %s", item->name);
    if(item->name != item->path->name)
        g_free(item->name);
    fm_path_unref(item->path);
    g_slice_free(FmBookmarkItem, item);
}

static void on_changed( GFileMonitor* mon, GFile* gf, GFile* other, 
                    GFileMonitorEvent evt, FmBookmarks* bookmarks )
{
    char* fpath;
    /* reload bookmarks */
    g_list_foreach(bookmarks->items, (GFunc)free_item, NULL);
    g_list_free(bookmarks->items);

    fpath = get_bookmarks_file();
    bookmarks->items = load_bookmarks(fpath);
    g_free(fpath);
    g_signal_emit(bookmarks, signals[CHANGED], 0);
}

static FmBookmarkItem* new_item(char* line)
{
    FmBookmarkItem* item = g_slice_new0(FmBookmarkItem);
    char* sep;
    sep = strchr(line, '\n');
    if(sep)
        *sep = '\0';
    sep = strchr(line, ' ');
    if(sep)
        *sep = '\0';

    /* FIXME: this is no longer needed once fm_path_new can convert file:/// to / */
    if(g_str_has_prefix(line, "file:/"))
    {
        char* fpath = g_filename_from_uri(line, NULL, NULL);
        item->path = fm_path_new(fpath);
        g_free(fpath);
    }
    else
    {
        /* FIXME: is unescape needed? */
        item->path = fm_path_new(line);
    }

    if(sep)
        item->name = g_strdup(sep+1);
    else
        item->name = g_filename_display_name(item->path->name);

    return item;
}

GList* load_bookmarks(const char* fpath)
{
    FILE* f;
    char buf[1024];
    FmBookmarkItem* item;
    GList* items = NULL;

    /* load the file */
    if( f = fopen(fpath, "r") )
    {
        while(fgets(buf, 1024, f))
        {
            item = new_item(buf);
            items = g_list_prepend(items, item);
        }
    }
    items = g_list_reverse(items);
    return items;
}

static void fm_bookmarks_init(FmBookmarks *self)
{
    GList* items = NULL;
    char* fpath = get_bookmarks_file();
    GFile* gf = g_file_new_for_path(fpath);
	self->mon = g_file_monitor_file(gf, 0, NULL, NULL);
    g_object_unref(gf);
    g_signal_connect(self->mon, "changed", G_CALLBACK(on_changed), self);

    self->items = load_bookmarks(fpath);
    g_free(fpath);
}

FmBookmarks *fm_bookmarks_new(void)
{
	return g_object_new(FM_BOOKMARKS_TYPE, NULL);
}

FmBookmarks* fm_bookmarks_get(void)
{
    if( G_LIKELY(singleton) )
        g_object_ref(singleton);
    else
    {
        singleton = fm_bookmarks_new();
        g_object_add_weak_pointer(singleton, &singleton);
    }
    return singleton;
}

GList* fm_bookmarks_list_all(FmBookmarks* bookmarks)
{
    return bookmarks->items;
}
