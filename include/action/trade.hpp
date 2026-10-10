#pragma once

/* /trade <name> — request a trade with another player in the same world. */
void trade_command(ENetEvent& event, const std::string_view args);

/* handles the trade request dialog response (accept / decline). */
void trade_dialog_return(ENetEvent& event, const ::hPipe& hPipe);
