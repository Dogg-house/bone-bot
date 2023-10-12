#pragma once
#include "insults.h"
#include <dpp/dpp.h>
#include <string>
#include <unordered_map>
#include <vector>

enum class team_type { normal, extra };

struct bone_team {
  std::string name;
  team_type type{team_type::normal};
  std::vector<dpp::guild_member> members;
};

std::vector<bone_team> make_teams(const std::vector<dpp::guild_member> &members, std::optional<int> team_count = {},
    std::optional<int> team_size = {}, std::optional<dpp::snowflake> captain_role = {});

std::string format_teams(const std::vector<bone_team> &teams, const word_collection &words);
