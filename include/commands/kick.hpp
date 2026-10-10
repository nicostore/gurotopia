#pragma once

bool is_admin(const std::string &growid);
void kick(ENetEvent& event, const std::string_view text);
