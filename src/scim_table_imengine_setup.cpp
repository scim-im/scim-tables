/** @file scim_table_imengine_setup.cpp
 * implementation of Setup Module of table imengine module.
 */

/*
 * Smart Common Input Method
 *
 * Copyright (c) 2002-2005 James Su <suzhe@tsinghua.org.cn>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: scim_table_imengine_setup.cpp,v 1.4 2005/10/26 07:53:53 suzhe Exp $
 *
 */

#define Uses_SCIM_CONFIG_BASE

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gtk/scimkeyselection.h>
#include <scim.h>
#include "scim_generic_table.h"
#include "scim_table_private.h"

using namespace scim;

#define scim_module_init table_imengine_setup_LTX_scim_module_init
#define scim_module_exit table_imengine_setup_LTX_scim_module_exit

#define scim_setup_module_create_ui       table_imengine_setup_LTX_scim_setup_module_create_ui
#define scim_setup_module_get_category    table_imengine_setup_LTX_scim_setup_module_get_category
#define scim_setup_module_get_name        table_imengine_setup_LTX_scim_setup_module_get_name
#define scim_setup_module_get_description table_imengine_setup_LTX_scim_setup_module_get_description
#define scim_setup_module_load_config     table_imengine_setup_LTX_scim_setup_module_load_config
#define scim_setup_module_save_config     table_imengine_setup_LTX_scim_setup_module_save_config
#define scim_setup_module_query_changed   table_imengine_setup_LTX_scim_setup_module_query_changed


#define SCIM_CONFIG_IMENGINE_TABLE_FULL_WIDTH_PUNCT_KEY    "/IMEngine/Table/FullWidthPunctKey"
#define SCIM_CONFIG_IMENGINE_TABLE_FULL_WIDTH_LETTER_KEY   "/IMEngine/Table/FullWidthLetterKey"
#define SCIM_CONFIG_IMENGINE_TABLE_MODE_SWITCH_KEY         "/IMEngine/Table/ModeSwitchKey"
#define SCIM_CONFIG_IMENGINE_TABLE_ADD_PHRASE_KEY          "/IMEngine/Table/AddPhraseKey"
#define SCIM_CONFIG_IMENGINE_TABLE_DEL_PHRASE_KEY          "/IMEngine/Table/DeletePhraseKey"
#define SCIM_CONFIG_IMENGINE_TABLE_SHOW_PROMPT             "/IMEngine/Table/ShowPrompt"
#define SCIM_CONFIG_IMENGINE_TABLE_SHOW_KEY_HINT           "/IMEngine/Table/ShowKeyHint"
#define SCIM_CONFIG_IMENGINE_TABLE_USER_TABLE_BINARY       "/IMEngine/Table/UserTableBinary"
#define SCIM_CONFIG_IMENGINE_TABLE_USER_PHRASE_FIRST       "/IMEngine/Table/UserPhraseFirst"
#define SCIM_CONFIG_IMENGINE_TABLE_LONG_PHRASE_FIRST       "/IMEngine/Table/LongPhraseFirst"
#define SCIM_CONFIG_IMENGINE_TABLE_AUTO_RELOAD             "/IMEngine/Table/AutoReload"

#define SCIM_TABLE_ICON_FILE                              (SCIM_ICONDIR "/table.png")

#define LIST_ICON_SIZE 20

static GtkWidget * create_setup_window ();
static void        load_config (const ConfigPointer &config);
static void        save_config (const ConfigPointer &config);
static bool        query_changed ();

static void        destroy_all_tables ();

// Module Interface.
extern "C" {
    void scim_module_init (void)
    {
        bindtextdomain (GETTEXT_PACKAGE, SCIM_TABLE_LOCALEDIR);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    }

    void scim_module_exit (void)
    {
        destroy_all_tables ();
    }

    GtkWidget * scim_setup_module_create_ui (void)
    {
        return create_setup_window ();
    }

    String scim_setup_module_get_category (void)
    {
        return String ("IMEngine");
    }

    String scim_setup_module_get_name (void)
    {
        return String (_("Generic Table"));
    }

    String scim_setup_module_get_description (void)
    {
        return String (_("An IMEngine Module which uses generic table input method file."));
    }

    void scim_setup_module_load_config (const ConfigPointer &config)
    {
        load_config (config);
    }

    void scim_setup_module_save_config (const ConfigPointer &config)
    {
        save_config (config);
    }

    bool scim_setup_module_query_changed ()
    {
        return query_changed ();
    }
} // extern "C"

// Internal data structure
struct KeyboardConfigData
{
    const char *key;
    const char *label;
    const char *title;
    const char *tooltip;
    GtkWidget  *entry;
    GtkWidget  *button;
    String      data;
};

enum
{
    TABLE_COLUMN_ICON = 0,
    TABLE_COLUMN_NAME,
    TABLE_COLUMN_LANG,
    TABLE_COLUMN_FILE,
    TABLE_COLUMN_TYPE,
    TABLE_COLUMN_LIBRARY,
    TABLE_COLUMN_IS_USER,
    TABLE_NUM_COLUMNS
};

struct TablePropertiesData
{
    String name;
    String author;
    String uuid;
    String serial;
    String icon;
    String languages;
    String status_prompt;
    String valid_input_chars;
    String multi_wildcard_chars;
    String single_wildcard_chars;
    String split_keys;
    String commit_keys;
    String forward_keys;
    String select_keys;
    String page_up_keys;
    String page_down_keys;
    int    max_key_length;
    bool   show_key_prompt;
    bool   auto_select;
    bool   auto_fill;
    bool   auto_wildcard;
    bool   auto_commit;
    bool   auto_split;
    bool   discard_invalid_key;
    bool   dynamic_adjust;
    bool   always_show_lookup;
    bool   def_full_width_punct;
    bool   def_full_width_letter;
};

// Internal data declaration.
static bool __config_show_prompt           = false;
static bool __config_show_key_hint         = false;
static bool __config_user_table_binary     = false;
static bool __config_user_phrase_first     = false;
static bool __config_long_phrase_first     = false;
static bool __config_auto_reload           = false;

static bool __have_changed                 = false;

static GtkWidget    * __widget_show_prompt           = 0;
static GtkWidget    * __widget_show_key_hint         = 0;
static GtkWidget    * __widget_user_table_binary     = 0;
static GtkWidget    * __widget_user_phrase_first     = 0;
static GtkWidget    * __widget_long_phrase_first     = 0;
static GtkWidget    * __widget_auto_reload           = 0;

static GtkWidget    * __widget_table_list_view       = 0;
static GtkListStore * __widget_table_list_model      = 0;

static GtkWidget    * __widget_table_install_button    = 0;
static GtkWidget    * __widget_table_delete_button     = 0;
static GtkWidget    * __widget_table_properties_button = 0;

static KeyboardConfigData __config_keyboards [] =
{
    {
        // key
        SCIM_CONFIG_IMENGINE_TABLE_FULL_WIDTH_PUNCT_KEY,
        // label
        N_("Full width _punctuation:"),
        // title
        N_("Select full width puncutation keys"),
        // tooltip
        _("The key events to switch full/half width punctuation input mode. "
           "Click on the button on the right to edit it."),
        // entry
        NULL,
        // button
        NULL,
        // data
        "Control+period"
    },
    {
        // key
        SCIM_CONFIG_IMENGINE_TABLE_FULL_WIDTH_LETTER_KEY,
        // label
        N_("Full width _letter:"),
        // title
        N_("Select full width letter keys"),
        // tooltip
        _("The key events to switch full/half width letter input mode. "
           "Click on the button on the right to edit it."),
        // entry
        NULL,
        // button
        NULL,
        // data
        "Shift+space"
    },
    {
        // key
        SCIM_CONFIG_IMENGINE_TABLE_MODE_SWITCH_KEY,
        // label
        N_("_Mode switch:"),
        // title
        N_("Select mode switch keys"),
        // tooltip
        _("The key events to change current input mode. "
           "Click on the button on the right to edit it."),
        // entry
        NULL,
        // button
        NULL,
        // data
        "Alt+Shift_L+KeyRelease,"
        "Alt+Shift_R+KeyRelease,"
        "Shift+Shift_L+KeyRelease,"
        "Shift+Shift_R+KeyRelease"
    },
    {
        // key
        SCIM_CONFIG_IMENGINE_TABLE_ADD_PHRASE_KEY,
        // label
        N_("_Add phrase:"),
        // title
        N_("Select add phrase keys."),
        // tooltip
        _("The key events to add a new user defined phrase. "
           "Click on the button on the right to edit it."),
        // entry
        NULL,
        // button
        NULL,
        // data
        "Control+a,"
        "Control+equal"
    },
    {
        // key
        SCIM_CONFIG_IMENGINE_TABLE_DEL_PHRASE_KEY,
        // label
        N_("_Delete phrase:"),
        // title
        N_("Select delete phrase keys."),
        // tooltip
        _("The key events to delete a selected phrase. "
           "Click on the button on the right to edit it."),
        // entry
        NULL,
        // button
        NULL,
        // data
        "Control+d,"
        "Control+minus"
    },
    {
        // key
        NULL,
        // label
        NULL,
        // title
        NULL,
        // tooltip
        NULL,
        // entry
        NULL,
        // button
        NULL,
        // data
        ""
    },
};

// Declaration of internal functions.
static void
on_default_editable_changed          (GtkEditable     *editable,
                                      gpointer         user_data);

static void
on_default_toggle_button_toggled     (GtkCheckButton  *checkbutton,
                                      gpointer         user_data);

static void
on_default_key_selection_clicked     (GtkButton       *button,
                                      gpointer         user_data);

static void
on_icon_file_selection_clicked       (GtkButton       *button,
                                      gpointer         user_data);

static void
on_table_list_selection_changed      (GtkTreeSelection *selection,
                                      gpointer          user_data);

static void
on_table_install_clicked             (GtkButton       *button,
                                      gpointer         user_data);

static void
on_table_delete_clicked              (GtkButton       *button,
                                      gpointer         user_data);

