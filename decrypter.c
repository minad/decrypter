#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>

enum {
	COL_TEXT,
	COL_SUBSTITUTE,
	COL_FREQUENCY,
	COL_FREQUENCY_INT,
	NUM_COLS,
};

gboolean window_delete_event(GtkWidget*, GdkEvent*, gpointer);
void crypted_text_changed(GtkTextBuffer*, gpointer);
GtkWidget* new_freq_table(const char*, const char*, GtkTreeView**);
GtkWidget* new_text_view(const char*, GtkTextView**);
char* get_crypted_text();
void frequency_walker(gpointer, gpointer, gpointer);
void analyze_frequencies(const char*);
void update_plain_text(char*);
char* decrypt_text(char*);
int sort_frequencies(GtkTreeModel*, GtkTreeIter*, GtkTreeIter*, gpointer);
void mapping_edited(GtkCellRendererText*, char*, char*, gpointer);
gboolean update_substitute(GtkTreeModel*, GtkTreePath*, GtkTreeIter*, gpointer);
GtkWidget* new_menu_bar();
void open_file(GtkWidget*, gpointer);
void save_file(GtkWidget*, gpointer);
void open_file_dialog(GtkWidget*, gpointer);
void save_file_dialog(GtkWidget*, gpointer);

GtkTextTag *tag;
GtkTreeView *letter_freq_table, *bigram_freq_table, *trigram_freq_table; 
GtkTextView *crypted_text_view, *plain_text_view;
char mapping[256];
int total_count;

int main(int argc, char* argv[]) {
	GtkWidget *window, *vbox, *hbox, *widget;
	
	memset(mapping, 0, sizeof (mapping));

	gtk_init(&argc, &argv);
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Substitution Encryption Cryptanalysis Tool");
	gtk_window_resize(GTK_WINDOW(window), 600, 500); 	
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(window_delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);

	vbox = gtk_vbox_new(FALSE, 0); 
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtk_box_pack_start(GTK_BOX(vbox), new_menu_bar(), FALSE, FALSE, 0);
	
	widget = new_text_view("Crypted Text", &crypted_text_view);
       	g_signal_connect(G_OBJECT(gtk_text_view_get_buffer(crypted_text_view)), "changed", G_CALLBACK(crypted_text_changed), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);
	
	widget = new_text_view("Plain Text", &plain_text_view);
	gtk_text_view_set_editable(plain_text_view, FALSE);
	tag = gtk_text_buffer_create_tag(gtk_text_view_get_buffer(plain_text_view), "uncrypted_text", "foreground", "red", NULL);  
	gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	widget = new_freq_table("Letters", "Letter", &letter_freq_table);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	widget = new_freq_table("Bigrams", "Bigram", &bigram_freq_table);
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	widget = new_freq_table("Trigrams", "Trigram", &trigram_freq_table); 
	gtk_box_pack_start(GTK_BOX(hbox), widget, TRUE, TRUE, 0);

	gtk_widget_show(window);

	gtk_main();

	return 0;
}

GtkWidget* new_menu_bar() {
	GtkWidget *menu_bar, *file_menu, *open_item, *quit_item, *save_item, *file_item;

	menu_bar = gtk_menu_bar_new();
	gtk_widget_show(menu_bar);

	file_menu = gtk_menu_new();
	gtk_widget_show(file_menu);

        file_item = gtk_menu_item_new_with_mnemonic("_File");
        gtk_widget_show(file_item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);	
	gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), file_item);

	open_item = gtk_menu_item_new_with_mnemonic("_Open");
	save_item = gtk_menu_item_new_with_mnemonic("_Save");
	quit_item = gtk_menu_item_new_with_mnemonic("_Quit");

	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);

	gtk_widget_show(open_item);
	gtk_widget_show(save_item);
	gtk_widget_show(quit_item);

	g_signal_connect(G_OBJECT(open_item), "activate", G_CALLBACK(open_file_dialog), NULL);
	g_signal_connect(G_OBJECT(save_item), "activate", G_CALLBACK(save_file_dialog), NULL);
	g_signal_connect(G_OBJECT(quit_item), "activate", G_CALLBACK(gtk_main_quit), NULL);

	return menu_bar;
}

