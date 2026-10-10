#pragma once

void home(ENetEvent& event, const std::string_view text);
void sethome(ENetEvent& event, const std::string_view text);
void msg(ENetEvent& event, const std::string_view text);
void online(ENetEvent& event, const std::string_view text);
