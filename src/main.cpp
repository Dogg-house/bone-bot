#include "insults.h"
#include "teams.h"
#include "project.h"
#include <algorithm>
#include <cstdlib>
#include <dpp/dpp.h>
#include <filesystem>
#include <fmt/format.h>
#include <iomanip>
#include <optional>
#include <random>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <toml++/toml.h>
#include <variant>
#include <vector>
#include "users.h"

const std::string logfile{"bone-bot.log"};

int main() {
  // ----- Setup spdlog -----
  spdlog::init_thread_pool(8192, 2);
  std::vector<spdlog::sink_ptr> sinks;
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logfile, 1024 * 1024 * 5, 10);
  sinks.push_back(stdout_sink);
  sinks.push_back(rotating);
  const auto log = std::make_shared<spdlog::async_logger>(
      "logs", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
  spdlog::register_logger(log);
  log->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
  log->set_level(spdlog::level::level_enum::debug);

  // ----- Config -----
  spdlog::info("Reading config");
  std::filesystem::path base_config_path;

  if (const auto xdg_env{std::getenv("XDG_CONFIG_HOME")}; xdg_env != nullptr)
    base_config_path = xdg_env;
  else if (const auto home_env{std::getenv("HOME")}; home_env != nullptr) {
    base_config_path = std::filesystem::path{home_env} / ".config/";
    spdlog::info("$XDG_CONFIG_HOME not set, using '{}'", base_config_path.string());
  } else {
    spdlog::error("$XDG_CONFIG_HOME and $HOME are both unset, at least one must be specified");
    std::exit(1);
  }
  base_config_path += "bone-bot/";

  if (!std::filesystem::exists(base_config_path)) {
    spdlog::info("Config directory '{}' does not exist, creating", base_config_path.string());
    std::filesystem::create_directories(base_config_path);
  }

  const std::filesystem::path full_config_path{base_config_path / "bone-bot.toml"};
  if (!std::filesystem::exists(full_config_path)) {
    spdlog::info("Configuration file '{}' does not exist, creating", full_config_path.string());
    std::filesystem::copy_file("base-config.toml", full_config_path);
  }

  spdlog::info("Parsing config file at: '{}'", full_config_path.string());

  toml::table config;
  try {
    config = toml::parse_file(full_config_path.string());
  } catch (const toml::parse_error &err) {
    spdlog::error("Failed to parse config file '', reason: ''", full_config_path.string(), err.description());
    std::exit(1);
  }

  if (!config["resources"]["resource-path"]) {
    spdlog::error("`[resources] resource-path` must be specified in config file");
    std::exit(1);
  }
  const std::filesystem::path resource_directory{config["resources"]["resource-path"].as_string()->get()};

  if (!std::filesystem::exists(resource_directory)) {
    spdlog::info("Resource directory '{}' does not exist, creating", resource_directory.string());
    std::filesystem::create_directories(resource_directory);
  }

  const std::filesystem::path sus_input_images_path{resource_directory / "sus_input_images"};
  if (!std::filesystem::exists(sus_input_images_path)) {
    spdlog::info("Sus input directory '{}' does not exist, creating", sus_input_images_path.string());
    std::filesystem::create_directories(sus_input_images_path);
  }

  const std::filesystem::path sus_output_images_path{resource_directory / "sus_output"};
  if (!std::filesystem::exists(sus_output_images_path)) {
    spdlog::info("Sus output directory '{}' does not exist, creating", sus_output_images_path.string());
    std::filesystem::create_directories(sus_output_images_path);
  }

  if (!config["auth"]["token"]) {
    spdlog::error("`[auth] token` must be specified in config file");
    std::exit(1);
  }
  const auto token = config["auth"]["token"].value<std::string>().value();

  if (token.size() > 255uL && token.find("VmFwb3Jlb24") != std::string::npos) {
    spdlog::error("Use the real token you baka! It goes in '{}'", full_config_path.string());
    std::exit(2);
  }

  // ----- Read in words -----
  word_collection words;

  spdlog::info("Reading adjectives");
  read_in_words(resource_directory / "pirate-adjectives.txt", words.adjectives);

  spdlog::info("Reading nouns");
  read_in_words(resource_directory / "pirate-nouns.txt", words.nouns);

  spdlog::info("Reading nouns - plural");
  read_in_words(resource_directory / "pirate-nouns-plural.txt", words.nouns_plural);

  spdlog::info("Reading verbs");
  read_in_words(resource_directory / "pirate-verbs.txt", words.verbs);

  // ----- Start Bot -----
  spdlog::info("Starting Bone Bot");
  dpp::cluster bot{token, dpp::i_default_intents | dpp::i_message_content | dpp::i_guild_members};

  // ----- DPP + spdlog config -----
  bot.on_log([&log](const dpp::log_t &event) {
    switch (event.severity) {
    case dpp::ll_trace:
      log->trace("{}", event.message);
      break;
    case dpp::ll_debug:
      log->debug("{}", event.message);
      break;
    case dpp::ll_info:
      log->info("{}", event.message);
      break;
    case dpp::ll_warning:
      log->warn("{}", event.message);
      break;
    case dpp::ll_error:
      log->error("{}", event.message);
      break;
    case dpp::ll_critical:
    default:
      log->critical("{}", event.message);
      break;
    }
  });

  // Searcher to dig through replies to see
  // if the reply chain was started by
  // an insult command
  SailorReplySearcher reply_searcher{bot, words};

  // ----- Slash commands -----
  bot.on_slashcommand([&words, resource_directory, sus_input_images_path, sus_output_images_path](
                          const dpp::slashcommand_t &event) -> dpp::task<void> {
    auto thinking = event.co_thinking();
    spdlog::debug("On slash command");
    auto cluster = event.from->creator;
    const auto &command_name = event.command.get_command_name();

    if (command_name == "bone-about") {
      spdlog::info("Sending `bone-about`");
      event.edit_response(fmt::format(R"(Bone Bot v{}
Code by: <@277914802071011328>
Art by: <@551533880432263201>
Contribute to the problem @ <https://github.com/The-Dogghouse/bone-bot>
Our versioning scheme <https://0ver.org/>)", // Links are in <> to supress the embed
          BONE_BOT_VERSION));
      co_return;
    }

    if (command_name == "bone-sailor") {
      const auto author_mention = event.command.member.get_mention();
      const auto insult = fmt::format("{}. {}.", author_mention, make_insult(words));
      spdlog::info("Sending insult {}", insult);
      co_await thinking;
      event.edit_response(insult);
      co_return;
    }

    if (command_name == "bone-sus") {
      const auto attachment =
          event.command.get_resolved_attachment(std::get<dpp::snowflake>(event.get_parameter("file")));
      if (!attachment.content_type.starts_with("image")) { // Only took 35 years baby!
        event.edit_response("I need an image you sussy baka!");
        co_return;
      }

      // Width is an optional param
      const auto width_param = event.get_parameter("width");
      int64_t width{21};
      if (std::holds_alternative<int64_t>(width_param))
        width = std::get<int64_t>(width_param);

      const auto response = co_await cluster->co_request(attachment.url, dpp::m_get);

      if (response.status != 200) {
        co_await thinking;
        event.edit_response("Error, could not download attachment");
        co_return;
      }

      // Add unix timestamp to the filename, so you can't overwrite something with the same name
      // if you managed to get two things up in the same second with the same name,
      // then let me be the first to welcome you here
      const auto current_unix_timestamp =
          std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
      const auto download_filename = std::to_string(current_unix_timestamp) + "-" + attachment.filename;

      const auto out_path = sus_input_images_path / download_filename;
      std::fstream test_out{out_path, std::ios::out | std::ios::binary};
      test_out.write(response.body.c_str(), response.body.size());
      test_out.close();

      const auto result_path = sus_output_images_path / (std::to_string(current_unix_timestamp) + ".gif");

      const auto sus_command =
          fmt::format("rusty-sussy/target/release/rusty-sussy --input=./{} --output=./{} --width={}", out_path.string(), result_path.string(), width);
      spdlog::info("sus command {}", sus_command);
      std::system(sus_command.c_str());

      dpp::message result{event.command.channel_id, ""};
      result.add_file(std::format("sussified-{}.gif", current_unix_timestamp), dpp::utility::read_file(result_path));

      co_await thinking;
      event.edit_response(result);
    }

    if (command_name == "bone-teams") {
      spdlog::info("Received 'bone-team'");

      const auto &cmd_data = event.command.get_command_interaction();
      const auto &subcommand = cmd_data.options[0];

      std::optional<int> team_count;
      if (const auto count_param = event.get_parameter("team-count"); std::holds_alternative<int64_t>(count_param)) {
        team_count = static_cast<int>(std::get<int64_t>(count_param));
      }

      std::optional<int> team_size;
      if (const auto size_param = event.get_parameter("team-size"); std::holds_alternative<int64_t>(size_param)) {
        team_size = static_cast<int>(std::get<int64_t>(size_param));
      }

      const auto captains = co_await get_captains_for_command(event);

      if (subcommand.name == "channel") {
        const auto channel_id = std::get<dpp::snowflake>(event.get_parameter("channel"));
        auto channel = co_await cluster->co_channel_get(channel_id);

        if (channel.is_error()) {
          co_await thinking;
          event.edit_response("Failed to get channel members");
          co_return;
        }

        const auto members = channel.get<dpp::channel>().get_voice_members();
        if (members.empty()) {
          co_await thinking;
          event.edit_response("Requested channel has no voice members");
          co_return;
        }

        std::vector<dpp::guild_member> result;
        result.reserve(members.size());
        for (const auto &[snowflake, _] : members)
          result.push_back(dpp::find_guild_member(event.command.guild_id, snowflake));

        const auto teams = make_teams(result, team_count, team_size, captains);
        const auto formatted_teams = format_teams(teams, words);

        co_await thinking;
        event.edit_response(formatted_teams);
        co_return;
      }

      if (subcommand.name == "event") {
        spdlog::info("Getting event info for guild: {}", event.command.guild_id.str());

        const auto event_url = std::get<std::string>(event.get_parameter("event-url"));

        unsigned long long parsed_event_id;
        if (event_url.find("discord.gg") != std::string::npos) {
          parsed_event_id = std::stoull(event_url.substr(event_url.find_last_of("event=") + 1));
        } else /* discord.com event link */ {
          parsed_event_id = std::stoull(event_url.substr(event_url.find_last_of('/') + 1));
        }

        spdlog::info("parsed_event_id: {}", parsed_event_id);
        const dpp::snowflake event_snowflake{parsed_event_id};

        auto command_event = co_await cluster->co_guild_event_get(event.command.guild_id, event_snowflake);
        if (command_event.is_error()) {
          co_await thinking;
          event.edit_response("Failed to get event");
          co_return;
        }

        co_await thinking;
        cluster->guild_event_users_get(event.command.guild_id, event_snowflake,
            [event, &words, team_count, team_size](const dpp::confirmation_callback_t &callback) {
              const auto event_member_map = std::get<dpp::event_member_map>(callback.value);

              if (event_member_map.empty()) {
                event.edit_response("Requested event has no 'interested' members");
                return;
              }

              std::vector<dpp::guild_member> members;
              members.reserve(event_member_map.size());
              for (const auto &[_, event_member] : event_member_map) {
                members.emplace_back(event_member.member);
              }

              const auto teams = make_teams(members, team_count, team_size);
              const auto formatted_teams = format_teams(teams, words);
              event.edit_response(formatted_teams);
            });

        co_return;
      }
    }
  });

  bot.on_message_create([&bot, &reply_searcher](const dpp::message_create_t &event) {
    const auto bot_mentioned = std::find_if(event.msg.mentions.begin(), event.msg.mentions.end(),
                                   [&bot](const std::pair<dpp::user, dpp::guild_member> &mention) {
                                     return mention.first == bot.me;
                                   }) != event.msg.mentions.end();

    if (!bot_mentioned && event.msg.type != dpp::message_type::mt_reply)
      return;

    if (bot_mentioned && event.msg.type != dpp::message_type::mt_reply) {
      spdlog::info("User {} used basic @mention", event.msg.author.username);
      event.reply("Woof!");
      return;
    }

    if (event.msg.type == dpp::message_type::mt_reply && event.msg.author != bot.me) {
      reply_searcher.set_begin(event.msg);
      bot.message_get(event.msg.message_reference.message_id, event.msg.message_reference.channel_id,
          std::bind(&SailorReplySearcher::operator(), reply_searcher, std::placeholders::_1)); // NOLINT(*-avoid-bind)
    }
  });

  bot.on_ready([&bot](const dpp::ready_t &event) {
    if (dpp::run_once<struct register_bot_commands>()) {
      dpp::slashcommand bone_sailor_command{"bone-sailor", "Engages in some jolly insult fights", bot.me.id};
      bot.global_command_create(bone_sailor_command);

      // Document required parameters,
      // rather than just having a 'true'
      constexpr auto required_param{true};

      dpp::slashcommand bone_sus_command{"bone-sus", "Bring the crew-mates in on an image", bot.me.id};
      bone_sus_command.add_option(dpp::command_option(dpp::co_attachment, "file", "Select an image", required_param));

      auto width_option = dpp::command_option(dpp::co_integer, "width", "Number of crew-mates per row");
      width_option.set_min_value(1);
      width_option.set_max_value(255);
      bone_sus_command.add_option(width_option);

      bot.global_command_create(bone_sus_command);

      dpp::slashcommand bone_team_command{"bone-teams", "Generate teams from an event/channel/etc.", bot.me.id};

      // clang-format off
      bone_team_command.add_option(
          dpp::command_option{dpp::co_sub_command, "channel", "Generate teams from members of a channel"}
              .add_option({dpp::co_channel, "channel", "Channel with people in it", required_param})
              .add_option({dpp::co_integer, "team-count", "Number of teams to generate, this or `team-size` is required"})
              .add_option({dpp::co_integer, "team-size", "Number members per team, this or `team-count` is required"})
              .add_option({dpp::co_user, "captain-1","Captain of the 1st team"})
              .add_option({dpp::co_user, "captain-2","Captain of the 2nd team"})
              .add_option({dpp::co_user, "captain-3","Captain of the 3rd team"})
              .add_option({dpp::co_user, "captain-4","Captain of the 4th team"})
              );
      bone_team_command.add_option(
          dpp::command_option{dpp::co_sub_command, "event", "Generate teams from people interested in an event"}
              .add_option({dpp::co_string, "event-url", "The URL of the event to pull participants from", required_param})
              .add_option({dpp::co_integer, "team-count", "Number of teams to generate, this or `team-size` is required"})
              .add_option({dpp::co_integer, "team-size", "Number members per team, this or `team-count` is required"})
              .add_option({dpp::co_user, "captain-1","Captain of the 1st team"})
              .add_option({dpp::co_user, "captain-2","Captain of the 2nd team"})
              .add_option({dpp::co_user, "captain-3","Captain of the 3rd team"})
              .add_option({dpp::co_user, "captain-4","Captain of the 4th team"})
              );

      // clang-format on
      bot.global_command_create(bone_team_command);

      dpp::slashcommand bone_about_command{
          "bone-about", "Names and shames the people responsible for this bot", bot.me.id};
      bot.global_command_create(bone_about_command);
    }
  });

  bot.start(dpp::st_wait);
  return 0;
}
