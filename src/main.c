#include <pebble.h>

#define KEY_INVERT 0
#define KEY_TEXT_TIME 1
#define KEY_HAND_ORDER 2
#define KEY_INVERT_START_MIN 3
#define KEY_INVERT_START_HOUR 4
#define KEY_INVERT_END_MIN 5
#define KEY_INVERT_END_HOUR 6
#define KEY_UPDATE_SUNTIMES 7

#define PS_INVERT_TIME 100
	
#define INVERT_ON_STR "on"
#define INVERT_OFF_STR "off"
#define INVERT_TIME_STR "time"
#define INVERT_SUNRISE_STR "sunrise"
#define INVERT_SUNSET_STR "sunset"

static const uint16_t EDGE = 8;
static const uint16_t THICKNESS = 10;
static const uint16_t OFFSET = 4;

static Window *s_main_window = NULL;

static char s_hand_order[4] = "SMH\0";
static int32_t s_hand_angle[3] = { 0, 0, 0 };

static bool s_inverted = false;
static bool s_show_text_time = false;

static Layer *s_clock_layer_inner = NULL;
static Layer *s_clock_layer_center = NULL;
static Layer *s_clock_layer_outer = NULL;

static TextLayer *s_text_time = NULL;

static InverterLayer *s_layer_invert = NULL;

static GPath *s_hand_path_inner;
static GPathInfo INNER_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 0},
		{6, 0},
		{6, -38},
		{-6,  -38},
	}
};
static GPath *s_hand_path_center;
static GPathInfo CENTER_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 0},
		{6, 0},
		{6, -53},
		{-6,  -53},
	}
};
static GPath *s_hand_path_outer;
static GPathInfo OUTER_HAND_POINTS = {
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

typedef enum {
	OFF,
	ON,
	TIME,
	SUNRISE,
	SUNSET
} InvertTimeType;

typedef struct InvertTime {
	uint8_t updateMin;
	uint8_t updateHour;
	uint8_t onMin;
	uint8_t onHour;
	uint8_t offMin;
	uint8_t offHour;
	InvertTimeType type;
} InvertTime;

static InvertTime invertTime;

struct tm *the_time;

static int get_hand_order(char unit) {
	for(int i = 0; i < 3; i++) {
		if(s_hand_order[i] == unit) {
			return i;
		}
	}
	return -1;
}

static void updateSunTimes() {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	int value = 1;
	dict_write_int(iter, KEY_UPDATE_SUNTIMES, &value, sizeof(value), true);
	app_message_outbox_send();
}

static void invert_face() {
	layer_set_hidden(inverter_layer_get_layer(s_layer_invert), !s_inverted);
}

static void toggle_text_time() {
	layer_set_hidden(text_layer_get_layer(s_text_time), !s_show_text_time);
}

static void draw_clock_layer_outer(Layer *layer, GContext *ctx) {
	uint16_t offset = 1;

	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_outer, s_hand_angle[0]);
	gpath_draw_filled(ctx, s_hand_path_outer);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset - THICKNESS);
}

static void draw_clock_layer_center(Layer *layer, GContext *ctx) {
	uint16_t offset = 1 + THICKNESS + OFFSET;

	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_center, s_hand_angle[1]);
	gpath_draw_filled(ctx, s_hand_path_center);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset - THICKNESS);
}

static void draw_clock_layer_inner(Layer *layer, GContext *ctx) {
	uint16_t offset = 1 + (THICKNESS * 2) + (OFFSET * 2);

	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_inner, s_hand_angle[2]);
	gpath_draw_filled(ctx, s_hand_path_inner);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset - THICKNESS);
}

static void draw_hand(int hand) {
	switch (hand) {
		case 0:
			layer_mark_dirty(s_clock_layer_outer);
			break;
		case 1:
			layer_mark_dirty(s_clock_layer_center);
			break;
		case 2:
			layer_mark_dirty(s_clock_layer_inner);
			break;
	}
}

