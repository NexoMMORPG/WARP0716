#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
class map_session_data;
namespace native_stylist {
bool open(map_session_data*);
void sync(map_session_data*);
void map_loaded(map_session_data*);
void parse(int,map_session_data*);
int clamp_palette(int,int,int);
int clamp_hair(map_session_data*,int,int,int);
void quit(map_session_data*);
void apply_view(map_session_data*);
void refresh_views(map_session_data*);
int view(map_session_data*,int,int);
}
