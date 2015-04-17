#include <pebble.h>

static const uint16_t EDGE = 8;
static const uint16_t THICKNESS = 10;
static const uint16_t OFFSET = 4;
	
static Window *s_main_window = NULL;
static Layer *s_clock_layer_secs = NULL;
static Layer *s_clock_layer_mins = NULL;
static Layer *s_clock_layer_hours = NULL;

static GPath *s_hand_path_sec;
static GPathInfo SECOND_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 15},
		{6, 15},
		{6, -38},
		{-6,  -38},
	}
};
static GPath *s_hand_path_min;
static GPathInfo MINUTE_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 15},
		{6, 15},
		{6, -53},
		{-6,  -53},
	}
};
static GPath *s_hand_path_hour;
static GPathInfo HOUR_HAND_POINTS = {
	4,
	(GPoint []) {
		{-6, 15},
		{6, 15},
		{6, -68},
		{-6,  -68},
	}
};

static GRect s_clock_bounds;
static GPoint s_clock_center;

static struct tm *the_time;

static void draw_clock_layer_hours(Layer *layer, GContext *ctx) {
	uint16_t offset = 1;
		
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, s_clock_center, s_clock_center.x - offset);
	graphics_context_set_fill_color(ctx, GColorWhite);
	gpath_rotate_to(s_hand_path_hour, TRIG_MAX_ANGLE * the_time->tm_hour / 12);
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
