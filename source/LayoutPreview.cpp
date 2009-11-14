/*
 * Copyright (c) 2009, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LayoutPreview.h"
#include "DragDrop.h"
#include "uiColorInput.h"
#include "CopyPaste.h"
#include "main.h"
#include "Converter.h"
#include "DynvHelpers.h"

#include "uiUtilities.h"
#include "ColorList.h"
#include "MathUtil.h"

#include "gtk/LayoutPreview.h"
#include "layout/Layout.h"
#include "layout/Style.h"

#include <math.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <list>
using namespace std;
using namespace layout;

struct Arguments{
	ColorSource source;

	GtkWidget *main;

	GtkWidget *layout;

	System* layout_system;
	Layouts* layouts;

	struct dynvSystem *params;
	GlobalState *gs;
};

typedef enum{
	LAYOUTLIST_HUMAN_NAME = 0,
	LAYOUTLIST_PTR,
	LAYOUTLIST_N_COLUMNS
}LayoutListColumns;

typedef enum{
	STYLELIST_HUMAN_NAME = 0,
	STYLELIST_CSS_SELECTOR,
	STYLELIST_PTR,
	STYLELIST_N_COLUMNS
}StyleListColumns;

static void style_cell_edited_cb(GtkCellRendererText *cell, gchar *path, gchar *new_text, GtkListStore *store){
	GtkTreeIter iter1;
	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(store), &iter1, path );
	gtk_list_store_set(store, &iter1, STYLELIST_CSS_SELECTOR, new_text, -1);
}

static GtkWidget* style_list_new(struct Arguments *args){

	GtkListStore  		*store;
	GtkCellRenderer     *renderer;
	GtkTreeViewColumn   *col;
	GtkWidget           *view;

	view = gtk_tree_view_new();

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), 1);

	store = gtk_list_store_new (STYLELIST_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(col,GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable(col, true);
	gtk_tree_view_column_set_title(col, "Style item");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, true);
	gtk_tree_view_column_add_attribute(col, renderer, "text", STYLELIST_HUMAN_NAME);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);

	col = gtk_tree_view_column_new();
	gtk_tree_view_column_set_sizing(col,GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable(col, true);
	gtk_tree_view_column_set_title(col, "CSS selector");
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(col, renderer, true);
	gtk_tree_view_column_add_attribute(col, renderer, "text", STYLELIST_CSS_SELECTOR);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
	g_object_set(renderer, "editable", true, NULL);
	g_signal_connect(renderer, "edited", (GCallback)style_cell_edited_cb, store);

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
	g_object_unref(GTK_TREE_MODEL(store));

	/*GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_reorderable(GTK_TREE_VIEW(view), TRUE);*/

	return view;
}

static void assign_css_selectors_cb(GtkWidget *widget, struct Arguments* args) {

	GtkWidget *table;

	GtkWidget *dialog = gtk_dialog_new_with_buttons("Assign CSS selectors", GTK_WINDOW(gtk_widget_get_toplevel(args->main)), GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OK, GTK_RESPONSE_OK,
			NULL);

	gtk_window_set_default_size(GTK_WINDOW(dialog), dynv_get_int32_wd(args->params, "css_selectors.window.width", -1),
		dynv_get_int32_wd(args->params, "css_selectors.window.height", -1));

	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dialog), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);


	gint table_y;
	table = gtk_table_new(1, 1, FALSE);
	table_y=0;

	GtkWidget* list_widget = style_list_new(args);
	gtk_widget_set_size_request(list_widget, 100, 100);
	gtk_table_attach(GTK_TABLE(table), list_widget, 0, 1, table_y, table_y+1, GtkAttachOptions(GTK_FILL|GTK_EXPAND), GtkAttachOptions(GTK_FILL|GTK_EXPAND), 5, 0);
	table_y++;
	
	
	GtkTreeIter iter1;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list_widget));

	struct dynvSystem *assignments_params = dynv_get_dynv(args->params, "css_selectors.assignments");

	string ident_selector;
	for (list<Style*>::iterator i = args->layout_system->styles.begin(); i != args->layout_system->styles.end(); i++){

		ident_selector = (*i)->ident_name + ".selector";
	
		const char *css_selector = dynv_get_string_wd(assignments_params, ident_selector.c_str(), (*i)->ident_name.c_str());
		
		gtk_list_store_append(GTK_LIST_STORE(model), &iter1);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter1,
			STYLELIST_HUMAN_NAME, (*i)->human_name.c_str(),
			STYLELIST_CSS_SELECTOR, css_selector,
			STYLELIST_PTR, (*i),
		-1);
		
	}
	
	gtk_widget_show_all(table);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK){
		
		gboolean valid;
		Style *style;
		char* selector;
		
		valid = gtk_tree_model_get_iter_first(model, &iter1);
		while (valid){
			gtk_tree_model_get(model, &iter1, STYLELIST_PTR, &style, STYLELIST_CSS_SELECTOR, &selector, -1);
			
			ident_selector = style->ident_name + ".selector";

			dynv_set_string(assignments_params, ident_selector.c_str(), selector);
			
			g_free(selector);

			valid = gtk_tree_model_iter_next(model, &iter1);
		}

	}
	
	dynv_system_release(assignments_params);

	gint width, height;
	gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);

	dynv_set_int32(args->params, "css_selectors.window.width", width);
	dynv_set_int32(args->params, "css_selectors.window.height", height);

	gtk_widget_destroy(dialog);
}

