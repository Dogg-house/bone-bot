#include "teams.h"
#include <cmath>
#include <random>

std::default_random_engine team_random_engine{std::random_device{}()};

std::vector<bone_team> make_teams(const std::vector<dpp::guild_member> &members, std::optional<int> team_count,
    std::optional<int> team_size, std::optional<dpp::snowflake> captain_role) {
  std::vector<dpp::guild_member> captains;
  if (captain_role) {
    for (const auto &possible_captain : members) {
      const auto &possible_captain_roles = possible_captain.get_roles();
      if (std::find(possible_captain_roles.begin(), possible_captain_roles.end(), captain_role.value()) !=
         possible_captain_roles.end())
        captains.emplace_back(possible_captain);
    }
  }

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
    if (std::find(captains.begin(), captains.end(), member) != captains.end())
      continue;
    team_pool.emplace_back(member);
  }

  std::shuffle(team_pool.begin(), team_pool.end(), team_random_engine);

  if (!team_size)
    team_size = std::ceil(static_cast<double>(team_pool.size()) / generate_teams);

  std::vector<bone_team> result{bone_team{}}; // Create with the first team already made
  auto member_number = 0;
  auto team_index = 0;
  for (const auto &member : team_pool) {
    if (member_number == team_size.value() && team_index < generate_teams) {
      team_index++;
      auto &new_team = result.emplace_back(); // Default construct another team at the end

      if (captains.size() > team_index)
        new_team.members.emplace_back(captains[team_index]);

      // We've generated all the teams,
      // we're on extras now
      if (team_index == generate_teams)
        new_team.type = team_type::extra;

      member_number = 0;
    }

    result[team_index].members.emplace_back(member);
    member_number++;
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
      ss << team_member.get_mention() << '\n';
    }

    index++;
  }

  return ss.str();
}