int sort_frequencies(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b, gpointer user_data) {
	int aval, bval;
	gtk_tree_model_get(model, a, COL_FREQUENCY_INT, &aval, -1);
	gtk_tree_model_get(model, b, COL_FREQUENCY_INT, &bval, -1);
	return (aval - bval);
}

GtkWidget* new_freq_table(const char* title, const char* col_title, GtkTreeView** result) {
	GtkWidget *frame, *scrolled_window, *table;
	GtkTreeViewColumn* col;
	GtkListStore* store;
	GtkCellRenderer *renderer, *editable_renderer;
	
	frame = gtk_frame_new(title);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 5);
	gtk_widget_show(scrolled_window);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);

	store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	table = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	gtk_widget_show(table);
        gtk_container_add(GTK_CONTAINER(scrolled_window), table);

	renderer = gtk_cell_renderer_text_new();
	
	editable_renderer = gtk_cell_renderer_text_new();
	g_object_set(editable_renderer, "editable", TRUE, NULL);
	g_signal_connect(G_OBJECT(editable_renderer), "edited", G_CALLBACK(mapping_edited), store);

	
	col = gtk_tree_view_column_new_with_attributes(col_title, renderer, 
						       "text", COL_TEXT,
						       NULL);
	gtk_tree_view_column_set_visible(col, TRUE);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_column_set_sort_column_id(col, COL_TEXT);
	gtk_tree_view_append_column(GTK_TREE_VIEW(table), col);
	
	col = gtk_tree_view_column_new_with_attributes("Substitute", editable_renderer, 
						       "text", COL_SUBSTITUTE,
						       NULL);
	gtk_tree_view_column_set_visible(col, TRUE);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_column_set_sort_column_id(col, COL_SUBSTITUTE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(table), col);
	
	
	col = gtk_tree_view_column_new_with_attributes("Frequency", renderer,
						       "text", COL_FREQUENCY,
						       NULL);
	gtk_tree_view_column_set_visible(col, TRUE);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_reorderable(col, TRUE);
	gtk_tree_view_column_set_sort_column_id(col, COL_FREQUENCY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(table), col);

        gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store), COL_FREQUENCY, sort_frequencies, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), COL_FREQUENCY, GTK_SORT_DESCENDING);
	
	if (result)
		*result = GTK_TREE_VIEW(table);
	
	return frame;
}

GtkWidget* new_text_view(const char* title, GtkTextView** result) {
	GtkWidget *frame, *scrolled_window, *text_view;
	
	frame = gtk_frame_new(title);
	gtk_container_set_border_width(GTK_CONTAINER(frame), 4);
	gtk_widget_show(frame);

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 5);
	gtk_widget_show(scrolled_window);
	gtk_container_add(GTK_CONTAINER(frame), scrolled_window);

	text_view = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
	gtk_widget_show(text_view);
	gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);

	if (result)
		*result = GTK_TEXT_VIEW(text_view);
	
	return frame;
}

gboolean window_delete_event(GtkWidget* widget, GdkEvent* event, gpointer data) {
	return FALSE;
}

void crypted_text_changed(GtkTextBuffer* buffer, gpointer data) {
	char* crypted = get_crypted_text();
	analyze_frequencies(crypted);
	update_plain_text(crypted);
	g_free(crypted);
}

char* get_crypted_text() {
	GtkTextIter start, end;
	GtkTextBuffer* buffer;

	buffer = gtk_text_view_get_buffer(crypted_text_view);
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	
	return gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
}

void frequency_walker(gpointer key, gpointer value, gpointer user_data) {
	GtkListStore* store = GTK_LIST_STORE(user_data);
	GtkTreeIter pos;
	char frequency[256], *plain = decrypt_text(g_strdup(key));

	snprintf(frequency, sizeof (frequency), "%d (%.1f%%)", (int)value, (100.0f * (int)value) / total_count); 
	
	gtk_list_store_append(store, &pos);
	gtk_list_store_set(store,
			   &pos,
			   COL_TEXT, key,
			   COL_SUBSTITUTE, plain,
			   COL_FREQUENCY, frequency,
			   COL_FREQUENCY_INT, value,
			   -1);

	g_free(plain);

	//g_print("%s: %s\n", key, frequency);
}