static int source_destroy(struct Arguments *args){
	if (args->layout_system) System::unref(args->layout_system);
	args->layout_system = 0;
	gtk_widget_destroy(args->main);
	dynv_system_release(args->params);
	delete args;
	return 0;
}

static int source_get_color(struct Arguments *args, struct ColorObject** color){
	Color c;
	if (gtk_layout_preview_get_current_color(GTK_LAYOUT_PREVIEW(args->layout), &c)==0){
		*color = color_list_new_color_object(args->gs->colors, &c);
		return 0;
	}
	return -1;
}

static int source_set_color(struct Arguments *args, struct ColorObject* color){
	Color c;
	color_object_get_color(color, &c);
	gtk_layout_preview_set_current_color(GTK_LAYOUT_PREVIEW(args->layout), &c);
	return -1;
}

static int source_deactivate(struct Arguments *args){

	return 0;
}

static struct ColorObject* get_color_object(struct DragDrop* dd){
	struct Arguments* args=(struct Arguments*)dd->userdata;
	struct ColorObject* colorobject;
	if (source_get_color(args, &colorobject)==0){
		return colorobject;
	}
	return 0;
}

static int set_color_object_at(struct DragDrop* dd, struct ColorObject* colorobject, int x, int y, bool move){
	struct Arguments* args=(struct Arguments*)dd->userdata;
	Color color;
	color_object_get_color(colorobject, &color);
	gtk_layout_preview_set_color_at(GTK_LAYOUT_PREVIEW(args->layout), &color, x, y);
	return 0;
}

bool test_at(struct DragDrop* dd, int x, int y){
	struct Arguments* args=(struct Arguments*)dd->userdata;

	gtk_layout_preview_set_focus_at(GTK_LAYOUT_PREVIEW(args->layout), x, y);

	return gtk_layout_preview_is_selected(GTK_LAYOUT_PREVIEW(args->layout));
}

static GtkWidget* layout_preview_dropdown_new(struct Arguments *args, GtkTreeModel *model){

	GtkListStore  		*store = 0;
	GtkCellRenderer     *renderer;
	GtkTreeViewColumn   *col;
	GtkWidget			*combo;

	if (model){
		combo = gtk_combo_box_new_with_model(model);
	}else{
		store = gtk_list_store_new (LAYOUTLIST_N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
		combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	}

	renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, true);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer, "text", LAYOUTLIST_HUMAN_NAME, NULL);

	if (store) g_object_unref (store);

	return combo;
}

static void edit_cb(GtkWidget *widget,  gpointer item) {
	struct Arguments* args=(struct Arguments*)item;

	struct ColorObject *color_object;
	struct ColorObject* new_color_object = 0;
	if (source_get_color(args, &color_object)==0){
		if (dialog_color_input_show(GTK_WINDOW(gtk_widget_get_toplevel(args->main)), args->gs, color_object, &new_color_object )==0){
			source_set_color(args, new_color_object);
			color_object_release(new_color_object);
		}
		color_object_release(color_object);
	}
}

