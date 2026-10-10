# Gurotopia — Custom GTPS

Gurotopia is an open-source Growtopia private server written in modern C/C++.
This fork adds features verified through an ENet test-client (`tools/gt_probe`) —
no real client needed to develop.

## Build (Linux/Debian)

```bash
sudo apt-get install build-essential libssl-dev libmariadb-dev mariadb-server
# GCC 13+ required (uses <format>): add ubuntu-toolchain-r/test PPA, then:
sudo apt-get install g++-13
git clone https://github.com/NousResearch-CN/gurotopia.git   # (repo kontribusi kamu)
cd Gurotopia
make -j$(nproc)          # atau: PATH=<dir-gcc13>:$PATH make -j$(nproc)
sudo ./main.out          # port 443 (sudo) + 17091 UDP
```

Butuh `items.dat` (dari client) di folder repo — versi format 0x1a+ didukung.

## Features (beyond stock gurotopia)

- **Trade system penuh** — `/trade <name>` → request dialog → live 2-side UI
  → item picker (ownership-checked) → ready both sides → real item exchange
- **Autosave crash-safety** — world + inventory persisted every 30s; survives kill -9
- **Password verification** — wrong password → `logon_fail` (account-hijack fix)
- **Duplicate-login guard** — relogin kicks the old session
- **Progress persistence** — level / XP / gems / home_world survive reconnect
- **Admin/moderator** — `admins.txt` (one growid per line):
  - `/kick <name>`, `/ban <name>`, `/unban <name>`, `/give <name> <item> <count>`
- **Social** — `/msg <name> <text>` (PM), `/online`, `/home` (warp), `/sethome`
  (in owned world), `/trade`
- **Banned accounts** — `banned.txt`, enforced at both login phases

## Dev harness

`tools/gt_probe/gt_probe.cpp` — ENet client simulating the GT login flow
(connect → protocol/ltoken → tankIDName → enter_game → join_request) plus
scriptable chat/dialog actions. Build it against the ZTzTopia/enet fork
bundle to talk to the server's new-packet wire format.

## Serialization notes

- Game packets: type byte 2 = text (4-byte header + payload + NUL),
  type 4 = variant list (compressed); tank packets per GT protocol.
