#pragma once

struct LockRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

float lock_icon_box_size();

float lock_card_width(float output_h);
float lock_card_height(float output_h);
float lock_center_scale(float output_h);

void lock_columns(float card_w, float card_h, float center_w,
                     LockRect &left, LockRect &center,
                     LockRect &right);

float lock_side_card_height(float column_h);

float lock_content_height(float clock_h, float date_h, float message_h);

int lock_fetch_colour_count(float available_w, int max_count);

float lock_dot_row_width(int count);
float lock_dot_x(int index, int count, float field_width);

void lock_panel_origin(float output_w, float output_h, float panel_w,
                          float panel_h, float &out_x, float &out_y);
