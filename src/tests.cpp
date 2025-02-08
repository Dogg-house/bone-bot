#include "teams.h"
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <sstream>
#include <string>
#include <iostream>

std::vector<dpp::guild_member> fake_members(int count) {
  std::vector<dpp::guild_member> result;
  auto user_cache = dpp::get_user_cache();

  for (auto i = 0; i < count; i++) {
    // DPP manages these
    auto u = new dpp::user;
    u->username = "user: " + std::to_string(i);
    u->global_name = "global_name: " + std::to_string(i);
    u->id = i;

    user_cache->store(u);

    dpp::guild_member gm{};
    gm.user_id = i;
    gm.set_nickname(fmt::format("nickname: {}", i));
    result.emplace_back(gm);
  }
  return result;
};

[[maybe_unused]]
std::string test_team_format(const std::vector<bone_team> &teams) {
  std::stringstream ss;

  for (auto index = 0; const auto &team : teams) {
    switch (team.type) {
    default:
      [[fallthrough]];
    case team_type::normal:
      ss << "Team " << index << ":\n";
      break;
    case team_type::extra:
      ss << "Extras \n";
      break;
    }

    for (const auto &team_member : team.members)
      ss << team_member.get_mention() << '\n';

    index++;
  }

  return ss.str();
}

TEST_CASE("All members are sorted onto a team", "[teams]") {
  const auto members = fake_members(8);
  auto teams = make_teams(members, 2);

  // should make 2 teams
  REQUIRE(teams.size() == 2ul);
  // Each team should have 4 members
  REQUIRE(teams[0].members.size() == 4ul);
  REQUIRE(teams[1].members.size() == 4ul);
}

TEST_CASE("Extra team members", "[teams]") {
  const auto members = fake_members(9);
  const auto team_count{2};
  const auto team_size{4};
  auto teams = make_teams(members, team_count, team_size);

  // should make 2 teams with an extra team
  REQUIRE(teams.size() == team_count + 1);
  // Normal teams should have `team_size` members
  // Extras should have 1
  for (const auto &team: teams) {
    if (team.type == team_type::normal)
      REQUIRE(team.members.size() == team_size);
    else
      REQUIRE(team.members.size() == 1);
  }
}

TEST_CASE("Team captains are respected", "[teams]") {
  const auto members = fake_members(12);

  // make the first two members captains
  const auto &captain_1 = members[0];
  const auto &captain_2 = members[1];
  std::vector<dpp::guild_member> captains{captain_1, captain_2};

  const auto teams = make_teams(members, 2, {}, captains);

  REQUIRE(teams.size() == 2ul);
  const auto &team_1 = teams[0].members;
  const auto &team_2 = teams[1].members;

  auto find_captain_1 = [&captain_1] (const dpp::guild_member &element) {
    return captain_1.user_id == element.user_id;
  };

  auto find_captain_2 = [&captain_2] (const dpp::guild_member &element) {
    return captain_2.user_id == element.user_id;
  };

  REQUIRE(std::ranges::find_if(team_1, find_captain_1) != team_1.end());
  REQUIRE(std::ranges::find_if(team_1, find_captain_2) == team_1.end());

  REQUIRE(std::ranges::find_if(team_2, find_captain_2) != team_2.end());
  REQUIRE(std::ranges::find_if(team_2, find_captain_1) == team_2.end());
};
