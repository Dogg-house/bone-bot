#include "teams.h"
#include "users.h"
#include <cmath>
#include <random>
#include <variant>
#include <ranges>
#include <fmt/core.h>
#include <dpp/unicode_emoji.h>

std::default_random_engine team_random_engine{std::random_device{}()};

std::vector<bone_team> make_teams(const std::vector<dpp::guild_member> &members, std::optional<int> team_count,
    std::optional<int> team_size, std::vector<dpp::guild_member> captains) {

  int generate_teams{2}; // generate 2 teams by default
  if (team_count)
    generate_teams = team_count.value();
  else if (team_size)
    generate_teams = static_cast<int>(members.size()) / team_size.value();

  // If we have more captains than teams, truncate the list
  if (captains.size() > generate_teams)
    captains.resize(captains.size() - (captains.size() - generate_teams));

  std::vector<dpp::guild_member> team_pool;
  team_pool.reserve(members.size() - captains.size());

  for (const auto &member : members) {
    auto is_captain = [&member](const dpp::guild_member &element) {
      return member.user_id == element.user_id;
    };
    if (std::ranges::find_if(captains, is_captain) != captains.end())
      continue;
    team_pool.emplace_back(member);
  }

  if (!team_size)
    team_size = std::ceil(static_cast<double>(team_pool.size() + captains.size()) / generate_teams);

  std::ranges::shuffle(team_pool, team_random_engine);

  std::vector<bone_team> result{bone_team{}}; // Create with the first team already made

  auto team_index = 0;
  // If we have captains, handle the first one by hand
  if (!captains.empty()) {
    result[0].members.emplace_back(captains[0]);
    result[0].captain = captains[0];
  }
  for (const auto &member : team_pool) {
    if (result[team_index].members.size() == team_size.value() && team_index < generate_teams) {
      team_index++;
      auto &new_team = result.emplace_back(); // Default construct another team at the end

      if (captains.size() > team_index) {
        new_team.members.emplace_back(captains[team_index]);
        new_team.captain = captains[team_index];
      }

      // We've generated all the teams,
      // we're on extras now
      if (team_index == generate_teams)
        new_team.type = team_type::extra;
    }

    result[team_index].members.emplace_back(member);
  }

  return result;
}

std::string format_teams(const std::vector<bone_team> &teams, const word_collection &words) {
  std::stringstream ss;

  for (auto index = 0; const auto &team : teams) {
    switch (team.type) {
    default:
      [[fallthrough]];
    case team_type::normal:
      ss << "Team " << index << " '" << team_name(words) << "' :\n";
      break;
    case team_type::extra:
      ss << "Extras '" << team_name(words) << "'\n";
      break;
    }

    for (const auto &team_member : team.members) {
      if (team.captain && team_member.user_id == team.captain->user_id)
        ss << dpp::unicode_emoji::star << ' ';
      ss << team_member.get_mention() << '\n';
    }

    index++;
  }

  return ss.str();
}

dpp::task<std::vector<dpp::guild_member>> get_captains_for_command(const dpp::slashcommand_t &event) {
  std::vector<dpp::guild_member> captains{};
  captains.reserve(4);

  for (auto i = 1; i <= 4; i++) {
    const auto &captain_param = event.get_parameter(fmt::format("captain-{}", i));
    if (!std::holds_alternative<dpp::snowflake>(captain_param))
      continue;

    const auto maybe_captain = co_await get_user(std::get<dpp::snowflake>(captain_param), event);
    if (!maybe_captain)
      continue;

    captains.emplace_back(maybe_captain.value());
  }

  co_return captains;
}