static void
on_table_properties_clicked          (GtkButton       *button,
                                      gpointer         user_data);

static void
on_toggle_button_toggled             (GtkToggleButton *button,
                                      gpointer         user_data);

static void
run_table_properties_dialog          (GenericTableLibrary *lib,
                                      GtkTreeModel        *model,
                                      GtkTreeIter         *iter,
                                      const TablePropertiesData &data,
                                      bool                 editable);

static bool
validate_table_properties_data       (const GenericTableLibrary *lib,
                                      const TablePropertiesData &data);

static GdkPixbuf *
scale_pixbuf                         (GdkPixbuf      **pixbuf,
                                      int              width,
                                      int              height);

static void
setup_widget_value ();

static GtkWidget *
create_generic_page ();

static GtkWidget *
create_keyboard_page ();

static GtkWidget *
create_table_management_page ();

static GtkListStore *
create_table_list_model ();

static void
get_table_list (std::vector<String> &table_list, const String &path);

static GenericTableLibrary *
load_table_file (const String &file);

static void
add_table_to_list (GenericTableLibrary *table, const String &dir, const String &file, bool user);

static void
delete_table_from_list (GtkTreeModel *model, GtkTreeIter *iter);

static void
load_all_tables ();

static void
save_all_tables ();

static bool
test_file_modify (const String &file);

static bool
test_file_unlink (const String &file);

// Helpers for asynchronous dialogs (GTK4 removed gtk_dialog_run).
static GtkWindow *
get_setup_window ()
{
    if (__widget_table_list_view) {
        GtkRoot *root = gtk_widget_get_root (__widget_table_list_view);
        if (root && GTK_IS_WINDOW (root))
            return GTK_WINDOW (root);
    }
    return NULL;
}

// Show a simple, non-blocking informational/error message.
static void
show_message (const gchar *msg)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new ("%s", msg);
    gtk_alert_dialog_show (dialog, get_setup_window ());
    g_object_unref (dialog);
}

// Asynchronous OK/Cancel confirmation.
typedef void (*ConfirmCallback) (gpointer user_data, bool ok);

struct ConfirmData {
    ConfirmCallback  cb;
    gpointer         user_data;
};

static void
confirm_alert_finish (GObject *source, GAsyncResult *result, gpointer data)
{
    ConfirmData *cd = static_cast<ConfirmData *> (data);
    int button = gtk_alert_dialog_choose_finish (GTK_ALERT_DIALOG (source), result, NULL);

    // Button index 1 is OK, everything else (including cancel/close) is not OK.
    bool ok = (button == 1);

    if (cd->cb)
        cd->cb (cd->user_data, ok);

    delete cd;
}

static void
show_confirm (const gchar *msg, ConfirmCallback cb, gpointer user_data)
{
    GtkAlertDialog *dialog = gtk_alert_dialog_new ("%s", msg);
    const char *buttons[] = { _("_Cancel"), _("_OK"), NULL };

    gtk_alert_dialog_set_buttons (dialog, buttons);
    gtk_alert_dialog_set_cancel_button (dialog, 0);
    gtk_alert_dialog_set_default_button (dialog, 1);

    ConfirmData *cd = new ConfirmData;
    cd->cb        = cb;
    cd->user_data = user_data;

    gtk_alert_dialog_choose (dialog, get_setup_window (), NULL, confirm_alert_finish, cd);
    g_object_unref (dialog);
}

// Function implementations.
static GtkWidget *
create_generic_page ()
{
    GtkWidget *vbox;

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    __widget_show_prompt = gtk_check_button_new_with_mnemonic (_("Show _prompt"));
    gtk_widget_set_margin_start  (__widget_show_prompt, 4);
    gtk_widget_set_margin_end    (__widget_show_prompt, 4);
    gtk_widget_set_margin_top    (__widget_show_prompt, 4);
    gtk_widget_set_margin_bottom (__widget_show_prompt, 4);
    gtk_box_append (GTK_BOX (vbox), __widget_show_prompt);

    __widget_show_key_hint = gtk_check_button_new_with_mnemonic (_("Show key _hint"));
    gtk_widget_set_margin_start  (__widget_show_key_hint, 4);
    gtk_widget_set_margin_end    (__widget_show_key_hint, 4);
    gtk_widget_set_margin_top    (__widget_show_key_hint, 4);
    gtk_widget_set_margin_bottom (__widget_show_key_hint, 4);
    gtk_box_append (GTK_BOX (vbox), __widget_show_key_hint);

    __widget_user_table_binary = gtk_check_button_new_with_mnemonic (_("Save _user table in binary format"));
    gtk_widget_set_margin_start  (__widget_user_table_binary, 4);
    gtk_widget_set_margin_end    (__widget_user_table_binary, 4);
    gtk_widget_set_margin_top    (__widget_user_table_binary, 4);
    gtk_widget_set_margin_bottom (__widget_user_table_binary, 4);
    gtk_box_append (GTK_BOX (vbox), __widget_user_table_binary);

    __widget_user_phrase_first = gtk_check_button_new_with_mnemonic (_("Show the u_ser defined phrases first"));
    gtk_widget_set_margin_start  (__widget_user_phrase_first, 4);
    gtk_widget_set_margin_end    (__widget_user_phrase_first, 4);
    gtk_widget_set_margin_top    (__widget_user_phrase_first, 4);
    gtk_widget_set_margin_bottom (__widget_user_phrase_first, 4);
    gtk_box_append (GTK_BOX (vbox), __widget_user_phrase_first);

    __widget_long_phrase_first = gtk_check_button_new_with_mnemonic (_("Show the _longer phrases first"));
    gtk_widget_set_margin_start  (__widget_long_phrase_first, 4);
    gtk_widget_set_margin_end    (__widget_long_phrase_first, 4);
    gtk_widget_set_margin_top    (__widget_long_phrase_first, 4);
    gtk_widget_set_margin_bottom (__widget_long_phrase_first, 4);
    gtk_box_append (GTK_BOX (vbox), __widget_long_phrase_first);

    __widget_auto_reload = gtk_check_button_new_with_mnemonic (_("_Reload the table when its file changes"));
    gtk_widget_set_margin_start  (__widget_auto_reload, 4);
    gtk_widget_set_margin_end    (__widget_auto_reload, 4);
    gtk_widget_set_margin_top    (__widget_auto_reload, 4);
    gtk_widget_set_margin_bottom (__widget_auto_reload, 4);
    gtk_box_append (GTK_BOX (vbox), __widget_auto_reload);

    // Connect all signals.
    g_signal_connect ((gpointer) __widget_show_prompt, "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      &__config_show_prompt);
    g_signal_connect ((gpointer) __widget_show_key_hint, "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      &__config_show_key_hint);
    g_signal_connect ((gpointer) __widget_user_table_binary, "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      &__config_user_table_binary);
    g_signal_connect ((gpointer) __widget_user_phrase_first, "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      &__config_user_phrase_first);
    g_signal_connect ((gpointer) __widget_long_phrase_first, "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      &__config_long_phrase_first);
    g_signal_connect ((gpointer) __widget_auto_reload, "toggled",
                      G_CALLBACK (on_default_toggle_button_toggled),
                      &__config_auto_reload);

    // Set all tooltips.
    const gchar *show_prompt_tooltip =
        _("If this option is checked, "
          "the key prompt of the currently selected phrase "
          "will be shown.");
    const gchar *show_key_hint_tooltip =
        _("If this option is checked, "
          "the remaining keystrokes of the phrases "
          "will be shown on the lookup table.");
    const gchar *user_table_binary_tooltip =
        _("If this option is checked, "
          "the user table will be stored with binary format, "
          "this will increase the loading speed.");
    const gchar *user_phrase_first_tooltip =
        _("If this option is checked, "
          "the user defined phrases will be shown "
          "in front of others. ");
    const gchar *long_phrase_first_tooltip =
        _("If this option is checked, "
          "the longer phrase will be shown "
          "in front of others. ");
    const gchar *auto_reload_tooltip =
        _("If this option is checked, "
          "a table whose file has been modified on disk "
          "will be reloaded automatically the next time a text field is focused.");
    gtk_widget_set_tooltip_text (__widget_show_prompt, show_prompt_tooltip);
    gtk_widget_set_tooltip_text (__widget_show_key_hint, show_key_hint_tooltip);
    gtk_widget_set_tooltip_text (__widget_user_table_binary, user_table_binary_tooltip);
    gtk_widget_set_tooltip_text (__widget_user_phrase_first, user_phrase_first_tooltip);
    gtk_widget_set_tooltip_text (__widget_long_phrase_first, long_phrase_first_tooltip);
    gtk_widget_set_tooltip_text (__widget_auto_reload, auto_reload_tooltip);

    return vbox;
}

static GtkWidget *
create_keyboard_page ()
{
    GtkWidget *table;
    GtkWidget *label;

    int i;

    table = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (table), 4);
    gtk_grid_set_column_spacing (GTK_GRID (table), 4);

    // Create keyboard setting.
    for (i = 0; __config_keyboards [i].key; ++ i) {
        label = gtk_label_new (NULL);
        gtk_label_set_text_with_mnemonic (GTK_LABEL (label), _(__config_keyboards[i].label));
        gtk_widget_set_halign (label, GTK_ALIGN_END);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        gtk_grid_attach (GTK_GRID (table), label, 0, i, 1, 1);

        __config_keyboards [i].entry = gtk_entry_new ();
        gtk_widget_set_hexpand (__config_keyboards [i].entry, TRUE);
        gtk_widget_set_halign (__config_keyboards [i].entry, GTK_ALIGN_FILL);
        gtk_grid_attach (GTK_GRID (table), __config_keyboards [i].entry, 1, i, 1, 1);

        gtk_editable_set_editable (GTK_EDITABLE (__config_keyboards[i].entry), FALSE);

        __config_keyboards[i].button = gtk_button_new_with_label ("...");
        gtk_grid_attach (GTK_GRID (table), __config_keyboards [i].button, 2, i, 1, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), __config_keyboards[i].button);
    }

    for (i = 0; __config_keyboards [i].key; ++ i) {
        g_signal_connect ((gpointer) __config_keyboards [i].button, "clicked",
                          G_CALLBACK (on_default_key_selection_clicked),
                          &(__config_keyboards [i]));
        g_signal_connect ((gpointer) __config_keyboards [i].entry, "changed",
                          G_CALLBACK (on_default_editable_changed),
                          &(__config_keyboards [i].data));
    }

    for (i = 0; __config_keyboards [i].key; ++ i) {
        gtk_widget_set_tooltip_text (__config_keyboards [i].entry,
                              __config_keyboards [i].tooltip);
    }

    return table;
}

static GtkListStore *
create_table_list_model ()
{
    GtkListStore *model;

    model = gtk_list_store_new (TABLE_NUM_COLUMNS,
                                GDK_TYPE_PIXBUF,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_POINTER,
                                G_TYPE_BOOLEAN);

    return model;
}

static GtkWidget *
create_table_management_page ()
{
    GtkWidget *page;
    GtkWidget *vbox;
    GtkWidget *label;
    GtkWidget *scrolledwindow;
    GtkWidget *hbox;
    GtkWidget *button;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection  *selection;

    page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

    label = gtk_label_new (_("The installed tables:"));
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start  (label, 2);
    gtk_widget_set_margin_end    (label, 2);
    gtk_widget_set_margin_top    (label, 2);
    gtk_widget_set_margin_bottom (label, 2);
    gtk_box_append (GTK_BOX (page), label);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand (hbox, TRUE);
    gtk_box_append (GTK_BOX (page), hbox);

    scrolledwindow = gtk_scrolled_window_new ();
    gtk_widget_set_hexpand (scrolledwindow, TRUE);
    gtk_widget_set_vexpand (scrolledwindow, TRUE);
    gtk_box_append (GTK_BOX (hbox), scrolledwindow);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

    // Create table list view
    __widget_table_list_model = create_table_list_model ();
    __widget_table_list_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (__widget_table_list_model));
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (__widget_table_list_view), TRUE);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), __widget_table_list_view);

    // Create name column
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, TABLE_COLUMN_NAME);

    gtk_tree_view_column_set_title (column, _("Name"));

    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "pixbuf", TABLE_COLUMN_ICON, NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "text", TABLE_COLUMN_NAME, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (__widget_table_list_view), column);

    // Create lang column
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, TABLE_COLUMN_LANG);

    gtk_tree_view_column_set_title (column, _("Language"));

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "text", TABLE_COLUMN_LANG, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (__widget_table_list_view), column);

    // Create type column.
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, TABLE_COLUMN_TYPE);

    gtk_tree_view_column_set_title (column, _("Type"));

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "text", TABLE_COLUMN_TYPE, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (__widget_table_list_view), column);

    // Create file column.
    column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_reorderable (column, TRUE);
    gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_sort_column_id (column, TABLE_COLUMN_FILE);

    gtk_tree_view_column_set_title (column, _("File"));

    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer,
                                         "text", TABLE_COLUMN_FILE, NULL);

    gtk_tree_view_append_column (GTK_TREE_VIEW (__widget_table_list_view), column);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (__widget_table_list_view));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

    g_signal_connect (G_OBJECT (selection), "changed",
                      G_CALLBACK (on_table_list_selection_changed),
                      0);

    // Create buttons.
    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (vbox, 4);
    gtk_box_append (GTK_BOX (hbox), vbox);

    button = gtk_button_new_with_mnemonic (_("_Install"));
    gtk_widget_set_margin_start  (button, 2);
    gtk_widget_set_margin_end    (button, 2);
    gtk_widget_set_margin_top    (button, 2);
    gtk_widget_set_margin_bottom (button, 2);
    gtk_box_append (GTK_BOX (vbox), button);

    const gchar *button_insert_tooltip = _("Install a new table.");
    gtk_widget_set_tooltip_text (button, button_insert_tooltip);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_table_install_clicked),
                      0);
    __widget_table_install_button = button;

    button = gtk_button_new_with_mnemonic (_("_Delete"));
    gtk_widget_set_margin_start  (button, 2);
    gtk_widget_set_margin_end    (button, 2);
    gtk_widget_set_margin_top    (button, 2);
    gtk_widget_set_margin_bottom (button, 2);
    gtk_box_append (GTK_BOX (vbox), button);
    const gchar *button_delete_tooltip = _("Delete the selected table.");
    gtk_widget_set_tooltip_text (button, button_delete_tooltip);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_table_delete_clicked),
                      0);
    __widget_table_delete_button = button;

    button = gtk_button_new_with_mnemonic (_("_Properties"));
    gtk_widget_set_margin_start  (button, 2);
    gtk_widget_set_margin_end    (button, 2);
    gtk_widget_set_margin_top    (button, 2);
    gtk_widget_set_margin_bottom (button, 2);
    gtk_box_append (GTK_BOX (vbox), button);
    const gchar *button_edit_tooltip = _("Edit the properties of the selected table.");
    gtk_widget_set_tooltip_text (button, button_edit_tooltip);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_table_properties_clicked),
                      0);
    __widget_table_properties_button = button;

    return page;
}

