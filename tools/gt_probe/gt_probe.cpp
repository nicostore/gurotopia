
// gt_probe.cpp — ENet test client for Gurotopia dev (Hermes VM)
// Simulates the Growtopia client login flow; prints server responses.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

#include "../../include/enet/enet.h"

static std::string base64_encode(const std::string &in)
{
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out; out.reserve(((in.size()+2)/3)*4);
    unsigned i = 0;
    while (i+3 <= in.size()) {
        unsigned v = (unsigned char)in[i]<<16 | (unsigned char)in[i+1]<<8 | (unsigned char)in[i+2];
        out += tbl[(v>>18)&63]; out += tbl[(v>>12)&63]; out += tbl[(v>>6)&63]; out += tbl[v&63]; i+=3;
    }
    size_t rem = in.size()-i;
    if (rem == 1) { unsigned v=(unsigned char)in[i]<<16; out+=tbl[(v>>18)&63]; out+=tbl[(v>>12)&63]; out+="=="; }
    else if (rem == 2){ unsigned v=(unsigned char)in[i]<<16|(unsigned char)in[i+1]<<8; out+=tbl[(v>>18)&63]; out+=tbl[(v>>12)&63]; out+=tbl[(v>>6)&63]; out+="="; }
    return out;
}

static void dump_packet(ENetPacket *p)
{
    printf("  << packet len=%u : ", p->dataLength);
    for (unsigned i = 0; i < p->dataLength && i < 200; ++i) {
        unsigned char c = p->data[i];
        putchar((c>=32 && c<127) ? c : '.');
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    const char *chat_text = (argc>5)? argv[5] : "";
    const char *host = (argc>1)? argv[1] : "127.0.0.1";
    int port = (argc>2)? atoi(argv[2]) : 17091;
    const char *growid = (argc>3)? argv[3] : "probe_user";
    const char *pass = (argc>4)? argv[4] : "password123";

    if (enet_initialize() != 0) { fprintf(stderr, "enet init failed\n"); return 1; }

    ENetHost *client = enet_host_create(ENET_ADDRESS_TYPE_IPV4, nullptr, 1, 2, 0, 0);
    if (!client) { fprintf(stderr, "host create failed\n"); return 1; }
    client->usingNewPacket = 1;                       // @note GT fork: new-format header
    client->checksum = enet_crc32;                    // @note server verifies CRC32 on incoming
    enet_host_compress_with_range_coder(client);      // @note server compresses outgoing w/ range coder
    setbuf(stdout, nullptr);                          // @note unbuffer so timeout-kill doesn't lose logs // @note GT client sends new-format packets
    if (!client) { fprintf(stderr, "host create failed\n"); return 1; }

    ENetAddress addr{};
    addr.type = ENET_ADDRESS_TYPE_IPV4;
    if (enet_address_set_host_ip(&addr, host) != 0) { fprintf(stderr,"bad host\n"); return 1; }
    addr.port = port;

    ENetPeer *peer = enet_host_connect(client, &addr, 2, 0);
    if (!peer) { fprintf(stderr, "connect failed\n"); return 1; }
    printf("[connecting to %s:%d]\n", host, port);

    bool connected = false, first = true, sent_tank = false, sent_enter = false, sent_join = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (std::chrono::steady_clock::now() < deadline)
    {
        ENetEvent event;
        while (enet_host_service(client, &event, 100) > 0)
        {
            switch (event.type)
            {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("[connect ok]\n");
                    connected = true;
                    first = true;
                    {
                        // 1st packet: 1-byte type 5 (connect status)
                        ENetPacket *p = enet_packet_create("\x05", 1, ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(peer, 0, p);
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    printf("<<RECV type=%u len=%lu\n", event.packet->data[0], (unsigned long)event.packet->dataLength);
                    dump_packet(event.packet);
                    std::string body((const char*)event.packet->data, event.packet->dataLength);
                    if (first && event.packet->dataLength >= 5)
                    {
                        // Step 1: protocol|213|ltoken|<base64>
                        first = false;
                        std::string tokenstr = "growId=" + std::string(growid) + "&password=" + std::string(pass);
                        std::string enc = base64_encode(tokenstr);
                        std::string payload = "protocol|213|ltoken|" + enc;
                        ENetPacket *p = enet_packet_create(nullptr, 4 + payload.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                        p->data[0] = 2;
                        std::memset(p->data + 1, 0, 3);
                        std::memcpy(p->data + 4, payload.data(), payload.size());
                        p->data[4 + payload.size()] = 0;
                        printf("[1/3 protocol|ltoken for %s]\n", growid);
                        enet_peer_send(peer, 0, p);
                    }

                    // state machine on server varlists
                    if (body.find("OnSendToServer") != std::string::npos && !sent_tank)
                    {
                        // Step 2: tankIDName with >=41 pipe fields
                        sent_tank = true;
                        std::string payload = "tankIDName|" + std::string(growid) +
                            "|tankIDPass|" + std::string(pass) +
                            "|requestedName|" + std::string(growid) +
                            "|protocol|213|country|id|hash|abc123|mac|00:11:22:33:44:55|platformID|0|user|1" +
                            "|f1|v1|f2|v2|f3|v3|f4|v4|f5|v5|f6|v6|f7|v7|f8|v8|f9|v9|f10|v10|f11|v11|f12|v12|f13|v13|f14|v14|f15|v15|f16|v16";
                        ENetPacket *p = enet_packet_create(nullptr, 4 + payload.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                        p->data[0] = 2;
                        std::memset(p->data + 1, 0, 3);
                        std::memcpy(p->data + 4, payload.data(), payload.size());
                        p->data[4 + payload.size()] = 0;
                        printf("[2/3 tankIDName sent]\n");
                        enet_peer_send(peer, 0, p);
                    }
                    else if (body.find("OnSuperMainStart") != std::string::npos && !sent_enter)
                    {
                        // Step 3: action|enter_game
                        sent_enter = true;
                        std::string payload = "action|enter_game";
                        ENetPacket *p = enet_packet_create(nullptr, 4 + payload.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                        p->data[0] = 2;
                        std::memset(p->data + 1, 0, 3);
                        std::memcpy(p->data + 4, payload.data(), payload.size());
                        p->data[4 + payload.size()] = 0;
                        printf("[3/3 enter_game sent]\n");
                        enet_peer_send(peer, 0, p);
                    }
                    if (body.find("OnRequestWorldSelectMenu") != std::string::npos && !sent_join && sent_enter)
                    {
                        sent_join = true;
                        std::string payload = "action|join_request|name|START";
                        ENetPacket *p = enet_packet_create(nullptr, 4 + payload.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                        p->data[0] = 2;
                        std::memset(p->data + 1, 0, 3);
                        std::memcpy(p->data + 4, payload.data(), payload.size());
                        p->data[4 + payload.size()] = 0;
                        printf("[4/4 join_request START sent]\n");
                        enet_peer_send(peer, 0, p);
                        if (chat_text[0]) {
                            std::string cpayload = "action|input|text|" + std::string(chat_text);
                            ENetPacket *p2 = enet_packet_create(nullptr, 4 + cpayload.size() + 1, ENET_PACKET_FLAG_RELIABLE);
                            p2->data[0] = 2;
                            std::memset(p2->data + 1, 0, 3);
                            std::memcpy(p2->data + 4, cpayload.data(), cpayload.size());
                            p2->data[4 + cpayload.size()] = 0;
                            printf("[chat sent: %s]\n", chat_text);
                            enet_peer_send(peer, 0, p2);
                        }
                    }
                    if (body.find("OnSetCurrentWorld") != std::string::npos || body.find("OnSpawn") != std::string::npos)
                        printf(">>> IN-WORLD CONFIRMED <<<\n");
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("[disconnect]\n");
                    connected = false;
                    break;
                default: break;
            }
        }
        if (connected && first == false && std::chrono::steady_clock::now() > deadline - std::chrono::seconds(5))
            break; // let it run, then time out
    }

    enet_peer_disconnect(peer, 0);
    enet_host_destroy(client);
    enet_deinitialize();
    printf("[done]\n");
    return 0;
}
