#include "pch.hpp"
#include "commands/__command.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "action/join_request.hpp"
#include "action/quit_to_exit.hpp"
#include "tools/string.hpp"
#include "social.hpp"

void home(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    if (pPeer->home_world.empty())
    {
        send_action(*event.peer, "log", "msg|`4You have not set a home yet. Use `/sethome` while in a world you own.``");
        return;
    }
    if (pPeer->home_world == pPeer->recent_worlds.back())
    {
        send_action(*event.peer, "log", "msg|`4You are already in your home world.``");
        return;
    }
    send_varlist(event.peer, { "OnSetFreezeState", 1 });
    send_action(*event.peer, "log", std::format("msg|Magically warping to your home `5{}``...", pPeer->home_world));
    action::quit_to_exit(event, "", true);
    action::join_request(event, "", pPeer->home_world);
}

void sethome(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    auto it = std::ranges::find(worlds, pPeer->recent_worlds.back(), &::world::name);
    if (it == worlds.end() || it->owner != pPeer->user_id)
    {
        send_action(*event.peer, "log", "msg|`4You can only set a home in a world you own.``");
        return;
    }
    pPeer->home_world = it->name;
    send_action(*event.peer, "log", std::format("msg|Home world set to `5{}``.", it->name));
}

void msg(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const std::string_view rest = text.size() >= strlen("msg ") ? text.substr(strlen("msg ")) : text;
    const std::size_t sp = rest.find(' ');
    if (sp == std::string_view::npos)
    {
        send_action(*event.peer, "log", "msg|`4Usage:`` /msg <player> <message>");
        return;
    }
    const std::string target{ rest.substr(0, sp) };
    const std::string body{ rest.substr(sp + 1) };

    bool sent = false;
    peers("", PEER_ALL, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->growid != target) return;
        if (pOther->user_id == pPeer->user_id) return;

        on::ConsoleMessage(&peer, std::format("`$PM from `w{}```: {}", pPeer->display_growid, body));
        sent = true;
    });

    if (!sent)
        send_action(*event.peer, "log", std::format("msg|`4{}`` is not online.", target));
}

void online(ENetEvent& event, const std::string_view text)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    std::vector<std::string> names{ pPeer->growid }; // @note include self
    peers("", PEER_ALL, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->user_id != pPeer->user_id)
            names.emplace_back(pOther->growid);
    });

    send_action(*event.peer, "log", std::format(
        "msg|`w{}`` player(s) online: `` {}",
        names.size(), join(names, ", ")
    ));
}