static GtkWidget *
create_setup_window ()
{
    static GtkWidget *window = 0;

    if (!window) {
        GtkWidget *notebook;
        GtkWidget *label;
        GtkWidget *page;

        // Create the Notebook.
        notebook = gtk_notebook_new ();

        // Create the first page.
        page = create_generic_page ();
        label = gtk_label_new (_("Generic"));
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the second page.
        page = create_keyboard_page ();
        label = gtk_label_new (_("Keyboard"));
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        // Create the third page.
        page = create_table_management_page ();
        label = gtk_label_new (_("Table Management"));
        gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, label);

        window = notebook;

        setup_widget_value ();
    }

    return window;
}

void
setup_widget_value ()
{
    if (__widget_show_prompt) {
        gtk_check_button_set_active (
            GTK_CHECK_BUTTON (__widget_show_prompt),
            __config_show_prompt);
    }

    if (__widget_show_key_hint) {
        gtk_check_button_set_active (
            GTK_CHECK_BUTTON (__widget_show_key_hint),
            __config_show_key_hint);
    }

    if (__widget_user_table_binary) {
        gtk_check_button_set_active (
            GTK_CHECK_BUTTON (__widget_user_table_binary),
            __config_user_table_binary);
    }

    if (__widget_user_phrase_first) {
        gtk_check_button_set_active (
            GTK_CHECK_BUTTON (__widget_user_phrase_first),
            __config_user_phrase_first);
    }

    if (__widget_long_phrase_first) {
        gtk_check_button_set_active (
            GTK_CHECK_BUTTON (__widget_long_phrase_first),
            __config_long_phrase_first);
    }

    if (__widget_auto_reload) {
        gtk_check_button_set_active (
            GTK_CHECK_BUTTON (__widget_auto_reload),
            __config_auto_reload);
    }

    for (int i = 0; __config_keyboards [i].key; ++ i) {
        if (__config_keyboards [i].entry) {
            gtk_editable_set_text (
                GTK_EDITABLE (__config_keyboards [i].entry),
                __config_keyboards [i].data.c_str ());
        }
    }

}

void
load_config (const ConfigPointer &config)
{
    if (!config.null ()) {
        __config_show_prompt =
            config->read (String (SCIM_CONFIG_IMENGINE_TABLE_SHOW_PROMPT),
                          __config_show_prompt);
        __config_show_key_hint =
            config->read (String (SCIM_CONFIG_IMENGINE_TABLE_SHOW_KEY_HINT),
                          __config_show_key_hint);
        __config_user_table_binary =
            config->read (String (SCIM_CONFIG_IMENGINE_TABLE_USER_TABLE_BINARY),
                          __config_user_table_binary);
        __config_user_phrase_first =
            config->read (String (SCIM_CONFIG_IMENGINE_TABLE_USER_PHRASE_FIRST),
                          __config_user_phrase_first);
        __config_long_phrase_first =
            config->read (String (SCIM_CONFIG_IMENGINE_TABLE_LONG_PHRASE_FIRST),
                          __config_long_phrase_first);
        __config_auto_reload =
            config->read (String (SCIM_CONFIG_IMENGINE_TABLE_AUTO_RELOAD),
                          __config_auto_reload);

        for (int i = 0; __config_keyboards [i].key; ++ i) {
            __config_keyboards [i].data =
                config->read (String (__config_keyboards [i].key),
                              __config_keyboards [i].data);
        }

        setup_widget_value ();

        load_all_tables ();

        __have_changed = false;
    }
}

void
save_config (const ConfigPointer &config)
{
    if (!config.null ()) {
        config->write (String (SCIM_CONFIG_IMENGINE_TABLE_SHOW_PROMPT),
                        __config_show_prompt);
        config->write (String (SCIM_CONFIG_IMENGINE_TABLE_SHOW_KEY_HINT),
                        __config_show_key_hint);
        config->write (String (SCIM_CONFIG_IMENGINE_TABLE_USER_TABLE_BINARY),
                       __config_user_table_binary);
        config->write (String (SCIM_CONFIG_IMENGINE_TABLE_USER_PHRASE_FIRST),
                       __config_user_phrase_first);
        config->write (String (SCIM_CONFIG_IMENGINE_TABLE_LONG_PHRASE_FIRST),
                       __config_long_phrase_first);
        config->write (String (SCIM_CONFIG_IMENGINE_TABLE_AUTO_RELOAD),
                       __config_auto_reload);

        for (int i = 0; __config_keyboards [i].key; ++ i) {
            config->write (String (__config_keyboards [i].key),
                          __config_keyboards [i].data);
        }

        save_all_tables ();

        __have_changed = false;
    }
}

bool
query_changed ()
{
    if (__have_changed)
        return true;

    GtkTreeIter iter;
    if (__widget_table_list_model &&
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (__widget_table_list_model), &iter)) {

        GenericTableLibrary *lib;

        do {
            gtk_tree_model_get (GTK_TREE_MODEL (__widget_table_list_model), &iter,
                                TABLE_COLUMN_LIBRARY, &lib,
                                -1);
            if (lib->updated ())
                return true;

        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (__widget_table_list_model), &iter));
    }
    return false;
}

