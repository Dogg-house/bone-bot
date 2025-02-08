#pragma once
#include "insults.h"
#include "users.h"
#include <dpp/dpp.h>
#include <string>
#include <unordered_map>
#include <vector>

enum class team_type { normal, extra };

struct bone_team {
  std::string name;
  team_type type{team_type::normal};
  std::optional<dpp::guild_member> captain;
  std::vector<dpp::guild_member> members;
};

std::vector<bone_team> make_teams(const std::vector<dpp::guild_member> &members, std::optional<int> team_count = {},
    std::optional<int> team_size = {}, std::vector<dpp::guild_member> captains = {});

std::string format_teams(const std::vector<bone_team> &teams, const word_collection &words);

dpp::task<std::vector<dpp::guild_member>> get_captains_for_command(const dpp::slashcommand_t &event);