//updates time display
static void updateTime(TimeUnits unitsChanged) {
	//get time
	time_t temp = time(NULL);
	the_time = localtime(&temp);

	//hand to update
	int hand;
	
	//update SunTimes if enabled and day changes
	if((invertTime.type == SUNRISE || invertTime.type == SUNSET) && (unitsChanged & DAY_UNIT) != 0) {
		updateSunTimes();
	}
	
	//instruct pebble to redraw neccessary layers
	if((unitsChanged & HOUR_UNIT) != 0 || the_time->tm_min % 10 == 0) {
		hand = get_hand_order('H');
		if(hand > -1) {
			s_hand_angle[hand] = TRIG_MAX_ANGLE * (((the_time->tm_hour % 12) * 6) + (the_time->tm_min / 10)) / (12 * 6);
			draw_hand(hand);
		}
	}

	if((unitsChanged & MINUTE_UNIT) != 0) {
		hand = get_hand_order('M');
		if(hand > -1) {
			s_hand_angle[hand] = TRIG_MAX_ANGLE * the_time->tm_min / 60;
			draw_hand(hand);
		}

		//update text time layer
		static char buffer[] = "00:00";
		// Write the current hours and minutes into the buffer
		if(clock_is_24h_style() == true) {
    		// Use 24 hour format
    		strftime(buffer, sizeof(buffer), "%H:%M", the_time);
		} else {
			// Use 12 hour format
			strftime(buffer, sizeof(buffer), "%I:%M", the_time);
		}
		text_layer_set_text(s_text_time, buffer);
		
		//invert face if neccessary
		if(invertTime.type == TIME || invertTime.type == SUNRISE || invertTime.type == SUNSET) {
			s_inverted = ((the_time->tm_hour * 60) + the_time->tm_min >= (invertTime.onHour * 60) + invertTime.onMin && (the_time->tm_hour * 60) + the_time->tm_min < (invertTime.offHour *60) + invertTime.offMin);
			invert_face();
		} 
		
		//try to update SunTimes every 20 minutes if not updated in last 12 hours
		if((invertTime.type == SUNRISE || invertTime.type == SUNSET) && (invertTime.updateHour == 255 || (the_time->tm_min % 20 == 0 && ((the_time->tm_hour * 60) + the_time->tm_min) - ((invertTime.updateHour * 60) + invertTime.updateMin) >= 60))) {
			updateSunTimes();
		}
	}

	hand = get_hand_order('S');
	if(hand > -1) {
		s_hand_angle[hand] = TRIG_MAX_ANGLE * the_time->tm_sec / 60;
		draw_hand(hand);
	}
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
	s_clock_layer_outer = layer_create(GRect(EDGE, EDGE, bounds.size.w - (EDGE * 2), bounds.size.h - (EDGE * 2)));
	layer_set_update_proc(s_clock_layer_outer, draw_clock_layer_outer);
	layer_add_child(window_layer, s_clock_layer_outer);

	s_clock_layer_center = layer_create(GRect(EDGE, EDGE, bounds.size.w - (EDGE * 2), bounds.size.h - (EDGE * 2)));
	layer_set_update_proc(s_clock_layer_center, draw_clock_layer_center);
	layer_add_child(window_layer, s_clock_layer_center);

	s_clock_layer_inner = layer_create(GRect(EDGE, EDGE, bounds.size.w - (EDGE * 2), bounds.size.h - (EDGE * 2)));
	layer_set_update_proc(s_clock_layer_inner, draw_clock_layer_inner);
	layer_add_child(window_layer, s_clock_layer_inner);

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
	layer_add_child(window_layer, text_layer_get_layer(s_text_time));
	toggle_text_time();

	//init inverter layer
	s_layer_invert = inverter_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h));
	layer_add_child(window_layer, inverter_layer_get_layer(s_layer_invert));
	invert_face();

	//store clock layer bounds
	s_clock_bounds = layer_get_bounds(s_clock_layer_outer);
	s_clock_center = grect_center_point(&s_clock_bounds);

	//init hand paths
	s_hand_path_outer = gpath_create(&OUTER_HAND_POINTS);
	gpath_move_to(s_hand_path_outer, s_clock_center);

	s_hand_path_center = gpath_create(&CENTER_HAND_POINTS);
	gpath_move_to(s_hand_path_center, s_clock_center);

	s_hand_path_inner = gpath_create(&INNER_HAND_POINTS);
	gpath_move_to(s_hand_path_inner, s_clock_center);
}