static void paste_cb(GtkWidget *widget, struct Arguments* args) {
	struct ColorObject* color_object;
	if (copypaste_get_color_object(&color_object, args->gs)==0){
		source_set_color(args, color_object);
		color_object_release(color_object);
	}
}

static void add_to_palette_cb(GtkWidget *widget,  gpointer item) {
	struct Arguments* args=(struct Arguments*)item;

	struct ColorObject *color_object;
	if (source_get_color(args, &color_object)==0){
		dynv_system_set(color_object->params, "string", "name", (void*)"");
		color_list_add_color_object(args->gs->colors, color_object, 1);
		color_object_release(color_object);
	}
}

static void add_all_to_palette_cb(GtkWidget *widget,  gpointer item) {
	struct Arguments* args=(struct Arguments*)item;

}

static void popup_menu_detach_cb(GtkWidget *attach_widget, GtkMenu *menu) {
	gtk_widget_destroy(GTK_WIDGET(menu));
}

static gboolean button_press_cb (GtkWidget *widget, GdkEventButton *event, struct Arguments* args) {
	static GtkWidget *menu=NULL;
	if (menu) {
		gtk_menu_detach(GTK_MENU(menu));
		menu=NULL;
	}

	if (event->button == 3 && event->type == GDK_BUTTON_PRESS){

		GtkWidget* item ;
		gint32 button, event_time;

		menu = gtk_menu_new ();

		bool selection_avail = gtk_layout_preview_is_selected(GTK_LAYOUT_PREVIEW(args->layout));

	    item = gtk_menu_item_new_with_image ("_Add to palette", gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (add_to_palette_cb), args);
		if (!selection_avail) gtk_widget_set_sensitive(item, false);

	    item = gtk_menu_item_new_with_image ("_Add all to palette", gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU));
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	    g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (add_all_to_palette_cb), args);

	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

	    item = gtk_menu_item_new_with_mnemonic ("_Copy to clipboard");
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		if (selection_avail){
			struct ColorObject* color_object;
			source_get_color(args, &color_object);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), converter_create_copy_menu (color_object, 0, args->gs));
			color_object_release(color_object);
		}else{
			gtk_widget_set_sensitive(item, false);
		}

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

		item = gtk_menu_item_new_with_image ("_Edit...", gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (edit_cb), args);
		if (!selection_avail) gtk_widget_set_sensitive(item, false);

		item = gtk_menu_item_new_with_image ("_Paste", gtk_image_new_from_stock(GTK_STOCK_PASTE, GTK_ICON_SIZE_MENU));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
		g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (paste_cb), args);

		if (copypaste_is_color_object_available(args->gs)!=0){
			gtk_widget_set_sensitive(item, false);
		}

		gtk_widget_show_all (GTK_WIDGET(menu));

		button = event->button;
		event_time = event->time;

		gtk_menu_attach_to_widget (GTK_MENU (menu), widget, popup_menu_detach_cb);
		gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, button, event_time);

		return TRUE;
	}
	return FALSE;
}

static void layout_changed_cb(GtkWidget *widget, struct Arguments* args) {
	GtkTreeIter iter;
	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter)) {
		GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));

		Layout* layout;
		gtk_tree_model_get(model, &iter, LAYOUTLIST_PTR, &layout, -1);

		System* layout_system = layouts_get(args->layouts, layout->name);
		gtk_layout_preview_set_system(GTK_LAYOUT_PREVIEW(args->layout), layout_system);
		if (args->layout_system) System::unref(args->layout_system);
		args->layout_system = layout_system;

		dynv_set_string(args->params, "layout_name", layout->name);
		
		
	}
}


