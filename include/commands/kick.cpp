#include "pch.hpp"
#include "commands/__command.hpp"
#include "action/quit.hpp"
#include "tools/string.hpp"
#include "kick.hpp"

#include <fstream>

/* @note admin list: one growid per line in admins.txt (next to the server binary).
 *       Accounts in this file may use moderator commands like /kick. */
bool is_admin(const std::string &growid)
{
    std::ifstream file("admins.txt");
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line))
    {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(line.begin());
        if (line == growid) return true;
    }
    return false;
}

void kick(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    if (!is_admin(pPeer->growid))
    {
        send_action(*event.peer, "log", "msg|`4You do not have permission to use this command.``");
        return;
    }

    const std::string_view name = text.size() >= strlen("kick ") ? text.substr(strlen("kick ")) : text;
    std::string target{ name };
    while (!target.empty() && (target.back() == ' ' || target.back() == '\r')) target.pop_back();
    if (target.empty())
    {
        send_action(*event.peer, "log", "msg|`4Usage:`` /kick <player name>");
        return;
    }

    bool kicked = false;
    peers("", PEER_ALL, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->growid != target) return;
        if (pOther->user_id == pPeer->user_id) return;

        kicked = true;
        send_action(peer, "log", std::format("msg|`4You have been kicked by {}``", pPeer->growid));
        send_varlist(&peer, { "OnKicked", std::format("kicked by {} (moderator)", pPeer->growid), 0, 0 });
        enet_peer_disconnect_later(&peer, 0); // @note server-side kick -> client sees the close
    });

    if (!kicked)
        send_action(*event.peer, "log", std::format("msg|`4`w{}`` is not online.``", target));
    else
        send_action(*event.peer, "log", std::format("msg|Kicked `w{}``.", target));
}