static void
on_default_editable_changed (GtkEditable *editable,
                             gpointer     user_data)
{
    String *str = static_cast <String *> (user_data);

    if (str) {
        *str = String (gtk_editable_get_text (GTK_EDITABLE (editable)));
        __have_changed = true;
    }
}

static void
on_default_toggle_button_toggled (GtkCheckButton *checkbutton,
                                  gpointer        user_data)
{
    bool *toggle = static_cast<bool*> (user_data);

    if (toggle) {
        *toggle = gtk_check_button_get_active (checkbutton);
        __have_changed = true;
    }
}

// Payload for the asynchronous key-selection dialog.
struct KeySelectionDialogData {
    GtkWidget *entry;
};

static void
key_selection_dialog_response_cb (GtkDialog *dialog, gint response, gpointer user_data)
{
    KeySelectionDialogData *d = static_cast<KeySelectionDialogData *> (user_data);

    if (response == GTK_RESPONSE_OK) {
        const gchar *keys = scim_key_selection_dialog_get_keys (
                        SCIM_KEY_SELECTION_DIALOG (dialog));

        if (!keys) keys = "";

        if (strcmp (keys, gtk_editable_get_text (GTK_EDITABLE (d->entry))) != 0)
            gtk_editable_set_text (GTK_EDITABLE (d->entry), keys);
    }

    delete d;
    gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
on_default_key_selection_clicked (GtkButton *button,
                                  gpointer   user_data)
{
    KeyboardConfigData *data = static_cast <KeyboardConfigData *> (user_data);

    if (data) {
        GtkWidget *dialog = scim_key_selection_dialog_new (_(data->title));

        scim_key_selection_dialog_set_keys (
            SCIM_KEY_SELECTION_DIALOG (dialog),
            gtk_editable_get_text (GTK_EDITABLE (data->entry)));

        GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (button));
        if (root && GTK_IS_WINDOW (root))
            gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (root));
        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        KeySelectionDialogData *d = new KeySelectionDialogData;
        d->entry = data->entry;

        g_signal_connect (dialog, "response",
                          G_CALLBACK (key_selection_dialog_response_cb), d);

        gtk_window_present (GTK_WINDOW (dialog));
    }
}

static void
icon_file_dialog_finish_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    GtkEntry      *entry  = static_cast <GtkEntry*> (user_data);

    GFile *file = gtk_file_dialog_open_finish (dialog, result, NULL);

    if (file) {
        char *path = g_file_get_path (file);
        if (path) {
            gtk_editable_set_text (GTK_EDITABLE (entry), path);
            g_free (path);
        }
        g_object_unref (file);
    }
}

static void
on_icon_file_selection_clicked (GtkButton *button,
                                gpointer   user_data)
{
    GtkEntry *entry = static_cast <GtkEntry*> (user_data);

    if (entry) {
        GtkFileDialog *dialog = gtk_file_dialog_new ();
        gtk_file_dialog_set_title (dialog, _("Select an icon file"));

        const char *cur = gtk_editable_get_text (GTK_EDITABLE (entry));
        if (cur && *cur) {
            GFile *f = g_file_new_for_path (cur);
            gtk_file_dialog_set_initial_file (dialog, f);
            g_object_unref (f);
        }

        GtkRoot   *root   = gtk_widget_get_root (GTK_WIDGET (button));
        GtkWindow *parent = (root && GTK_IS_WINDOW (root)) ? GTK_WINDOW (root) : NULL;

        gtk_file_dialog_open (dialog, parent, NULL, icon_file_dialog_finish_cb, entry);
        g_object_unref (dialog);
    }
}

// Table manager related functions.
static bool
test_file_modify (const String &file)
{
    if (access (file.c_str (), W_OK) != 0 && errno != ENOENT)
        return false;

    return true;
}

static bool
test_file_unlink (const String &file)
{
    String path;
    String::size_type pos = file.rfind (SCIM_PATH_DELIM);

    if (pos != String::npos) path = file.substr (0, pos);

    if (!path.length ()) path = SCIM_PATH_DELIM_STRING;

    if (access (path.c_str (), W_OK) != 0)
        return false;

    return true;
}

static void
get_table_list (std::vector<String> &table_list, const String &path)
{
    table_list.clear ();

    DIR *dir = opendir (path.c_str ());
    if (dir != NULL) {
        struct dirent *file = readdir (dir);
        while (file != NULL) {
            struct stat filestat;
            String absfn = path + SCIM_PATH_DELIM_STRING + file->d_name;
            stat (absfn.c_str (), &filestat);

            if (S_ISREG (filestat.st_mode))
                table_list.push_back (absfn);

            file = readdir (dir);
        }
        closedir (dir);
    }
}

static GdkPixbuf *
scale_pixbuf (GdkPixbuf **pixbuf,
               int         width,
               int         height)
{
    if (pixbuf && *pixbuf) {
        if (gdk_pixbuf_get_width (*pixbuf) != width ||
            gdk_pixbuf_get_height (*pixbuf) != height) {
            GdkPixbuf *dest = gdk_pixbuf_scale_simple (*pixbuf, width, height, GDK_INTERP_BILINEAR);
            g_object_unref (*pixbuf);
            *pixbuf = dest;
        }
        return *pixbuf;
    }
    return 0;
}

static GenericTableLibrary *
load_table_file (const String &file)
{
    GenericTableLibrary *library = 0;

    if (file.length ()) {
        library = new GenericTableLibrary ();
        if (!library->init (file, "", "", true)) {
            delete library;
            library = 0;
        }
    }

    return library;
}

static void
add_table_to_list (GenericTableLibrary *table, const String &dir, const String &file, bool user)
{
    if (!table || !table->valid () || !__widget_table_list_model) return;

    GtkTreeIter iter;
    GdkPixbuf   *pixbuf;
    String      name;
    String      lang;

    pixbuf = gdk_pixbuf_new_from_file (table->get_icon_file ().c_str (), NULL);

    if (!pixbuf) {
        pixbuf = gdk_pixbuf_new_from_file (SCIM_TABLE_ICON_FILE, NULL);
    }

    scale_pixbuf (&pixbuf, LIST_ICON_SIZE, LIST_ICON_SIZE);

    name = utf8_wcstombs (table->get_name (scim_get_current_locale ()));
    lang = scim_get_language_name (table->get_language ());

    gtk_list_store_append (__widget_table_list_model, &iter);

    gtk_list_store_set (__widget_table_list_model, &iter,
                        TABLE_COLUMN_ICON, pixbuf,
                        TABLE_COLUMN_NAME, name.c_str (),
                        TABLE_COLUMN_LANG, lang.c_str (),
                        TABLE_COLUMN_FILE, file.c_str (),
                        TABLE_COLUMN_TYPE, user ? _("User") : _("System"),
                        TABLE_COLUMN_LIBRARY, table,
                        TABLE_COLUMN_IS_USER, user,
                        -1);

    if (pixbuf)
        g_object_unref (pixbuf);
}

static void
load_all_tables ()
{
    if (!__widget_table_list_model) return;

    std::vector<String> usr_tables;
    std::vector<String> sys_tables;
    std::vector<String>::iterator it;
    GenericTableLibrary *library;

    String sys_dir (SCIM_TABLE_SYSTEM_TABLE_DIR);
    String usr_dir (scim_get_user_data_dir () + SCIM_TABLE_USER_TABLE_DIR);

    destroy_all_tables ();

    get_table_list (sys_tables, sys_dir);
    get_table_list (usr_tables, usr_dir);

    for (it = sys_tables.begin (); it != sys_tables.end (); ++it) {
        if ((library = load_table_file (*it)) != 0)
            add_table_to_list (library, sys_dir, *it, false);
    }

    for (it = usr_tables.begin (); it != usr_tables.end (); ++it) {
        if ((library = load_table_file (*it)) != 0)
            add_table_to_list (library, usr_dir, *it, true);
    }
}

static gboolean
table_list_destroy_iter_func (GtkTreeModel *model,
                              GtkTreePath  *path,
                              GtkTreeIter  *iter,
                              gpointer      data)
{
    GenericTableLibrary *library;
    gtk_tree_model_get (model, iter, TABLE_COLUMN_LIBRARY, &library, -1);

    if (library) {
        delete library;
        gtk_list_store_set (GTK_LIST_STORE (model), iter, TABLE_COLUMN_LIBRARY, NULL, -1);
    }

    return FALSE;
}

static void
delete_table_from_list (GtkTreeModel *model, GtkTreeIter *iter)
{
    if (model && iter) {
        table_list_destroy_iter_func (model, 0, iter, 0);
        gtk_list_store_remove (GTK_LIST_STORE (model), iter);
    }
}

static void
destroy_all_tables ()
{
    if (__widget_table_list_model) {
        gtk_tree_model_foreach (GTK_TREE_MODEL (__widget_table_list_model),
                                table_list_destroy_iter_func,
                                0);
        gtk_list_store_clear (__widget_table_list_model);
    }
}

static void
on_table_list_selection_changed (GtkTreeSelection *selection,
                                 gpointer          user_data)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gchar        *file = 0;
    bool          can_unlink;

    if (__widget_table_delete_button) {
        if (gtk_tree_selection_get_selected (selection, &model, &iter))
            gtk_tree_model_get (model, &iter,
                                TABLE_COLUMN_FILE, &file,
                                -1);

        if (file) {
            can_unlink = test_file_unlink (file);
            g_free (file);
        } else {
            can_unlink = false;
        }

        gtk_widget_set_sensitive (__widget_table_delete_button, can_unlink);
    }
}

