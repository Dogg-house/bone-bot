#pragma once
#include <dpp/dpp.h>
#include <random>
#include <string>
#include <vector>

struct word_collection {
  std::vector<std::string> nouns;
  std::vector<std::string> nouns_plural;
  std::vector<std::string> adjectives;
  std::vector<std::string> verbs;
};

class SailorReplySearcher {
  word_collection &words;
  dpp::cluster &bot;
  dpp::message beginning_message;

public:
  explicit SailorReplySearcher(dpp::cluster &bot, word_collection &words);

  void set_begin(const dpp::message &root);

  void send_insult_back(const dpp::message &last_message);

  void operator()(const dpp::confirmation_callback_t &cb);
};

void read_in_words(const std::filesystem::path &file_path, std::vector<std::string> &word_class);

std::string make_insult(const word_collection &w);

std::string team_name(const word_collection &w);
