#include <dpp/dpp.h>
#include <optional>
#include <spdlog/spdlog.h>
#include "users.h"

std::optional<dpp::guild_member> get_cached_user(dpp::snowflake user_id, const dpp::interaction &command) {
  const auto &command_members = command.resolved.members;

  if (const auto command_member = command_members.find(user_id); command_member != command_members.end()) {
    return  command_member->second;
  }

  if (const auto guild = dpp::find_guild(command.guild_id)) {
    if (const auto &guild_cache_member = guild->members.find(user_id); guild_cache_member != guild->members.end()) {
      return  guild_cache_member->second;
    }
  }

  return {};
}

dpp::task<std::optional<dpp::guild_member>> get_user(const dpp::snowflake user_id, const dpp::slashcommand_t &event) {
  if (const auto cached_user = get_cached_user(user_id, event.command))
    co_return cached_user;

  const auto confirmation = co_await event.from->creator->co_guild_get_member(event.command.guild_id, user_id);
  if (confirmation.is_error()) {
    spdlog::error(confirmation.get_error().human_readable);
    co_return {};
  }

  co_return confirmation.get<dpp::guild_member>();
}