static bool find_table_in_list_by_file (const String &file, GtkTreeIter *iter_found)
{
    GtkTreeIter iter;

    if (__widget_table_list_model &&
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (__widget_table_list_model), &iter)) {
        do {
            gchar *fn;
            gtk_tree_model_get (GTK_TREE_MODEL (__widget_table_list_model), &iter,
                                TABLE_COLUMN_FILE, &fn,
                                -1);
            if (String (fn) == file) {
                g_free (fn);

                if (iter_found)
                    *iter_found = iter;

                return true;
            }
            g_free (fn);
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (__widget_table_list_model), &iter));
    }
    return false;
}

static bool find_table_in_list_by_library (GenericTableLibrary *library, GtkTreeIter *iter_found)
{
    GtkTreeIter iter;

    if (__widget_table_list_model && library &&
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (__widget_table_list_model), &iter)) {
        do {
            GenericTableLibrary *lib;

            gtk_tree_model_get (GTK_TREE_MODEL (__widget_table_list_model), &iter,
                                TABLE_COLUMN_LIBRARY, &lib,
                                -1);

            if (lib && lib->get_uuid () == library->get_uuid ()) {
                if (iter_found)
                    *iter_found = iter;
                return true;
            }
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (__widget_table_list_model), &iter));
    }
    return false;
}

// Asynchronous "install table" flow.
//
// The original synchronous flow was: pick a file, then possibly show a
// sequence of confirmation dialogs before actually saving the table.  Since
// GTK4 removed gtk_dialog_run, the flow is broken into a chain of callbacks
// that carry state through this context structure.
struct InstallContext {
    GenericTableLibrary *library;
    String               new_file;
    String               path;
    bool                 user_table;
    String               sys_dir;
    String               usr_dir;
};

static void install_check_uuid (InstallContext *ctx);
static void install_check_file (InstallContext *ctx);
static void install_do_save    (InstallContext *ctx);

static void
install_do_save (InstallContext *ctx)
{
    if (!scim_make_dir (ctx->path) ||
        !ctx->library->save (ctx->new_file, "", "", __config_user_table_binary)) {
        char buf [1024];
        snprintf (buf, sizeof (buf),
                  _("Failed to install the table to %s!"),
                  ctx->new_file.c_str ());
        show_message (buf);

        delete ctx->library;
        delete ctx;
        return;
    }

    add_table_to_list (ctx->library, ctx->path, ctx->new_file, ctx->user_table);

    // Ownership of the library was transferred to the list model.
    delete ctx;
}

static void
install_overwrite_confirm_cb (gpointer user_data, bool ok)
{
    InstallContext *ctx = static_cast<InstallContext *> (user_data);

    if (!ok) {
        delete ctx->library;
        delete ctx;
        return;
    }

    GtkTreeIter iter;
    if (find_table_in_list_by_file (ctx->new_file, &iter))
        delete_table_from_list (GTK_TREE_MODEL (__widget_table_list_model), &iter);

    install_do_save (ctx);
}

static void
install_check_file (InstallContext *ctx)
{
    GtkTreeIter iter;

    // Find if the file is already existed.
    if (find_table_in_list_by_file (ctx->new_file, &iter)) {
        if (!test_file_modify (ctx->new_file)) {
            show_message (_("Failed to install the table! "
                            "A table with the same file name was already installed."));
            delete ctx->library;
            delete ctx;
            return;
        }

        show_confirm (_("A table with the same file name was already installed. "
                        "Do you want to overwrite it?"),
                      install_overwrite_confirm_cb, ctx);
        return;
    }

    install_do_save (ctx);
}

static void
install_replace_confirm_cb (gpointer user_data, bool ok)
{
    InstallContext *ctx = static_cast<InstallContext *> (user_data);

    if (!ok) {
        delete ctx->library;
        delete ctx;
        return;
    }

    GtkTreeIter iter;
    if (find_table_in_list_by_library (ctx->library, &iter))
        delete_table_from_list (GTK_TREE_MODEL (__widget_table_list_model), &iter);

    String::size_type pos = ctx->new_file.rfind (SCIM_PATH_DELIM);
    if (pos != String::npos && pos != 0) ctx->path = ctx->new_file.substr (0, pos);
    else ctx->path = SCIM_PATH_DELIM_STRING;

    if (ctx->path == ctx->sys_dir) ctx->user_table = false;

    install_check_file (ctx);
}

static void
install_check_uuid (InstallContext *ctx)
{
    GtkTreeIter iter;

    // Find if there is a table with same uuid was already installed.
    if (find_table_in_list_by_library (ctx->library, &iter)) {
        gchar *fn;

        gtk_tree_model_get (GTK_TREE_MODEL (__widget_table_list_model), &iter,
                            TABLE_COLUMN_FILE, &fn, -1);
        ctx->new_file = String (fn);
        g_free (fn);

        if (!test_file_modify (ctx->new_file)) {
            show_message (_("Failed to install the table! "
                            "Another version of this table was already installed."));
            delete ctx->library;
            delete ctx;
            return;
        }

        show_confirm (_("Another version of this table was already installed. "
                        "Do you want to replace it with the new one?"),
                      install_replace_confirm_cb, ctx);
        return;
    }

    install_check_file (ctx);
}

static void
install_file_dialog_finish_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);

    GFile *gf = gtk_file_dialog_open_finish (dialog, result, NULL);
    if (!gf)
        return;

    char *fp = g_file_get_path (gf);
    String file = fp ? String (fp) : String ();
    if (fp) g_free (fp);
    g_object_unref (gf);

    if (!file.length ())
        return;

    String new_file;
    String path;
    String::size_type pos;
    GenericTableLibrary *library;

    String sys_dir (SCIM_TABLE_SYSTEM_TABLE_DIR);
    String usr_dir (scim_get_user_data_dir () + SCIM_TABLE_USER_TABLE_DIR);

    pos = file.rfind (SCIM_PATH_DELIM);

    new_file = usr_dir + SCIM_PATH_DELIM_STRING;

    // Check if the file is already in the table directories.
    if (pos != String::npos) {
        path = file.substr (0, pos);
        if (!path.length ()) path = SCIM_PATH_DELIM_STRING;

        if (path == sys_dir || path == usr_dir) {
            show_message (_("Failed to install the table! "
                            "It's already in table file directory."));
            return;
        }
        new_file += file.substr (pos + 1);
    } else {
        new_file += file;
    }

    path = usr_dir;

    // Load the table into memory.
    if ((library = load_table_file (file)) == 0) {
        show_message (_("Failed to load the table file!"));
        return;
    }

    InstallContext *ctx = new InstallContext;
    ctx->library    = library;
    ctx->new_file   = new_file;
    ctx->path       = path;
    ctx->user_table = true;
    ctx->sys_dir    = sys_dir;
    ctx->usr_dir    = usr_dir;

    install_check_uuid (ctx);
}

static void
on_table_install_clicked (GtkButton *button,
                          gpointer   user_data)
{
    // Select the table file.
    GtkFileDialog *dialog = gtk_file_dialog_new ();
    gtk_file_dialog_set_title (dialog, _("Please select the table file to be installed."));

    gtk_file_dialog_open (dialog, get_setup_window (), NULL,
                          install_file_dialog_finish_cb, NULL);
    g_object_unref (dialog);
}

// Asynchronous "delete table" flow.
struct DeleteContext {
    GtkTreeModel *model;
    GtkTreeIter   iter;
    String        file;
};

static void
delete_confirm_cb (gpointer user_data, bool ok)
{
    DeleteContext *ctx = static_cast<DeleteContext *> (user_data);

    if (ok) {
        if (unlink (ctx->file.c_str ()) != 0) {
            show_message (_("Failed to delete the table file!"));
        } else {
            delete_table_from_list (ctx->model, &ctx->iter);
        }
    }

    delete ctx;
}

static void
on_table_delete_clicked (GtkButton *button,
                         gpointer   user_data)
{
    GtkTreeIter  iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (__widget_table_list_view));

    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
        gchar *fn;
        String file;

        gtk_tree_model_get (model, &iter,
                            TABLE_COLUMN_FILE, &fn, -1);
        file = String (fn);
        g_free (fn);

        if (!test_file_unlink (file)) {
            char buf [1024];
            snprintf (buf, sizeof (buf),
                      _("Can not delete the file %s!"),
                      file.c_str ());
            show_message (buf);
            return;
        }

        DeleteContext *ctx = new DeleteContext;
        ctx->model = model;
        ctx->iter  = iter;
        ctx->file  = file;

        show_confirm (_("Are you sure to delete this table file?"),
                      delete_confirm_cb, ctx);
    }
}

static void
on_toggle_button_toggled (GtkToggleButton *button,
                          gpointer         user_data)
{
    if (gtk_toggle_button_get_active (button))
        gtk_button_set_label (GTK_BUTTON (button), _("True"));
    else
        gtk_button_set_label (GTK_BUTTON (button), _("False"));
}

// Context carried through the asynchronous table properties dialog.
struct TablePropertiesDialogData {
    GtkWidget           *dialog;
    GenericTableLibrary *lib;
    GtkTreeModel        *model;
    GtkTreeIter          iter;
    TablePropertiesData  data;
    TablePropertiesData  olddata;
    bool                 editable;

    GtkWidget *entry_name;
    GtkWidget *entry_author;
    GtkWidget *entry_uuid;
    GtkWidget *entry_serial;
    GtkWidget *entry_icon;
    GtkWidget *button_icon;
    GtkWidget *entry_languages;
    GtkWidget *entry_status_prompt;
    GtkWidget *entry_valid_input_chars;
    GtkWidget *entry_multi_wildcard_chars;
    GtkWidget *entry_single_wildcard_chars;
    GtkWidget *spin_max_key_length;
    GtkWidget *toggle_dynamic_adjust;
    GtkWidget *toggle_always_show_lookup;
    GtkWidget *toggle_show_key_prompt;
    GtkWidget *toggle_auto_select;
    GtkWidget *toggle_auto_fill;
    GtkWidget *toggle_auto_wildcard;
    GtkWidget *toggle_auto_commit;
    GtkWidget *toggle_auto_split;
    GtkWidget *toggle_discard_invalid_key;
    GtkWidget *toggle_def_full_width_punct;
    GtkWidget *toggle_def_full_width_letter;

