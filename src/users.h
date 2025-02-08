#pragma once
#include <dpp/dpp.h>
#include <optional>

std::optional<dpp::guild_member> get_cached_user(dpp::snowflake user_id, const dpp::interaction &command);

dpp::task<std::optional<dpp::guild_member>> get_user(dpp::snowflake user_id, const dpp::slashcommand_t &event);
