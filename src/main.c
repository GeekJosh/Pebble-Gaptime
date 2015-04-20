#include <pebble.h>
	
#define KEY_INVERT 0
#define KEY_TEXT_TIME 1

static const uint16_t EDGE = 8;
static const uint16_t THICKNESS = 10;
static const uint16_t OFFSET = 4;
	
static Window *s_main_window = NULL;

static Layer *s_clock_layer_secs = NULL;
static Layer *s_clock_layer_mins = NULL;
static Layer *s_clock_layer_hours = NULL;

static TextLayer *s_text_time = NULL;

static InverterLayer *s_layer_invert = NULL;

static GPath *s_hand_path_sec;
static GPathInfo SECOND_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 0},
		{6, 0},
		{6, -38},
		{-6,  -38},
	}
};
static GPath *s_hand_path_min;
static GPathInfo MINUTE_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 0},
		{6, 0},
		{6, -53},
		{-6,  -53},
	}
};
static GPath *s_hand_path_hour;
static GPathInfo HOUR_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 0},
		{6, 0},
		{6, -68},
		{-6,  -68},
	}
};

static GRect s_clock_bounds;
static GPoint s_clock_center;

static struct tm *the_time;

static void invert_face() {
	bool inverted = false;
	if(persist_exists(KEY_INVERT)) {
		inverted = persist_read_bool(KEY_INVERT);
	} else {
		persist_write_bool(KEY_INVERT, inverted);
	}
	
	if(inverted) {
		Layer *window_layer = window_get_root_layer(s_main_window);
		layer_add_child(window_layer, inverter_layer_get_layer(s_layer_invert));
	} else {
		layer_remove_from_parent(inverter_layer_get_layer(s_layer_invert));
	}
}

static void toggle_text_time() {
	bool showTextTime = false;
	if(persist_exists(KEY_TEXT_TIME)) {
		showTextTime = persist_read_bool(KEY_TEXT_TIME);
	} else {
		persist_write_bool(KEY_TEXT_TIME, showTextTime);
	}
	
	if(showTextTime) {
		Layer *window_layer = window_get_root_layer(s_main_window);
		layer_add_child(window_layer, text_layer_get_layer(s_text_time));
	} else {
		layer_remove_from_parent(text_layer_get_layer(s_text_time));
	}
}

static void in_recv_handler(DictionaryIterator *iterator, void *context) {
	//Get tuple
	Tuple *t = dict_read_first(iterator);
	
	if(t) {
		switch (t->key) {
			case KEY_INVERT:
				if(strcmp(t->value->cstring, "on") == 0) {
					persist_write_bool(KEY_INVERT, true);
				} else if (strcmp(t->value->cstring, "off") == 0) {
					persist_write_bool(KEY_INVERT, false);
				}
				invert_face();
				break;
			case KEY_TEXT_TIME:
				if(strcmp(t->value->cstring, "on") == 0) {
					persist_write_bool(KEY_TEXT_TIME, true);
				} else if (strcmp(t->value->cstring, "off") == 0) {
					persist_write_bool(KEY_TEXT_TIME, false);
				}
				toggle_text_time();
				break;
		}
	}
}

static void draw_clock_layer_hours(Layer *layer, GContext *ctx) {
	uint16_t offset = 1;
		
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_hour, TRIG_MAX_ANGLE * (((the_time->tm_hour % 12) * 6) + (the_time->tm_min / 10)) / (12 * 6));
	gpath_draw_filled(ctx, s_hand_path_hour);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset - THICKNESS);
}

static void draw_clock_layer_mins(Layer *layer, GContext *ctx) {
	uint16_t offset = 1 + THICKNESS + OFFSET;
	
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_min, TRIG_MAX_ANGLE * the_time->tm_min / 60);
	gpath_draw_filled(ctx, s_hand_path_min);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset - THICKNESS);
}

static void draw_clock_layer_secs(Layer *layer, GContext *ctx) {
	uint16_t offset = 1 + (THICKNESS * 2) + (OFFSET * 2);
	
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_sec, TRIG_MAX_ANGLE * the_time->tm_sec / 60);
	gpath_draw_filled(ctx, s_hand_path_sec);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset - THICKNESS);
}