    KeyboardConfigData split_keys;
    KeyboardConfigData commit_keys;
    KeyboardConfigData forward_keys;
    KeyboardConfigData select_keys;
    KeyboardConfigData page_up_keys;
    KeyboardConfigData page_down_keys;
};

static GtkWidget *
prop_dialog_add_label (GtkWidget *grid, const char *text, int row)
{
    GtkWidget *label = gtk_label_new (text);
    gtk_widget_set_halign (label, GTK_ALIGN_END);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
    return label;
}

// Apply the (validated) properties back to the library.
static void
apply_table_properties (TablePropertiesDialogData *pd)
{
    GenericTableLibrary *lib     = pd->lib;
    GtkTreeModel        *model   = pd->model;
    GtkTreeIter          iter    = pd->iter;
    TablePropertiesData &data    = pd->data;
    TablePropertiesData &olddata = pd->olddata;

    std::vector <KeyEvent> keyevents;

    if (data.icon != olddata.icon) {
        GdkPixbuf * pixbuf = gdk_pixbuf_new_from_file (data.icon.c_str (), NULL);
        scale_pixbuf (&pixbuf, LIST_ICON_SIZE, LIST_ICON_SIZE);

        gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                            TABLE_COLUMN_ICON, pixbuf,
                            -1);

        if (pixbuf)
            g_object_unref (pixbuf);

        lib->set_icon_file (data.icon);
    }

    if (data.languages != olddata.languages)
        lib->set_languages (data.languages);

    if (data.status_prompt != olddata.status_prompt)
        lib->set_status_prompt (utf8_mbstowcs (data.status_prompt));

    if (data.single_wildcard_chars != olddata.single_wildcard_chars)
        lib->set_single_wildcard_chars (data.single_wildcard_chars);

    if (data.multi_wildcard_chars != olddata.multi_wildcard_chars)
        lib->set_multi_wildcard_chars (data.multi_wildcard_chars);

    if (data.max_key_length != olddata.max_key_length)
        lib->set_max_key_length (data.max_key_length);

    if (data.show_key_prompt != olddata.show_key_prompt)
        lib->set_show_key_prompt (data.show_key_prompt);

    if (data.auto_select != olddata.auto_select)
        lib->set_auto_select (data.auto_select);

    if (data.auto_fill != olddata.auto_fill)
        lib->set_auto_fill (data.auto_fill);

    if (data.auto_wildcard != olddata.auto_wildcard)
        lib->set_auto_wildcard (data.auto_wildcard);

    if (data.auto_commit != olddata.auto_commit)
        lib->set_auto_commit (data.auto_commit);

    if (data.auto_split != olddata.auto_split)
        lib->set_auto_split (data.auto_split);

    if (data.discard_invalid_key != olddata.discard_invalid_key)
        lib->set_discard_invalid_key (data.discard_invalid_key);

    if (data.dynamic_adjust != olddata.dynamic_adjust)
        lib->set_dynamic_adjust (data.dynamic_adjust);

    if (data.always_show_lookup != olddata.always_show_lookup)
        lib->set_always_show_lookup (data.always_show_lookup);

    if (data.def_full_width_punct != olddata.def_full_width_punct)
        lib->set_def_full_width_punct (data.def_full_width_punct);

    if (data.def_full_width_letter != olddata.def_full_width_letter)
        lib->set_def_full_width_letter (data.def_full_width_letter);

    if (data.split_keys != olddata.split_keys &&
        scim_string_to_key_list (keyevents, data.split_keys))
        lib->set_split_keys (keyevents);

    if (data.commit_keys != olddata.commit_keys &&
        scim_string_to_key_list (keyevents, data.commit_keys))
        lib->set_commit_keys (keyevents);

    if (data.forward_keys != olddata.forward_keys &&
        scim_string_to_key_list (keyevents, data.forward_keys))
        lib->set_forward_keys (keyevents);

    if (data.select_keys != olddata.select_keys &&
        scim_string_to_key_list (keyevents, data.select_keys))
        lib->set_select_keys (keyevents);

    if (data.page_up_keys != olddata.page_up_keys &&
        scim_string_to_key_list (keyevents, data.page_up_keys))
        lib->set_page_up_keys (keyevents);

    if (data.page_down_keys != olddata.page_down_keys &&
        scim_string_to_key_list (keyevents, data.page_down_keys))
        lib->set_page_down_keys (keyevents);
}

static void
table_properties_dialog_response_cb (GtkDialog *dialog, gint response, gpointer user_data)
{
    TablePropertiesDialogData *pd = static_cast<TablePropertiesDialogData *> (user_data);

    if (response != GTK_RESPONSE_OK) {
        gtk_window_destroy (GTK_WINDOW (dialog));
        delete pd;
        return;
    }

    TablePropertiesData &data = pd->data;

    data.icon = String (gtk_editable_get_text (GTK_EDITABLE (pd->entry_icon)));
    data.languages = String (gtk_editable_get_text (GTK_EDITABLE (pd->entry_languages)));
    data.status_prompt = String (gtk_editable_get_text (GTK_EDITABLE (pd->entry_status_prompt)));
    data.multi_wildcard_chars  = String (gtk_editable_get_text (GTK_EDITABLE (pd->entry_multi_wildcard_chars)));
    data.single_wildcard_chars = String (gtk_editable_get_text (GTK_EDITABLE (pd->entry_single_wildcard_chars)));
    data.split_keys = String (gtk_editable_get_text (GTK_EDITABLE (pd->split_keys.entry)));
    data.commit_keys = String (gtk_editable_get_text (GTK_EDITABLE (pd->commit_keys.entry)));
    data.forward_keys = String (gtk_editable_get_text (GTK_EDITABLE (pd->forward_keys.entry)));
    data.select_keys = String (gtk_editable_get_text (GTK_EDITABLE (pd->select_keys.entry)));
    data.page_up_keys = String (gtk_editable_get_text (GTK_EDITABLE (pd->page_up_keys.entry)));
    data.page_down_keys = String (gtk_editable_get_text (GTK_EDITABLE (pd->page_down_keys.entry)));

    data.max_key_length = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (pd->spin_max_key_length));
    data.show_key_prompt = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_show_key_prompt));
    data.auto_select = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_select));
    data.auto_fill = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_fill));
    data.auto_wildcard = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_wildcard));
    data.auto_commit = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_commit));
    data.auto_split = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_split));
    data.discard_invalid_key = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_discard_invalid_key));
    data.dynamic_adjust = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_dynamic_adjust));
    data.always_show_lookup = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_always_show_lookup));
    data.def_full_width_punct = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_def_full_width_punct));
    data.def_full_width_letter = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (pd->toggle_def_full_width_letter));

    // If the data is invalid, keep the dialog open so the user can correct it
    // (validate_table_properties_data shows the error message itself).
    if (!validate_table_properties_data (pd->lib, data))
        return;

    apply_table_properties (pd);

    gtk_window_destroy (GTK_WINDOW (dialog));
    delete pd;
}

