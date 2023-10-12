#include "insults.h"
#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <random>
#include <spdlog/spdlog.h>
#include <sstream>
#include <string_view>
#include <thread>
#include <variant>

std::default_random_engine random_engine{std::random_device{}()};

[[nodiscard]] std::string_view random_item(const std::vector<std::string> &w) {
  return w[std::uniform_int_distribution<std::size_t>{0, w.size() - 1u}(random_engine)];
}

SailorReplySearcher::SailorReplySearcher(dpp::cluster &bot, word_collection &words) : words(words), bot(bot) {
}

void SailorReplySearcher::set_begin(const dpp::message &root) {
  beginning_message = root;
}

void SailorReplySearcher::send_insult_back(const dpp::message &last_message) {
  dpp::message reply{fmt::format("Oh yeah {}! {}", last_message.author.get_mention(), make_insult(words)),
                     dpp::mt_reply};
  reply.set_reference(last_message.id, last_message.guild_id, last_message.channel_id);

  reply.channel_id = last_message.channel_id;
  reply.guild_id = last_message.guild_id;

  bot.message_create(reply);
}

void SailorReplySearcher::operator()(const dpp::confirmation_callback_t &cb) {
  if (cb.is_error() || !std::holds_alternative<dpp::message>(cb.value)) {
    spdlog::error("`SailorReplySearcher` failed to retrieve reply, exiting search");
    return;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  const auto &msg = std::get<dpp::message>(cb.value);
  if (msg.type == dpp::mt_reply) {
    bot.message_get(msg.message_reference.message_id, msg.message_reference.channel_id,
                    std::bind(&SailorReplySearcher::operator(), this, std::placeholders::_1)); // NOLINT(*-avoid-bind)
    return;
  }

  if (msg.type == dpp::mt_application_command && msg.interaction.name == "bone-sailor") {
    spdlog::info("At end of 'bone-sailor' reply chain, deploying new insult");
    send_insult_back(beginning_message);
  }
}

void read_in_words(const std::filesystem::path &file_path, std::vector<std::string> &word_class) {
  const auto begin_time = std::chrono::steady_clock::now();
  std::ifstream file{file_path};

  std::string line;
  while (std::getline(file, line)) {
    word_class.emplace_back(line);
  }

  const auto end_time = std::chrono::steady_clock::now();
  spdlog::info("Read file {} in {}us", file_path.string(),
               std::chrono::duration_cast<std::chrono::microseconds>(end_time - begin_time).count());
}

std::string make_insult(const word_collection &w) {
  switch (std::uniform_int_distribution<>{0, 8}(random_engine)) {
  case 0:
    return fmt::format("I'll eat yer {} and drink your {} ye {}, {}, {} {}!", random_item(w.nouns),
                       random_item(w.nouns), random_item(w.adjectives), random_item(w.adjectives),
                       random_item(w.adjectives), random_item(w.nouns));
  case 1:
    return fmt::format("My {} has a smaller nose than ye, you {}, {}, {} {}!", random_item(w.nouns),
                       random_item(w.adjectives), random_item(w.adjectives), random_item(w.adjectives),
                       random_item(w.nouns));
  case 2:
    return fmt::format("Ya fight like a {}, you {}, {}, {} {}!", random_item(w.nouns), random_item(w.adjectives),
                       random_item(w.adjectives), random_item(w.adjectives), random_item(w.nouns));
  case 3:
    return fmt::format("Yer as [smart, dumb] as a {}, you {}, {}, {} {}!", random_item({"smart", "dumb"}),
                       random_item(w.nouns), random_item(w.adjectives), random_item(w.adjectives),
                       random_item(w.adjectives), random_item(w.nouns));
  case 4:
    return fmt::format("Yer breath could kill a {}, ya {}, {}, {} {}!", random_item(w.nouns), random_item(w.adjectives),
                       random_item(w.adjectives), random_item(w.adjectives), random_item(w.nouns));
  case 5: {
    std::vector<std::string> base_insult{"was a", "smelt of", "licked a", "kissed a", "tastes of"};
    return fmt::format("Yer mother {} {} and your father {} {}", random_item(base_insult), random_item(w.nouns),
                       random_item(base_insult), random_item(w.nouns));
  }
  case 6:
    return fmt::format("You be a {}, {}, {} {}, who's only good for {} {}", random_item(w.adjectives),
                       random_item(w.adjectives), random_item(w.adjectives), random_item(w.nouns), random_item(w.verbs),
                       random_item(w.nouns_plural));
  case 7:
    return fmt::format("You don't need a {}, yer face be deadlier, you {}, {}, {} {}!", random_item(w.nouns),
                       random_item(w.adjectives), random_item(w.adjectives), random_item(w.adjectives),
                       random_item(w.nouns));
  case 8:
    [[fallthrough]];
  default:
    return fmt::format("I'll {} yer {} and feed it to the {}, ya {}, {}, {} {}!",
                       random_item({"cut out", "pull out", "yank out", "cleave off"}), random_item(w.nouns),
                       random_item(w.nouns_plural), random_item(w.adjectives), random_item(w.adjectives),
                       random_item(w.adjectives), random_item(w.nouns));
  }
}
std::string team_name(const word_collection &w) {
  return fmt::format("{} {}", random_item(w.adjectives), random_item(w.nouns_plural));
}