//updates time display
static void updateTime(TimeUnits unitsChanged) {
	//get time
	time_t temp = time(NULL);
	the_time = localtime(&temp);
	
	//instruct pebble to redraw neccessary layers
	if((unitsChanged & HOUR_UNIT) != 0) {
		layer_mark_dirty(s_clock_layer_hours);
	}
	
	if((unitsChanged & MINUTE_UNIT) != 0) {
		layer_mark_dirty(s_clock_layer_mins);
		
		//also redraw hour hand every 10 minutes
		//this helps clarify difference at the hour i.e. 11:59 or 11:00
		if(the_time->tm_min % 10 == 0) {
			updateTime(HOUR_UNIT);
		}
		
		//update text time layer
		static char buffer[] = "00:00";
		// Write the current hours and minutes into the buffer
		if(clock_is_24h_style() == true) {
    		// Use 24 hour format
    		strftime(buffer, sizeof("00:00"), "%H:%M", the_time);
		} else {
			// Use 12 hour format
			strftime(buffer, sizeof("00:00"), "%I:%M", the_time);
		}
		text_layer_set_text(s_text_time, buffer);
	}
	
	layer_mark_dirty(s_clock_layer_secs);
}

static void tick_handler(struct tm *tick_time, TimeUnits unitsChanged) {
	updateTime(unitsChanged);
}

static void main_window_load(Window *window) {
	//get root layer of window
	Layer *window_layer = window_get_root_layer(window);
	
	//get window dimensions
	GRect bounds = layer_get_bounds(window_layer);
	
	//add drawing layers
	s_clock_layer_hours = layer_create(GRect(EDGE, EDGE, bounds.size.w - (EDGE * 2), bounds.size.h - (EDGE * 2)));
	layer_set_update_proc(s_clock_layer_hours, draw_clock_layer_hours);
	layer_add_child(window_layer, s_clock_layer_hours);
	
	s_clock_layer_mins = layer_create(GRect(EDGE, EDGE, bounds.size.w - (EDGE * 2), bounds.size.h - (EDGE * 2)));
	layer_set_update_proc(s_clock_layer_mins, draw_clock_layer_mins);
	layer_add_child(window_layer, s_clock_layer_mins);
	
	s_clock_layer_secs = layer_create(GRect(EDGE, EDGE, bounds.size.w - (EDGE * 2), bounds.size.h - (EDGE * 2)));
	layer_set_update_proc(s_clock_layer_secs, draw_clock_layer_secs);
	layer_add_child(window_layer, s_clock_layer_secs);
	
	//init text time layer
	GSize max_size = graphics_text_layout_get_content_size(
		"00:00",
		fonts_get_system_font(FONT_KEY_GOTHIC_14), 
		GRect(0, 0, bounds.size.w, bounds.size.h), 
		GTextOverflowModeTrailingEllipsis, 
		GTextAlignmentCenter
	);
	s_text_time = text_layer_create(GRect(
		(bounds.size.w / 2) - (max_size.w / 2), 
		(bounds.size.h / 2) - (max_size.h / 2), 
		max_size.w, 
		max_size.h)
	);
	text_layer_set_background_color(s_text_time, GColorClear);
	text_layer_set_text_color(s_text_time, GColorBlack);
	text_layer_set_font(s_text_time, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_alignment(s_text_time, GTextAlignmentCenter);
	toggle_text_time();
	
	//init inverter layer
	s_layer_invert = inverter_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
	invert_face();
	
	//store clock layer bounds
	s_clock_bounds = layer_get_bounds(s_clock_layer_secs);
	s_clock_center = grect_center_point(&s_clock_bounds);
	
	//init hand paths
	s_hand_path_hour = gpath_create(&HOUR_HAND_POINTS);
	gpath_move_to(s_hand_path_hour, s_clock_center);
	
	s_hand_path_min = gpath_create(&MINUTE_HAND_POINTS);
	gpath_move_to(s_hand_path_min, s_clock_center);
	
	s_hand_path_sec = gpath_create(&SECOND_HAND_POINTS);
	gpath_move_to(s_hand_path_sec, s_clock_center);
}

static void main_window_unload(Window *window) {
	layer_destroy(s_clock_layer_hours);
	layer_destroy(s_clock_layer_mins);
	layer_destroy(s_clock_layer_secs);
	
	text_layer_destroy(s_text_time);
	
	inverter_layer_destroy(s_layer_invert);
	
	gpath_destroy(s_hand_path_hour);
	gpath_destroy(s_hand_path_min);
	gpath_destroy(s_hand_path_sec);
}

static void init(void) {
 // create main window and assign to pointer 
	s_main_window = window_create();
	
	//assign main window handlers
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});
	
	//show the main window with animation
	window_stack_push(s_main_window, true);
	
	//set initial time
	updateTime(SECOND_UNIT | MINUTE_UNIT | HOUR_UNIT);
	
	//register tick event service
	tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
	
	//register service to receive config from phone
	app_message_register_inbox_received((AppMessageInboxReceived) in_recv_handler);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
	//destroy main window
	window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
