#include "pch.hpp"
#include "commands/__command.hpp"
#include "commands/kick.hpp"          // @note is_admin()
#include "onVariant/ConsoleMessage.hpp"
#include "tools/string.hpp"
#include "admin_actions.hpp"

#include <fstream>

namespace
{
    /* @note banned growids, one per line in banned.txt next to the binary */
    bool load_banned(std::vector<std::string> &out)
    {
        out.clear();
        std::ifstream file("banned.txt");
        if (!file.is_open()) return false;
        std::string line;
        while (std::getline(file, line))
        {
            while (!line.empty() && (line.back()=='\r'||line.back()=='\n'||line.back()==' ')) line.pop_back();
            while (!line.empty() && (line.front()==' '||line.front()=='\t')) line.erase(line.begin());
            if (line.empty() || line.front() == '#') continue;
            out.push_back(line);
        }
        return true;
    }

    void save_banned(const std::vector<std::string> &list)
    {
        std::ofstream file("banned.txt");
        for (const auto &name : list) file << name << "\n";
    }
}

bool is_banned(const std::string &growid)
{
    std::vector<std::string> list;
    if (!load_banned(list)) return false;
    return std::ranges::find(list, growid) != list.end();
}

/* /ban <name> — ban a player (persistent). Bans the growid; kicks if online. */
void ban(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!is_admin(pPeer->growid))
    {
        send_action(*event.peer, "log", "msg|`4You do not have permission to use this command.``");
        return;
    }

    std::string target{ text.size() >= strlen("ban ") ? text.substr(strlen("ban ")) : text };
    while (!target.empty() && (target.back()==' '||target.back()=='\r')) target.pop_back();
    if (target.empty())
    {
        send_action(*event.peer, "log", "msg|`4Usage:`` /ban <player name>");
        return;
    }
    if (target == pPeer->growid)
    {
        send_action(*event.peer, "log", "msg|`4You cannot ban yourself.``");
        return;
    }

    std::vector<std::string> list;
    load_banned(list);
    if (std::ranges::find(list, target) == list.end()) list.push_back(target);
    save_banned(list);
    send_action(*event.peer, "log", std::format("msg|Banned `w{}``.", target));

    /* kick if online */
    peers("", PEER_ALL, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->growid != target) return;
        send_action(peer, "log", "msg|`4You have been banned by a moderator.``");
        send_varlist(&peer, { "OnKicked", "You have been banned.", 0, 0 });
        enet_peer_disconnect_later(&peer, 0);
    });
}

/* /unban <name> — remove a ban. */
void unban(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!is_admin(pPeer->growid))
    {
        send_action(*event.peer, "log", "msg|`4You do not have permission to use this command.``");
        return;
    }

    std::string target{ text.size() >= strlen("unban ") ? text.substr(strlen("unban ")) : text };
    while (!target.empty() && (target.back()==' '||target.back()=='\r')) target.pop_back();
    if (target.empty())
    {
        send_action(*event.peer, "log", "msg|`4Usage:`` /unban <player name>");
        return;
    }

    std::vector<std::string> list;
    load_banned(list);
    auto it = std::ranges::find(list, target);
    if (it == list.end())
    {
        send_action(*event.peer, "log", std::format("msg|`4`w{}`` is not banned.``", target));
        return;
    }
    list.erase(it);
    save_banned(list);
    send_action(*event.peer, "log", std::format("msg|Unbanned `w{}``.", target));
}

/* /give <name> <itemID> <count> — admin gives items to a player. */
void give(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (!is_admin(pPeer->growid))
    {
        send_action(*event.peer, "log", "msg|`4You do not have permission to use this command.``");
        return;
    }

    std::string rest{ text.size() >= strlen("give ") ? text.substr(strlen("give ")) : text };
    std::vector<std::string> parts = readch(rest, ' ');
    if (parts.size() < 3)
    {
        send_action(*event.peer, "log", "msg|`4Usage:`` /give <name> <itemID> <count>");
        return;
    }

    const std::string &target = parts[0];
    int itemID = atoi(parts[1].c_str());
    int count  = atoi(parts[2].c_str());
    if (itemID <= 0 || itemID >= (int)items.size() || count <= 0 || count > 200)
    {
        send_action(*event.peer, "log", "msg|`4Invalid item or count (max 200).``");
        return;
    }

    bool found = false;
    peers("", PEER_ALL, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->growid != target) return;
        found = true;

        { ENetEvent e{}; e.peer = &peer; modify_item_inventory(e, ::slot((short)itemID, (short)count)); }
        { ENetEvent e{}; e.peer = &peer; send_inventory_state(e); }
        on::ConsoleMessage(&peer, std::format("`2You received `w{} x{}`` from a moderator!``",
                                              id_to_item((u_short)itemID).raw_name, count));
    });

    if (!found)
        send_action(*event.peer, "log", std::format("msg|`4`w{}`` is not online.``", target));
    else
        send_action(*event.peer, "log", std::format("msg|Gave `w{}`` x{} of `5{}``.", target, count, id_to_item((u_short)itemID).raw_name));
}
