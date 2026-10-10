#include "pch.hpp"
#include "tools/create_dialog.hpp"
#include "onVariant/ConsoleMessage.hpp"
#include "tools/string.hpp"
#include "trade.hpp"

#include <unordered_map>

namespace
{
    /* @note one active trade per pairing; sessions are cleaned up on completion/cancel */
    struct trade_session
    {
        int a_uid{0}; /* @note requester */
        int b_uid{0}; /* @note accepter */
        std::unordered_map<short, short> offer_a; /* @note itemID -> count */
        std::unordered_map<short, short> offer_b;
        bool ready_a{false};
        bool ready_b{false};
    };

    std::unordered_map<int, int> trade_partner; /* @note uid -> partner uid */
    std::unordered_map<int, trade_session> trade_sessions; /* @note key = min(a_uid,b_uid) */
    std::unordered_map<int, std::string> pending_requests; /* @note target uid -> requester growid */

    std::string trimmed(std::string_view v)
    {
        std::string s{ v };
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
        while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
        return s;
    }

    ENetPeer* find_peer(int user_id)
    {
        ENetPeer* found = nullptr;
        peers("", PEER_ALL, [&](ENetPeer& peer)
        {
            ::peer *p = static_cast<::peer*>(peer.data);
            if (p && p->user_id == user_id) found = &peer;
        });
        return found;
    }

    int session_key(int uid)
    {
        auto it = trade_partner.find(uid);
        return (it == trade_partner.end()) ? uid : std::min(uid, it->second);
    }

    void close_trade(int uid, const std::string &reason)
    {
        auto it = trade_partner.find(uid);
        if (it == trade_partner.end()) return;
        const int partner = it->second;
        const int key = std::min(uid, partner);

        ENetPeer* p1 = find_peer(uid);
        ENetPeer* p2 = find_peer(partner);
        if (p1) on::ConsoleMessage(p1, reason);
        if (p2 && p2 != p1) on::ConsoleMessage(p2, reason);

        trade_sessions.erase(key);
        trade_partner.erase(uid);
        trade_partner.erase(partner);
    }

    void send_trade_ui(ENetPeer& peer, const trade_session &ts, bool is_a)
    {
        ::peer *p = static_cast<::peer*>(peer.data);
        const int my_uid = p->user_id;
        const int partner_uid = is_a ? ts.b_uid : ts.a_uid;
        const auto &my_offer = is_a ? ts.offer_a : ts.offer_b;
        const auto &their_offer = is_a ? ts.offer_b : ts.offer_a;
        const bool my_ready = is_a ? ts.ready_a : ts.ready_b;
        const bool their_ready = is_a ? ts.ready_b : ts.ready_a;
        ENetPeer* partner_peer = find_peer(partner_uid);
        ::peer *partner = partner_peer ? static_cast<::peer*>(partner_peer->data) : nullptr;
        const std::string partner_name = partner ? partner->growid : "?";

        create_dialog d;
        d.set_default_color("`o")
            .add_label_with_icon("big", std::format("Trading with `w{}``", partner_name), 18)
            .add_spacer("small")
            .add_label("big", "`5You offer:``")
            .embed_data("type", "trade");

        if (my_offer.empty())
            d.add_smalltext("(nothing yet — pick items below)");
        else
            for (const auto &[id, cnt] : my_offer)
                d.add_label_with_icon("big", std::format("`w{}`` x{}", id_to_item(id).raw_name, cnt), id);

        d.add_spacer("small")
            .add_label("big", "`5They offer:``");
        if (their_offer.empty())
            d.add_smalltext("(nothing yet)");
        else
            for (const auto &[id, cnt] : their_offer)
                d.add_label_with_icon("big", std::format("`w{}`` x{}", id_to_item(id).raw_name, cnt), id);

        d.add_spacer("small")
            .add_item_picker("add", "Add item to offer", "Select the item you want to trade (one stack of 1 per tap)")
            .add_spacer("small")
            .add_layout_spacer("small")
            .add_button("ready", std::format("{}{}", my_ready ? "(ready) " : "", their_ready ? "Partner ready" : "Ready"));

        d.add_button("cancel_trade", "Cancel Trade")
            .add_quick_exit();

        send_varlist(&peer, { "OnDialogRequest", d.end_dialog("trade_ui", "Cancel Trade", "Cancel Trade") });
    }
}