void analyze_frequencies(const char* crypted) {
	const char* p;
	char bigram[3] = "", trigram[4] = "";
	int freq;
	GHashTable *letter_freq, *bigram_freq, *trigram_freq;
	int total_letters = 0, total_bigrams = 0, total_trigrams = 0;
	GtkListStore* store;
	
	letter_freq = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	bigram_freq = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	trigram_freq = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	for (p = crypted; *p; ++p) {
		char letter[] = { *p, '\0' };

		if (isalpha(*p)) {
			freq = (int)g_hash_table_lookup(letter_freq, letter);
			g_hash_table_replace(letter_freq, g_strdup(letter), (gpointer)(freq + 1));
			++total_letters;
		}

		bigram[0] = bigram[1];
		bigram[1] = *p;
		
		if (isalpha(bigram[0]) && isalpha(bigram[1])) {
			freq = (int)g_hash_table_lookup(bigram_freq, bigram);
			g_hash_table_replace(bigram_freq, g_strdup(bigram), (gpointer)(freq + 1));
			++total_bigrams;
		}
		
		trigram[0] = trigram[1];
		trigram[1] = trigram[2];
		trigram[2] = *p;
		
		if (isalpha(trigram[0]) && isalpha(trigram[1]) && isalpha(trigram[2])) {
			freq = (int)g_hash_table_lookup(trigram_freq, trigram);
			g_hash_table_replace(trigram_freq, g_strdup(trigram), (gpointer)(freq + 1));
			++total_trigrams;
		}
	}

	//g_print("\n\n\n\nLetter frequencies:\n");
        store = GTK_LIST_STORE(gtk_tree_view_get_model(letter_freq_table));        
	gtk_list_store_clear(store);
	total_count = total_letters;
	g_hash_table_foreach(letter_freq, frequency_walker, store);
	
	//g_print("\nBigram frequencies:\n");
        store = GTK_LIST_STORE(gtk_tree_view_get_model(bigram_freq_table));        
	gtk_list_store_clear(store);
	total_count = total_bigrams;
        g_hash_table_foreach(bigram_freq, frequency_walker, store);
		
	//g_print("\nTrigram frequencies:\n");
        store = GTK_LIST_STORE(gtk_tree_view_get_model(trigram_freq_table));    
	gtk_list_store_clear(store);
	total_count = total_trigrams;
	g_hash_table_foreach(trigram_freq, frequency_walker, store);

	g_hash_table_destroy(letter_freq);
	g_hash_table_destroy(bigram_freq);
	g_hash_table_destroy(trigram_freq);
}

void update_plain_text(char* crypted) {
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(plain_text_view);
	GtkTextIter start, end;
	int i;
	char* plain;

	plain = g_strdup(crypted);
	decrypt_text(plain);	
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(plain_text_view), plain, strlen(plain));
		
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_buffer_remove_all_tags(buffer, &start, &end);

	for (i = 0; crypted[i]; ++i) {
		if (mapping[(int)crypted[i]] <= 0 || mapping[(int)crypted[i]] == crypted[i]) {
			gtk_text_buffer_get_iter_at_offset(buffer, &start, i);
			while (crypted[i] && (mapping[(int)crypted[i]] <= 0 || mapping[(int)crypted[i]] == crypted[i]))
				++i;
			gtk_text_buffer_get_iter_at_offset(buffer, &end, i);
			gtk_text_buffer_apply_tag(buffer, tag, &start, &end);
		}	
	}
}

char* decrypt_text(char* crypted) {
	char* p;
	for (p = crypted; *p; ++p) {
		if (mapping[(int)*p] > 0)
			*p = mapping[(int)*p];
	}
	return crypted;
}

