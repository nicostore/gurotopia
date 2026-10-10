#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "trade.hpp"

namespace
{
    /* @note pending trade requests: target user_id -> requester growid */
    std::unordered_map<int, std::string> pending_requests;

    std::string trimmed(std::string_view v)
    {
        std::string s{ v };
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
        return s;
    }
}

void trade_command(ENetEvent& event, const std::string_view args)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    /* @note args arrives as text.substr(1) — still carries the "trade " prefix */
    const std::string_view stripped = args.size() >= strlen("trade ") ? args.substr(strlen("trade ")) : args;
    const std::string name = trimmed(stripped);
    if (name.empty())
    {
        on::ConsoleMessage(event.peer, "`4Usage:`` /trade <player name>");
        return;
    }

    bool found = false;
    peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->user_id == pPeer->user_id) return;
        if (pOther->growid != name) return;

        found = true;
        pending_requests[pOther->user_id] = pPeer->growid;

        create_dialog dialog;
        dialog.set_default_color("`o")
            .add_label_with_icon("big", std::format("`w{}`` wants to trade with you!", pPeer->growid), 18)
            .add_spacer("small")
            .add_textbox("Accept to start a trade. You can exchange items and world locks.")
            .add_spacer("small")
            .embed_data("type", "trade")
            .add_button("accept", "Accept")
            .add_button("decline", "Decline")
            .add_quick_exit();

        send_varlist(&peer, { "OnDialogRequest", dialog.end_dialog("close", "Close") });
        on::ConsoleMessage(event.peer, std::format("Trade request sent to `w{}``", pOther->growid));
    });

    if (!found)
        on::ConsoleMessage(event.peer, std::format("`4Player `w{}`` is not in this world.``", name));
}

void trade_dialog_return(ENetEvent& event, const ::hPipe& hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    const std::string &clicked = hPipe["buttonClicked"];

    auto it = pending_requests.find(pPeer->user_id);
    if (it == pending_requests.end())
    {
        on::ConsoleMessage(event.peer, "`4No pending trade request.``");
        return;
    }
    const std::string requester = it->second;

    if (clicked == "accept")
    {
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& peer)
        {
            ::peer *pOther = static_cast<::peer*>(peer.data);
            if (pOther->growid != requester) return;
            on::ConsoleMessage(&peer, std::format("`w{}`` accepted your trade request!``", pPeer->growid));
        });
        on::ConsoleMessage(event.peer, std::format("Trade with `w{}`` started! (item exchange coming soon)``", requester));
    }
    else if (clicked == "decline")
    {
        peers(pPeer->recent_worlds.back(), PEER_SAME_WORLD, [&](ENetPeer& peer)
        {
            ::peer *pOther = static_cast<::peer*>(peer.data);
            if (pOther->growid != requester) return;
            on::ConsoleMessage(&peer, std::format("`w{}`` declined your trade request.``", pPeer->growid));
        });
        on::ConsoleMessage(event.peer, "Trade request declined.");
    }

    pending_requests.erase(it);
}