static void
run_table_properties_dialog (GenericTableLibrary *lib,
                             GtkTreeModel        *model,
                             GtkTreeIter         *iter,
                             const TablePropertiesData &data,
                             bool                 editable)
{
    GtkWidget *dialog;
    GtkWidget *dialog_vbox;
    GtkWidget *scrolledwindow;
    GtkWidget *table;
    GtkWidget *hbox;

    TablePropertiesDialogData *pd = new TablePropertiesDialogData;
    pd->lib      = lib;
    pd->model    = model;
    pd->iter     = *iter;
    pd->data     = data;
    pd->olddata  = data;
    pd->editable = editable;

    pd->split_keys     = {NULL, _("Split Keys:"), _("Split Keys:"),
                          _("The key strokes to split inputted string."),
                          NULL, NULL, ""};
    pd->commit_keys    = {NULL, _("Commit Keys:"), _("Commit Keys:"),
                          _("The key strokes to commit converted result to client."),
                          NULL, NULL, ""};
    pd->forward_keys   = {NULL, _("Forward Keys:"), _("Forward Keys:"),
                          _("The key strokes to forward inputted string to client."),
                          NULL, NULL, ""};
    pd->select_keys    = {NULL, _("Select Keys:"), _("Select Keys:"),
                          _("The key strokes to select candidate phrases in lookup table."),
                          NULL, NULL, ""};
    pd->page_up_keys   = {NULL, _("Page Up Keys:"), _("Page Up Keys:"),
                          _("The lookup table page up keys"),
                          NULL, NULL, ""};
    pd->page_down_keys = {NULL, _("Page Down Keys:"), _("Page Down Keys:"),
                          _("The lookup table page down keys"),
                          NULL, NULL, ""};

    KeyboardConfigData *all_keys [] = {
        &pd->split_keys,
        &pd->commit_keys,
        &pd->forward_keys,
        &pd->select_keys,
        &pd->page_up_keys,
        &pd->page_down_keys,
        NULL
    };

    int row = 0;

    // Create dialog.
    dialog = gtk_dialog_new_with_buttons (_("Table Properties"),
                                          get_setup_window (),
                                          GTK_DIALOG_MODAL,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("_OK"), GTK_RESPONSE_OK,
                                          NULL);
    pd->dialog = dialog;

    dialog_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    scrolledwindow = gtk_scrolled_window_new ();
    gtk_widget_set_vexpand (scrolledwindow, TRUE);
    gtk_box_append (GTK_BOX (dialog_vbox), scrolledwindow);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

    table = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (table), 2);
    gtk_grid_set_column_spacing (GTK_GRID (table), 2);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scrolledwindow), table);

    // Name
    prop_dialog_add_label (table, _("Name:"), row);
    pd->entry_name = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_name, TRUE);
    gtk_widget_set_halign (pd->entry_name, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_name, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_name, _("The name of this table."));
    ++ row;

    // Author
    prop_dialog_add_label (table, _("Author:"), row);
    pd->entry_author = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_author, TRUE);
    gtk_widget_set_halign (pd->entry_author, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_author, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_author, _("The author of this table."));
    ++ row;

    // UUID
    prop_dialog_add_label (table, _("UUID:"), row);
    pd->entry_uuid = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_uuid, TRUE);
    gtk_widget_set_halign (pd->entry_uuid, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_uuid, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_uuid, _("The unique ID of this table."));
    ++ row;

    // Serial Number
    prop_dialog_add_label (table, _("Serial Number:"), row);
    pd->entry_serial = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_serial, TRUE);
    gtk_widget_set_halign (pd->entry_serial, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_serial, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_serial, _("The serial number of this table."));
    ++ row;

    // Icon file
    prop_dialog_add_label (table, _("Icon File:"), row);
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand (hbox, TRUE);
    gtk_widget_set_halign (hbox, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), hbox, 1, row, 1, 1);

    pd->entry_icon = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_icon, TRUE);
    gtk_box_append (GTK_BOX (hbox), pd->entry_icon);

    pd->button_icon = gtk_button_new_with_mnemonic (_("Browse"));
    gtk_box_append (GTK_BOX (hbox), pd->button_icon);

    g_signal_connect (G_OBJECT (pd->button_icon), "clicked",
                      G_CALLBACK (on_icon_file_selection_clicked),
                      pd->entry_icon);

    gtk_widget_set_tooltip_text (pd->entry_icon, _("The icon file of this table."));
    ++ row;

    // Supported Languages
    prop_dialog_add_label (table, _("Supported Languages:"), row);
    pd->entry_languages = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_languages, TRUE);
    gtk_widget_set_halign (pd->entry_languages, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_languages, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_languages, _("The languages supported by this table."));
    ++ row;

    // Status Prompts
    prop_dialog_add_label (table, _("Status Prompt:"), row);
    pd->entry_status_prompt = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_status_prompt, TRUE);
    gtk_widget_set_halign (pd->entry_status_prompt, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_status_prompt, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_status_prompt, _("A prompt string to be shown in status area."));
    ++ row;

    // Valid Input Chars
    prop_dialog_add_label (table, _("Valid Input Chars:"), row);
    pd->entry_valid_input_chars = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_valid_input_chars, TRUE);
    gtk_widget_set_halign (pd->entry_valid_input_chars, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_valid_input_chars, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_valid_input_chars, _("The valid input chars of this table."));
    ++ row;

    // Mulit Wildcard Char
    prop_dialog_add_label (table, _("Multi Wildcard Char:"), row);
    pd->entry_multi_wildcard_chars = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_multi_wildcard_chars, TRUE);
    gtk_widget_set_halign (pd->entry_multi_wildcard_chars, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_multi_wildcard_chars, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_multi_wildcard_chars,
        _("The multi wildcard chars of this table. "
          "These chars can be used to match one or more arbitrary chars."));
    ++ row;

    // Single Wildcard Char
    prop_dialog_add_label (table, _("Single Wildcard Char:"), row);
    pd->entry_single_wildcard_chars = gtk_entry_new ();
    gtk_widget_set_hexpand (pd->entry_single_wildcard_chars, TRUE);
    gtk_widget_set_halign (pd->entry_single_wildcard_chars, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->entry_single_wildcard_chars, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->entry_single_wildcard_chars,
        _("The single wildcard chars of this table."
          "These chars can be used to match one arbitrary char."));
    ++ row;

    // All keyboard settings
    for (int i = 0; all_keys [i]; ++i) {
        prop_dialog_add_label (table, all_keys [i]->label, row);

        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_hexpand (hbox, TRUE);
        gtk_widget_set_halign (hbox, GTK_ALIGN_FILL);
        gtk_grid_attach (GTK_GRID (table), hbox, 1, row, 1, 1);

        all_keys [i]->entry = gtk_entry_new ();
        gtk_widget_set_hexpand (all_keys [i]->entry, TRUE);
        gtk_box_append (GTK_BOX (hbox), all_keys [i]->entry);

        all_keys [i]->button = gtk_button_new_with_label (_("..."));
        gtk_box_append (GTK_BOX (hbox), all_keys [i]->button);

        g_signal_connect ((gpointer) all_keys [i]->button, "clicked",
                          G_CALLBACK (on_default_key_selection_clicked),
                          all_keys [i]);

        gtk_widget_set_tooltip_text (all_keys [i]->entry, all_keys [i]->tooltip);

        ++ row;
    }

    // Max key length
    prop_dialog_add_label (table, _("Max Key Length:"), row);
    pd->spin_max_key_length = gtk_spin_button_new_with_range (1, SCIM_GT_MAX_KEY_LENGTH, 1);
    gtk_spin_button_set_digits (GTK_SPIN_BUTTON (pd->spin_max_key_length), 0);
    gtk_widget_set_hexpand (pd->spin_max_key_length, TRUE);
    gtk_widget_set_halign (pd->spin_max_key_length, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->spin_max_key_length, 1, row, 1, 1);
    gtk_widget_set_tooltip_text (pd->spin_max_key_length, _("The maxmium length of key strings."));
    ++ row;

    // Show key prompt.
    prop_dialog_add_label (table, _("Show Key Prompt:"), row);
    pd->toggle_show_key_prompt = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_show_key_prompt, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_show_key_prompt, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_show_key_prompt), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_show_key_prompt), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_show_key_prompt,
        _("If true then the key prompts will be shown "
          "instead of the raw keys."));
    ++ row;

    // Auto Select
    prop_dialog_add_label (table, _("Auto Select:"), row);
    pd->toggle_auto_select = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_auto_select, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_auto_select, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_select), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_auto_select), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_auto_select,
        _("If true then the first candidate phrase will be "
          "selected automatically when inputing the next key."));
    ++ row;

    // Auto Wildcard
    prop_dialog_add_label (table, _("Auto Wildcard:"), row);
    pd->toggle_auto_wildcard = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_auto_wildcard, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_auto_wildcard, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_wildcard), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_auto_wildcard), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_auto_wildcard,
        _("If true then a multi wildcard char will be appended to "
          "the end of the inputted key string when searching phrases."));
    ++ row;

    // Auto Commit
    prop_dialog_add_label (table, _("Auto Commit:"), row);
    pd->toggle_auto_commit = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_auto_commit, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_auto_commit, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_commit), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_auto_commit), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_auto_commit,
        _("If true then the converted result string will "
          "be committed to client automatically."));
    ++ row;

    // Auto Split
    prop_dialog_add_label (table, _("Auto Split:"), row);
    pd->toggle_auto_split = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_auto_split, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_auto_split, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_split), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_auto_split), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_auto_split,
        _("If true then the inputted key string will be "
          "split automatically when necessary."));
    ++ row;

    // Discard Invalid Key
    prop_dialog_add_label (table, _("Discard Invalid Key:"), row);
    pd->toggle_discard_invalid_key = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_discard_invalid_key, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_discard_invalid_key, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_discard_invalid_key), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_discard_invalid_key), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_discard_invalid_key,
        _("If true then the invalid key will be discarded automatically."
          "This option is only valid when Auto Select and Auto Commit is true."));
    ++ row;

    // Dynamic Adjust
    prop_dialog_add_label (table, _("Dynamic Adjust:"), row);
    pd->toggle_dynamic_adjust = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_dynamic_adjust, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_dynamic_adjust, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_dynamic_adjust), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_dynamic_adjust), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_dynamic_adjust,
        _("If true then the phrases' frequencies "
          "will be adjusted dynamically."));
    ++ row;

    // Auto Fill Preedit String
    prop_dialog_add_label (table, _("Auto Fill Preedit Area:"), row);
    pd->toggle_auto_fill = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_auto_fill, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_auto_fill, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_fill), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_auto_fill), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_auto_fill,
        _("If true then the preedit string will be filled up with the "
          "current candiate phrase automatically."
          "This option is only valid when Auto Select is TRUE."));
    ++ row;

    // Always Show Lookup
    prop_dialog_add_label (table, _("Always Show Lookup Table:"), row);
    pd->toggle_always_show_lookup = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_always_show_lookup, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_always_show_lookup, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_always_show_lookup), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_always_show_lookup), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_always_show_lookup,
        _("If true then the lookup table will always be shown "
          "when any candidate phrase is available. Otherwise "
          "the lookup table will only be shown when necessary.\n"
          "If Auto Fill is false, then this option will be no effect, "
          "and always be true."));
    ++ row;

    // Default full width punctuation
    prop_dialog_add_label (table, _("Default Full Width Punct:"), row);
    pd->toggle_def_full_width_punct = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_def_full_width_punct, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_def_full_width_punct, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_def_full_width_punct), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_def_full_width_punct), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_def_full_width_punct,
        _("If true then full width punctuations will be inputted by default."));
    ++ row;

    // Default full width letter
    prop_dialog_add_label (table, _("Default Full Width Letter:"), row);
    pd->toggle_def_full_width_letter = gtk_toggle_button_new_with_label (_("True"));
    gtk_widget_set_halign (pd->toggle_def_full_width_letter, GTK_ALIGN_FILL);
    gtk_grid_attach (GTK_GRID (table), pd->toggle_def_full_width_letter, 1, row, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_def_full_width_letter), TRUE);
    g_signal_connect (G_OBJECT (pd->toggle_def_full_width_letter), "toggled",
                      G_CALLBACK (on_toggle_button_toggled), 0);
    gtk_widget_set_tooltip_text (pd->toggle_def_full_width_letter,
        _("If true then full width letters will be inputted by default."));

    {// Set initial data and the widgets status.
        gtk_editable_set_editable (GTK_EDITABLE (pd->entry_name), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->entry_author), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->entry_uuid), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->entry_serial), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->entry_icon), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->entry_valid_input_chars), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->split_keys.entry), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->commit_keys.entry), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->forward_keys.entry), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->select_keys.entry), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->page_up_keys.entry), FALSE);
        gtk_editable_set_editable (GTK_EDITABLE (pd->page_down_keys.entry), FALSE);

        if (!editable) {
            gtk_editable_set_editable (GTK_EDITABLE (pd->entry_status_prompt), FALSE);
            gtk_editable_set_editable (GTK_EDITABLE (pd->entry_languages), FALSE);
            gtk_editable_set_editable (GTK_EDITABLE (pd->entry_multi_wildcard_chars), FALSE);
            gtk_editable_set_editable (GTK_EDITABLE (pd->entry_single_wildcard_chars), FALSE);

            gtk_widget_set_sensitive (pd->spin_max_key_length, FALSE);
            gtk_widget_set_sensitive (pd->toggle_show_key_prompt, FALSE);
            gtk_widget_set_sensitive (pd->toggle_auto_select, FALSE);
            gtk_widget_set_sensitive (pd->toggle_auto_fill, FALSE);
            gtk_widget_set_sensitive (pd->toggle_auto_wildcard, FALSE);
            gtk_widget_set_sensitive (pd->toggle_auto_commit, FALSE);
            gtk_widget_set_sensitive (pd->toggle_auto_split, FALSE);
            gtk_widget_set_sensitive (pd->toggle_discard_invalid_key, FALSE);
            gtk_widget_set_sensitive (pd->toggle_dynamic_adjust, FALSE);
            gtk_widget_set_sensitive (pd->toggle_always_show_lookup, FALSE);
            gtk_widget_set_sensitive (pd->toggle_def_full_width_punct, FALSE);
            gtk_widget_set_sensitive (pd->toggle_def_full_width_letter, FALSE);
            gtk_widget_set_sensitive (pd->button_icon, FALSE);
            gtk_widget_set_sensitive (pd->split_keys.button, FALSE);
            gtk_widget_set_sensitive (pd->commit_keys.button, FALSE);
            gtk_widget_set_sensitive (pd->forward_keys.button, FALSE);
            gtk_widget_set_sensitive (pd->select_keys.button, FALSE);
            gtk_widget_set_sensitive (pd->page_up_keys.button, FALSE);
            gtk_widget_set_sensitive (pd->page_down_keys.button, FALSE);
        }

        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_name), pd->data.name.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_author), pd->data.author.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_uuid), pd->data.uuid.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_serial), pd->data.serial.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_icon), pd->data.icon.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_languages), pd->data.languages.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_status_prompt), pd->data.status_prompt.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_valid_input_chars), pd->data.valid_input_chars.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_multi_wildcard_chars), pd->data.multi_wildcard_chars.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->entry_single_wildcard_chars), pd->data.single_wildcard_chars.c_str ());

        pd->split_keys.data = pd->data.split_keys;
        pd->commit_keys.data = pd->data.commit_keys;
        pd->forward_keys.data = pd->data.forward_keys;
        pd->select_keys.data = pd->data.select_keys;
        pd->page_up_keys.data = pd->data.page_up_keys;
        pd->page_down_keys.data = pd->data.page_down_keys;

        gtk_editable_set_text  (GTK_EDITABLE (pd->split_keys.entry), pd->data.split_keys.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->commit_keys.entry), pd->data.commit_keys.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->forward_keys.entry), pd->data.forward_keys.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->select_keys.entry), pd->data.select_keys.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->page_up_keys.entry), pd->data.page_up_keys.c_str ());
        gtk_editable_set_text  (GTK_EDITABLE (pd->page_down_keys.entry), pd->data.page_down_keys.c_str ());

        gtk_spin_button_set_range (GTK_SPIN_BUTTON (pd->spin_max_key_length), pd->data.max_key_length, SCIM_GT_MAX_KEY_LENGTH);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_show_key_prompt), pd->data.show_key_prompt);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_select), pd->data.auto_select);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_fill), pd->data.auto_fill);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_wildcard), pd->data.auto_wildcard);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_commit), pd->data.auto_commit);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_auto_split), pd->data.auto_split);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_discard_invalid_key), pd->data.discard_invalid_key);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_dynamic_adjust), pd->data.dynamic_adjust);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_always_show_lookup), pd->data.always_show_lookup);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_def_full_width_punct), pd->data.def_full_width_punct);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pd->toggle_def_full_width_letter), pd->data.def_full_width_letter);
    }

    gtk_window_set_default_size (GTK_WINDOW (dialog), 560, 400);

    g_signal_connect (dialog, "response",
                      G_CALLBACK (table_properties_dialog_response_cb), pd);

    gtk_window_present (GTK_WINDOW (dialog));
}