static int save_css_file(const char* filename, struct Arguments* args){

	ofstream file(filename, ios::out);
	if (file.is_open()){

		Converters *converters = (Converters*)dynv_system_get(args->gs->params, "ptr", "Converters");
		Converter *converter = converters_get_first(converters, CONVERTERS_ARRAY_TYPE_COPY);

		struct ColorObject *co_color, *co_background_color;
		Color t;
		co_color = color_list_new_color_object(args->gs->colors, &t);
		co_background_color = color_list_new_color_object(args->gs->colors, &t);
		char *color, *background_color;
		
		struct dynvSystem *assignments_params = dynv_get_dynv(args->params, "css_selectors.assignments");
		string ident_selector;

		for (list<Style*>::iterator i=args->layout_system->styles.begin(); i!=args->layout_system->styles.end(); i++){

			ident_selector = (*i)->ident_name + ".selector";
			const char *css_selector = dynv_get_string_wd(assignments_params, ident_selector.c_str(), (*i)->ident_name.c_str());
			
			if (css_selector[0]!=0){
			
				color_object_set_color(co_color, &(*i)->color);

				converter_get_text(converter->function_name, co_color, 0, args->gs->params, &color);
					
				file << css_selector << " {" << endl;
				
				if ((*i)->style_type == Style::TYPE_BACKGROUND){
					file << "\tbackground-color: " << color << ";" << endl;
				}else if ((*i)->style_type == Style::TYPE_COLOR){
					file << "\tcolor: " << color << ";" << endl;
				}else if ((*i)->style_type == Style::TYPE_BORDER){
					file << "\tborder-color: " << color << ";" << endl;
				}
				
				file << "}" << endl << endl;
			
			}

			g_free(color);
		}
		
		dynv_system_release(assignments_params);

		color_object_release(co_color);
		color_object_release(co_background_color);

		file.close();
		return 0;
	}
	return -1;
}

static void export_css_cb(GtkWidget *widget, struct Arguments* args){
	GtkWidget *dialog;
	GtkFileFilter *filter;

	dialog = gtk_file_chooser_dialog_new ("Export", GTK_WINDOW(gtk_widget_get_toplevel(widget)),
						  GTK_FILE_CHOOSER_ACTION_SAVE,
						  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						  GTK_STOCK_SAVE, GTK_RESPONSE_OK,
						  NULL);

	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dialog), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL, -1);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	const char* default_path = dynv_get_string_wd(args->params, "export_path", "");
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), default_path);

	filter = gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "Cascading Style Sheets *.css");
	gtk_file_filter_add_pattern(filter, "*.css");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), filter);

	gboolean finished = false;

	while (!finished){
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
			gchar *filename;
			filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

			gchar *path;
			path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
			dynv_set_string(args->params, "export_path", path);
			g_free(path);

			if (save_css_file(filename, args)==0){

				finished = true;
			}else{
				GtkWidget* message;
				message=gtk_message_dialog_new(GTK_WINDOW(dialog), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "File could not be saved");
				//gtk_window_set_title(GTK_WINDOW(dialog), "Open");
				gtk_dialog_run(GTK_DIALOG(message));
				gtk_widget_destroy(message);
			}

			g_free(filename);
		}else break;
	}

	gtk_widget_destroy (dialog);
}

static GtkWidget* attach_label(GtkWidget *widget, const char *label){
	GtkWidget* hbox = gtk_hbox_new(false, 5);
	gtk_box_pack_start(GTK_BOX(hbox), gtk_label_aligned_new(label, 0, 0.5, 0, 0), false, true, 0);
	gtk_box_pack_start(GTK_BOX(hbox), widget, true, true, 0);
	return hbox;
}