void trade_command(ENetEvent& event, const std::string_view args)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const std::string_view stripped = args.size() >= strlen("trade ") ? args.substr(strlen("trade ")) : args;
    const std::string name = trimmed(stripped);
    if (name.empty())
    {
        on::ConsoleMessage(event.peer, "`4Usage:`` /trade <player name>");
        return;
    }
    if (trade_partner.contains(pPeer->user_id))
    {
        on::ConsoleMessage(event.peer, "`4You are already in a trade.``");
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
            .add_textbox("Accept to open the trade window. Both players must confirm to exchange items.")
            .add_spacer("small")
            .embed_data("type", "trade")
            .add_button("accept", "Accept")
            .add_button("decline", "Decline")
            .add_quick_exit();

        send_varlist(&peer, { "OnDialogRequest", dialog.end_dialog("trade", "Close") });
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
    ENetPeer* requester_peer = nullptr;
    peers("", PEER_ALL, [&](ENetPeer& peer)
    {
        ::peer *pOther = static_cast<::peer*>(peer.data);
        if (pOther->growid == requester) requester_peer = &peer;
    });

    if (clicked == "accept")
    {
        if (!requester_peer)
        {
            on::ConsoleMessage(event.peer, "`4The requester left.``");
            pending_requests.erase(it);
            return;
        }
        ::peer *pRequester = static_cast<::peer*>(requester_peer->data);
        if (trade_partner.contains(pRequester->user_id) || trade_partner.contains(pPeer->user_id))
        {
            on::ConsoleMessage(event.peer, "`4That player is already in a trade.``");
            pending_requests.erase(it);
            return;
        }

        const int key = std::min(pRequester->user_id, pPeer->user_id);
        auto &ts = trade_sessions[key];
        ts.a_uid = pRequester->user_id;
        ts.b_uid = pPeer->user_id;
        trade_partner[pRequester->user_id] = pPeer->user_id;
        trade_partner[pPeer->user_id] = pRequester->user_id;
        pending_requests.erase(it);

        on::ConsoleMessage(requester_peer, std::format("`w{}`` accepted your trade request!``", pPeer->growid));
        on::ConsoleMessage(event.peer, std::format("Trade with `w{}`` started!``", requester));

        send_trade_ui(*requester_peer, ts, true);
        send_trade_ui(*event.peer, ts, false);
    }
    else if (clicked == "decline")
    {
        if (requester_peer)
            on::ConsoleMessage(requester_peer, std::format("`w{}`` declined your trade request.``", pPeer->growid));
        on::ConsoleMessage(event.peer, "Trade request declined.");
        pending_requests.erase(it);
    }
    else
    {
        pending_requests.erase(it);
    }
}