static bool
validate_table_properties_data (const GenericTableLibrary *lib, const TablePropertiesData &data)
{
    bool ok = true;
    String err;

    if (ok && !data.icon.length () && access (data.icon.c_str (), R_OK) != 0) {
        ok = false;
        err = _("Invalid icon file.");
    }

    if (ok && !data.languages.length ()) {
        ok = false;
        err = _("Invalid languages.");
    }

    if (ok && !data.status_prompt.length ()) {
        ok = false;
        err = _("Invalid status prompt.");
    }

    if (ok && data.multi_wildcard_chars.length ()) {
        for (String::const_iterator i = data.multi_wildcard_chars.begin ();
             i != data.multi_wildcard_chars.end (); ++i) {
            if (lib->is_valid_input_char (*i)) {
                ok = false;
                err = _("Invalid multi wildcard chars.");
                break;
            }
        }
    }

    if (ok && data.single_wildcard_chars.length ()) {
        for (String::const_iterator i = data.single_wildcard_chars.begin ();
             i != data.single_wildcard_chars.end (); ++i) {
            if (lib->is_valid_input_char (*i) ||
                data.multi_wildcard_chars.find (*i) != String::npos) {
                ok = false;
                err = _("Invalid single wildcard chars.");
                break;
            }
        }
    }

    if (ok && !data.commit_keys.length ()) {
        ok = false;
        err = _("Invalid commit keys.");
    }

    if (ok && !data.select_keys.length ()) {
        ok = false;
        err = _("Invalid select keys.");
    }

    if (ok && !data.page_up_keys.length ()) {
        ok = false;
        err = _("Invalid page up keys.");
    }

    if (ok && !data.page_down_keys.length ()) {
        ok = false;
        err = _("Invalid page down keys.");
    }

    if (ok && (data.max_key_length < (int) lib->get_max_key_length () ||
               data.max_key_length > SCIM_GT_MAX_KEY_LENGTH)) {
        ok = false;
        err = _("Invalid max key length.");
    }

    if (!ok) {
        show_message (err.c_str ());
    }

    return ok;
}

static void
on_table_properties_clicked (GtkButton *button,
                             gpointer   user_data)
{
    GtkTreeIter  iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (__widget_table_list_view));

    if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
        GenericTableLibrary *lib;
        gchar               *file;

        gtk_tree_model_get (model, &iter,
                            TABLE_COLUMN_LIBRARY, &lib,
                            TABLE_COLUMN_FILE, &file,
                            -1);

        if (!lib || !file) {
            g_free (file);
            return;
        }

        TablePropertiesData data;

        data.name                  = utf8_wcstombs (lib->get_name (scim_get_current_locale ()));
        data.author                = utf8_wcstombs (lib->get_author ());
        data.uuid                  = lib->get_uuid ();
        data.serial                = lib->get_serial_number ();
        data.languages             = lib->get_languages ();
        data.icon                  = lib->get_icon_file ();
        data.status_prompt         = utf8_wcstombs (lib->get_status_prompt ());
        data.valid_input_chars     = lib->get_valid_input_chars ();
        data.multi_wildcard_chars  = lib->get_multi_wildcard_chars ();
        data.single_wildcard_chars = lib->get_single_wildcard_chars ();

        data.max_key_length        = lib->get_max_key_length ();
        data.show_key_prompt       = lib->is_show_key_prompt ();
        data.auto_select           = lib->is_auto_select ();
        data.auto_fill             = lib->is_auto_fill ();
        data.auto_wildcard         = lib->is_auto_wildcard ();
        data.auto_commit           = lib->is_auto_commit ();
        data.auto_split            = lib->is_auto_split ();
        data.discard_invalid_key   = lib->is_discard_invalid_key ();
        data.dynamic_adjust        = lib->is_dynamic_adjust ();
        data.always_show_lookup    = lib->is_always_show_lookup ();
        data.def_full_width_punct  = lib->is_def_full_width_punct ();
        data.def_full_width_letter = lib->is_def_full_width_letter ();

        scim_key_list_to_string (data.split_keys, lib->get_split_keys ());
        scim_key_list_to_string (data.commit_keys, lib->get_commit_keys ());
        scim_key_list_to_string (data.forward_keys, lib->get_forward_keys ());
        scim_key_list_to_string (data.select_keys, lib->get_select_keys ());
        scim_key_list_to_string (data.page_up_keys, lib->get_page_up_keys ());
        scim_key_list_to_string (data.page_down_keys, lib->get_page_down_keys ());

        bool file_editable = test_file_modify (file);

        g_free (file);

        // The dialog runs asynchronously; the changes are written back to the
        // library in the dialog's "response" handler.
        run_table_properties_dialog (lib, model, &iter, data, file_editable);
    }
}

static void
save_all_tables ()
{
    GtkTreeIter iter;
    if (__widget_table_list_model &&
        gtk_tree_model_get_iter_first (GTK_TREE_MODEL (__widget_table_list_model), &iter)) {

        GenericTableLibrary *lib;
        gchar *file;
        gchar *name;
        gboolean is_user;

        do {
            gtk_tree_model_get (GTK_TREE_MODEL (__widget_table_list_model), &iter,
                                TABLE_COLUMN_LIBRARY, &lib,
                                TABLE_COLUMN_FILE, &file,
                                TABLE_COLUMN_NAME, &name,
                                TABLE_COLUMN_IS_USER, &is_user,
                                -1);
            if (lib->updated () && file) {
                if (!lib->save (file, "", "", is_user ? __config_user_table_binary : true)) {
                    char buf [1024];
                    snprintf (buf, sizeof (buf),
                              _("Failed to save table %s!"),
                              name);
                    show_message (buf);
                }
            }
            g_free (file);
            g_free (name);
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (__widget_table_list_model), &iter));
    }
}


/*
vi:ts=4:nowrap:expandtab
*/
