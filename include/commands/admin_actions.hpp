#pragma once

bool is_banned(const std::string &growid);
void ban(ENetEvent& event, const std::string_view text);
void unban(ENetEvent& event, const std::string_view text);
void give(ENetEvent& event, const std::string_view text);