void trade_ui_return(ENetEvent& event, const ::hPipe& hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);
    const std::string &clicked = hPipe["buttonClicked"];

    if (!trade_partner.contains(pPeer->user_id))
    {
        on::ConsoleMessage(event.peer, "`4That trade has ended.``");
        return;
    }

    const int key = session_key(pPeer->user_id);
    auto &ts = trade_sessions[key];
    const bool is_a = (ts.a_uid == pPeer->user_id);
    auto &my_offer = is_a ? ts.offer_a : ts.offer_b;
    auto &other_offer = is_a ? ts.offer_b : ts.offer_a;

    ENetPeer* partner_peer = find_peer(is_a ? ts.b_uid : ts.a_uid);
    ::peer *partner = partner_peer ? static_cast<::peer*>(partner_peer->data) : nullptr;
    if (!partner)
    {
        close_trade(pPeer->user_id, "`4Trade cancelled — partner left.``");
        return;
    }

    /* item picker selection: searchableItemListButton_<n>_<itemID>_<count> */
    if (clicked.starts_with("searchableItemListButton"))
    {
        std::vector<std::string> parts = readch(clicked, '_');
        if (parts.size() < 3) return;
        /* @note real wire format: searchableItemListButton_<index>_<itemID>_<count> */
        short itemID = static_cast<short>(atoi(parts[2].c_str()));
        if (itemID <= 0 || itemID >= static_cast<short>(items.size()))
        {
            itemID = static_cast<short>(atoi(parts[1].c_str())); // @note fallback for legacy clients
            if (itemID <= 0 || itemID >= static_cast<short>(items.size())) return;
        }

        /* ownership check */
        bool owns = false;
        for (const ::slot &s : pPeer->slots)
            if (s.id == itemID && s.count > 0) { owns = true; break; }
        if (!owns)
        {
            on::ConsoleMessage(event.peer, "`4You don't have that item.``");
            send_trade_ui(*event.peer, ts, is_a);
            return;
        }
        my_offer[itemID] += 1;
        ts.ready_a = ts.ready_b = false; /* @note any change re-opens confirmation */

        send_trade_ui(*event.peer, ts, is_a);
        if (partner_peer) send_trade_ui(*partner_peer, ts, !is_a);
        return;
    }

    if (clicked == "ready")
    {
        if (is_a) ts.ready_a = !ts.ready_a; else ts.ready_b = !ts.ready_b;

        if (ts.ready_a && ts.ready_b)
        {
            /* ---- both confirmed: execute the exchange ---- */
            auto has_enough = [](::peer *p, const std::unordered_map<short, short> &offer)
            {
                for (const auto &[id, cnt] : offer)
                {
                    bool ok = false;
                    for (const ::slot &s : p->slots)
                        if (s.id == id && s.count >= cnt) { ok = true; break; }
                    if (!ok) return false;
                }
                return true;
            };

            if (!has_enough(pPeer, my_offer) || !has_enough(partner, other_offer))
            {
                on::ConsoleMessage(event.peer, "`4Trade failed — not enough items (check stacks).``");
                on::ConsoleMessage(partner_peer, "`4Trade failed — not enough items.``");
                close_trade(pPeer->user_id, "");
                return;
            }

            /* take from A, give to B */
            for (const auto &[id, cnt] : ts.offer_a)
            {
                { ENetEvent e{}; e.peer = event.peer; modify_item_inventory(e, ::slot(id, -cnt)); }
                { ENetEvent e{}; e.peer = partner_peer; modify_item_inventory(e, ::slot(id, cnt)); }
            }
            /* take from B, give to A */
            for (const auto &[id, cnt] : ts.offer_b)
            {
                { ENetEvent e{}; e.peer = partner_peer; modify_item_inventory(e, ::slot(id, -cnt)); }
                { ENetEvent e{}; e.peer = event.peer; modify_item_inventory(e, ::slot(id, cnt)); }
            }

            send_inventory_state(event);
            { ENetEvent e{}; e.peer = partner_peer; send_inventory_state(e); }
            on::ConsoleMessage(event.peer, std::format("`2Trade with `w{}`` complete!``", partner->growid));
            on::ConsoleMessage(partner_peer, std::format("`2Trade with `w{}`` complete!``", pPeer->growid));

            trade_sessions.erase(key);
            trade_partner.erase(pPeer->user_id);
            trade_partner.erase(partner->user_id);
            return;
        }

        on::ConsoleMessage(event.peer, ts.ready_a && ts.ready_b ? "" : 
            std::format("{}`1You are ready.`` Waiting for the other player... {}", "\n", ""));
        send_trade_ui(*event.peer, ts, is_a);
        if (partner_peer) send_trade_ui(*partner_peer, ts, !is_a);
        return;
    }

    if (clicked == "cancel_trade" || clicked == "Cancel Trade")
    {
        close_trade(pPeer->user_id, "`4Trade cancelled.``");
        return;
    }

    /* default: re-open UI (e.g. stray clicks) */
    send_trade_ui(*event.peer, ts, is_a);
}