static void main_window_unload(Window *window) {
	layer_destroy(s_clock_layer_outer);
	layer_destroy(s_clock_layer_center);
	layer_destroy(s_clock_layer_inner);

	text_layer_destroy(s_text_time);

	inverter_layer_destroy(s_layer_invert);

	gpath_destroy(s_hand_path_outer);
	gpath_destroy(s_hand_path_center);
	gpath_destroy(s_hand_path_inner);
}

static void in_recv_handler(DictionaryIterator *iterator, void *context) {
	//Get tuple
	Tuple *t = dict_read_first(iterator);

	while(t != NULL) {
		switch (t->key) {
			case KEY_INVERT:
				if (strcmp(t->value->cstring, "on") == 0) {
					invertTime.type = ON;
				} else if(strcmp(t->value->cstring, "off") == 0) {
					invertTime.type = OFF;
				} else if(strcmp(t->value->cstring, "time") == 0) {
					invertTime.type = TIME;
				} else if(strcmp(t->value->cstring, "sunrise") == 0) {
					invertTime.type = SUNRISE;
				} else if(strcmp(t->value->cstring, "sunset") == 0) {
					invertTime.type = SUNSET;
				}
				break;
			case KEY_TEXT_TIME:
				if(strcmp(t->value->cstring, "on") == 0) {
					s_show_text_time = true;
				} else {
					s_show_text_time = false;
				}
				toggle_text_time();
				break;
			case KEY_HAND_ORDER:
				for(int i =0; i < 3 && i < (int)strlen(t->value->cstring); i++) {
					s_hand_order[i] = t->value->cstring[i];
				}
				updateTime(0xFF);
				break;
			case KEY_INVERT_END_MIN:
				invertTime.offMin = t->value->uint8;
				break;
			case KEY_INVERT_END_HOUR:
				invertTime.offHour = t->value->uint8;
				break;
			case KEY_INVERT_START_MIN:
				invertTime.onMin = t->value->uint8;
				break;
			case KEY_INVERT_START_HOUR:
				invertTime.onHour = t->value->uint8;
				break;
			case KEY_UPDATE_SUNTIMES:
				invertTime.updateHour = the_time->tm_hour;
				invertTime.updateMin = the_time->tm_min;
				break;
		}

		t = dict_read_next(iterator);
	}
	
	//update invert face
	switch(invertTime.type) {
		case ON:
			s_inverted = true;
			invert_face();
			break;
		case OFF:
			s_inverted = false;
			invert_face();
			break;
		default:
			break;
	}
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init(void) {
	//initialize invertTime
	invertTime.offMin = 255;
	invertTime.offHour = 255;
	invertTime.onMin = 255;
	invertTime.onHour = 255;
	invertTime.updateMin = 255;
	invertTime.updateHour = 255;
	invertTime.type = OFF;
	
	// load user settings
	if(persist_exists(KEY_HAND_ORDER)) {
		persist_read_string(KEY_HAND_ORDER, s_hand_order, 4);
	}

	if(persist_exists(KEY_TEXT_TIME)) {
		s_show_text_time = persist_read_bool(KEY_TEXT_TIME);
	}

	if(persist_exists(KEY_INVERT)) {
		s_inverted = persist_read_bool(KEY_INVERT);
	}
	
	if(persist_exists(PS_INVERT_TIME)) {
		persist_read_data(PS_INVERT_TIME, &invertTime, sizeof(invertTime));
	}

	//register service to receive config from phone
	app_message_register_inbox_received((AppMessageInboxReceived)in_recv_handler);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

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
	updateTime(0xFF);

	//register tick event service
	tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
}

static void deinit(void) {
	//destroy main window
	window_destroy(s_main_window);

	//save user settings
	persist_write_bool(KEY_INVERT, s_inverted);
	persist_write_bool(KEY_TEXT_TIME, s_show_text_time);
	persist_write_string(KEY_HAND_ORDER, s_hand_order);
	persist_write_data(PS_INVERT_TIME, &invertTime, sizeof(invertTime));
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