ColorSource* layout_preview_new(GlobalState* gs, GtkWidget **out_widget){
	struct Arguments* args = new struct Arguments;

	args->params = dynv_get_dynv(gs->params, "gpick.layout_preview");

	args->layout_system = 0;

	color_source_init(&args->source);
	args->source.destroy = (int (*)(ColorSource *source))source_destroy;
	args->source.get_color = (int (*)(ColorSource *source, ColorObject** color))source_get_color;
	args->source.set_color = (int (*)(ColorSource *source, ColorObject* color))source_set_color;
	args->source.deactivate = (int (*)(ColorSource *source))source_deactivate;

	Layouts* layouts = (Layouts*)dynv_system_get(gs->params, "ptr", "Layouts");
	args->layouts = layouts;

	GtkWidget *table, *vbox, *hbox, *widget;

	vbox = gtk_vbox_new(false, 10);
	hbox = gtk_hbox_new(false, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, true, true, 5);

	gint table_y;
	table = gtk_table_new(4, 4, false);
	gtk_box_pack_start(GTK_BOX(hbox), table, true, true, 5);
	table_y = 0;

	GtkToolItem *tool;

	GtkWidget *toolbar = gtk_toolbar_new();
	gtk_table_attach(GTK_TABLE(table), toolbar, 0, 3, table_y, table_y+1, GtkAttachOptions(GTK_FILL), GTK_FILL, 0, 0);
	table_y++;
	
	tool = gtk_tool_item_new();
    gtk_tool_item_set_expand(tool, true);
	GtkWidget *layout_dropdown = layout_preview_dropdown_new(args, 0);
    gtk_container_add(GTK_CONTAINER(tool), attach_label(layout_dropdown, "Layout:"));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool, -1);
	g_signal_connect (G_OBJECT(layout_dropdown), "changed", G_CALLBACK(layout_changed_cb), args);
	
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
		
	tool = gtk_menu_tool_button_new(gtk_image_new_from_stock(GTK_STOCK_SAVE_AS, GTK_ICON_SIZE_BUTTON), "Export CSS");
	g_signal_connect(G_OBJECT(tool), "clicked", G_CALLBACK(export_css_cb), args);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool, -1);
	
	GtkWidget *menu;
	GtkWidget* item;

	menu = gtk_menu_new ();
	item = gtk_menu_item_new_with_mnemonic ("_Assign CSS selectors");
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (assign_css_selectors_cb), args);
	gtk_widget_show_all(menu);
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(tool), menu);
	
	GtkWidget *scrolled = gtk_scrolled_window_new(0, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_table_attach(GTK_TABLE(table), scrolled, 0, 3, table_y, table_y+1 ,GtkAttachOptions(GTK_FILL | GTK_EXPAND),GtkAttachOptions(GTK_FILL | GTK_EXPAND),0,0);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), args->layout = gtk_layout_preview_new());
	g_signal_connect_after(G_OBJECT(args->layout), "button-press-event", G_CALLBACK (button_press_cb), args);
	table_y++;

	struct DragDrop dd;
	dragdrop_init(&dd, gs);

	dd.userdata = args;
	dd.get_color_object = get_color_object;
	dd.set_color_object_at = set_color_object_at;
	dd.test_at = test_at;
	dd.handler_map = dynv_system_get_handler_map(gs->colors->params);


	gtk_drag_dest_set(args->layout, GtkDestDefaults(GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT), 0, 0, GDK_ACTION_COPY);
	gtk_drag_source_set(args->layout, GDK_BUTTON1_MASK, 0, 0, GDK_ACTION_COPY);
	dragdrop_widget_attach(args->layout, DragDropFlags(DRAGDROP_SOURCE | DRAGDROP_DESTINATION), &dd);

	args->gs = gs;


	// Restore settings and fill list
	const char* layout_name = dynv_get_string_wd(args->params, "layout_name", "std_layout_menu_1");

	GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(layout_dropdown));
	uint32_t n_layouts;
	Layout** layout_table = layouts_get_all(layouts, &n_layouts);
	GtkTreeIter iter1;
	bool layout_found = false;

	for (uint32_t i=0; i!=n_layouts; ++i){
		gtk_list_store_append(GTK_LIST_STORE(model), &iter1);

		gtk_list_store_set(GTK_LIST_STORE(model), &iter1,
			LAYOUTLIST_HUMAN_NAME, layout_table[i]->human_readable,
			LAYOUTLIST_PTR, layout_table[i],
		-1);

		if (g_strcmp0(layout_name, layout_table[i]->name)==0){
			gtk_combo_box_set_active_iter(GTK_COMBO_BOX(layout_dropdown), &iter1);
			layout_found = true;
		}
	}

	if (!layout_found){
		gtk_combo_box_set_active(GTK_COMBO_BOX(layout_dropdown), 0);
	}


	gtk_widget_show_all(vbox);

	//update(0, args);

	args->main = vbox;
	*out_widget = vbox;

	return (ColorSource*)args;
}