gboolean update_substitute(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data) {
	char *crypted;
	gtk_tree_model_get(model, iter, COL_TEXT, &crypted, -1);
	gtk_list_store_set(GTK_LIST_STORE(model), iter, COL_SUBSTITUTE, decrypt_text(crypted), -1);
	g_free(crypted);
	return FALSE;
}

void mapping_edited(GtkCellRendererText* renderer, char* row, char* plain, gpointer user_data) {
	GtkTreeModel* model = GTK_TREE_MODEL(user_data);
	GtkTreePath* path = gtk_tree_path_new_from_string(row);
	GtkTreeIter iter;
	char *crypted, *p, *q;
	
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_TEXT, &crypted, -1);
	
	for (p = crypted, q = plain; *p && *q; ++p, ++q)
		mapping[(int)*p] = *q;
	
	gtk_tree_path_free(path);
	g_free(crypted);
	
	gtk_tree_model_foreach(gtk_tree_view_get_model(letter_freq_table), update_substitute, NULL);
	gtk_tree_model_foreach(gtk_tree_view_get_model(bigram_freq_table), update_substitute, NULL);
	gtk_tree_model_foreach(gtk_tree_view_get_model(trigram_freq_table), update_substitute, NULL);
	
        crypted = get_crypted_text();
	update_plain_text(crypted);
        g_free(crypted);

	/*
	for (i = 'a'; i <= 'z'; ++i) {
		g_print("%c --> %c", i, mapping[i]);
		g_print("\n");
	}

	for (i = 'A'; i <= 'Z'; ++i) {
		g_print("%c --> %c", i, mapping[i]);
		g_print("\n");
	}
	*/
}

void open_file_dialog(GtkWidget* widget, gpointer user_data) {
	GtkWidget* file = gtk_file_selection_new("Open File");
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(file)->ok_button), "clicked", G_CALLBACK(open_file), file);
	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(file)->cancel_button), "clicked", G_CALLBACK(gtk_widget_destroy), G_OBJECT(file));
	gtk_widget_show(file);
}

void save_file_dialog(GtkWidget* widget, gpointer user_data) {
	GtkWidget* file = gtk_file_selection_new("Save File");
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(file)->ok_button), "clicked", G_CALLBACK(save_file), file);
	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(file)->cancel_button), "clicked", G_CALLBACK(gtk_widget_destroy), G_OBJECT(file));
	gtk_widget_show(file);
}

void open_file(GtkWidget* widget, gpointer user_data) {
	GtkFileSelection* file_selection = GTK_FILE_SELECTION(user_data);
	const char* name = gtk_file_selection_get_filename(file_selection);
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(crypted_text_view);
	int off = 0;
	GtkTextIter start, end;
	
	gtk_widget_destroy(GTK_WIDGET(file_selection));

	FILE* file = fopen(name, "r");
	if (!file)
		return;

	memset(mapping, 0, sizeof (mapping));

	while (!feof(file)) {
		char buf[32];
		int a, b;
		fgets(buf, sizeof (buf), file);
		if (buf[0] == '-')
			break;
		sscanf(buf, "%c -> %c", &a, &b);
		mapping[a] = b;
	}

	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_buffer_delete(buffer, &start, &end);

	while (!feof(file)) {
		char buf[1024];
		int len;
		fgets(buf, sizeof (buf), file);
		len = strlen(buf);
		gtk_text_buffer_get_iter_at_offset(buffer, &start, off);
		gtk_text_buffer_insert(buffer, &start, buf, len);
		off += len;
	}
	fclose(file);
}

void save_file(GtkWidget* widget, gpointer user_data) {
	GtkFileSelection* file_selection = GTK_FILE_SELECTION(user_data);
	const char* name = gtk_file_selection_get_filename(file_selection);
	FILE* file = fopen(name, "w");
	int i;

	gtk_widget_destroy(GTK_WIDGET(file_selection));

	if (!file)
		return;
	for (i = 0; i < 256; ++i) {
		if (mapping[i] && mapping[i] != i)
			fprintf(file, "%c -> %c\n", i, mapping[i]);
	}
	fprintf(file, "-----\n");
	fputs(get_crypted_text(), file);
	fclose(file);
}
