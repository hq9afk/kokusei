#pragma once

struct PenanceRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

float penance_icon_box_size();

float penance_card_width(float output_h);
float penance_card_height(float output_h);
float penance_center_scale(float output_h);

void penance_columns(float card_w, float card_h, float center_w,
                     PenanceRect &left, PenanceRect &center,
                     PenanceRect &right);

float penance_side_card_height(float column_h);

float penance_content_height(float clock_h, float date_h, float message_h);

int penance_fetch_colour_count(float available_w, int max_count);

float penance_dot_row_width(int count);
float penance_dot_x(int index, int count, float field_width);

void penance_panel_origin(float output_w, float output_h, float panel_w,
                          float panel_h, float &out_x, float &out_y);
