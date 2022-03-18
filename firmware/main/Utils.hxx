#pragma once

#include <algorithm> 
#include <cctype>
#include <esp_log.h>
#include <locale>

/// Helper method to break a string into a pair<string, string> based on a delimeter.
///
/// @param str is the string to break.
/// @param delim is the delimeter to break the string on.
///
/// @return the pair of string objects.
static inline std::pair<std::string, std::string> break_string(std::string &str,
                                                               const std::string& delim)
{
  size_t pos = str.find_first_of(delim);
  if (pos == std::string::npos)
  {
    return make_pair(str, "");
  }
  size_t end_pos = pos + delim.length();
  if (end_pos > str.length())
  {
    end_pos = pos;
  }
  return make_pair(str.substr(0, pos), str.substr(end_pos));
}

/// Helper which will break a string into multiple pieces based on a provided
/// delimeter.
///
/// @param str String to tokenize.
/// @param tokens Container which will receive the tokenized strings.
/// @param delimeter Delimeter to tokenize the string on. Default is space.
/// @param keep_incomplete When true, take the last token of the input string
/// and insert it to the container as the last element. When false, the last
/// token will not be inserted to the container. Default is true.
/// @param discard_empty When true, empty tokens will be discarded. When false
/// they will be inserted.
template <class ContainerT>
std::string::size_type tokenize(const std::string& str, ContainerT& tokens,
                                const std::string& delimeter = " ",
                                bool keep_incomplete = true,
                                bool discard_empty = false)
{
  std::string::size_type pos, lastPos = 0;

  using value_type = typename ContainerT::value_type;
  using size_type  = typename ContainerT::size_type;

  while(lastPos < str.length())
  {
    pos = str.find_first_of(delimeter, lastPos);
    if (pos == std::string::npos)
    {
      if (!keep_incomplete)
      {
        return lastPos;
      }
      pos = str.length();
    }

    if (pos != lastPos || !discard_empty)
    {
      tokens.emplace_back(value_type(str.data() + lastPos,
                                     (size_type)pos - lastPos));
    }
    lastPos = pos + delimeter.length();
  }
  return lastPos;
}

/// Inplace trim of spaces from provided string.
///
/// @param s String to be modified.
static inline void string_trim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
    [](unsigned char ch)
    {
        return !std::isspace(ch);
    }));

    s.erase(std::find_if(s.rbegin(), s.rend(),
    [](unsigned char ch)
    {
        return !std::isspace(ch);
    }).base(), s.end());
}

/// Configures log levels to use for various modules of the project.
static inline void configure_log_levels()
{
  struct log_level_t
  {
    const char *tag;
    esp_log_level_t level;
  } log_levels[] =
  {
      {.tag = "*", .level = ESP_LOG_ERROR},
      {.tag = "main", .level = ESP_LOG_INFO},
      {.tag = "heap_mon", .level = ESP_LOG_INFO},
      {.tag = "gcode_server", .level = ESP_LOG_INFO},
      {.tag = "gcode_client", .level = ESP_LOG_INFO},
      {.tag = "wifi_mgr", .level = ESP_LOG_INFO},
      {.tag = "feeder_mgr", .level = ESP_LOG_INFO},
      {.tag = "soc_info", .level = ESP_LOG_INFO},
  };

  for (log_level_t log : log_levels)
  {
      esp_log_level_set(log.tag, log.level);
  }
}