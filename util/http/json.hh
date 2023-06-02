#pragma once

extern "C" {
#include "http.h"
}

#include <stdio.h>
#include <nlohmann/json.hpp>

inline nlohmann::json http_parse_json_body(http_worker_t *worker, FILE *stream, unsigned max_body_size)
{
  std::string text;

  size_t i = 0;
  size_t n = (size_t)worker->content_length;
  if (n < 0 || n > max_body_size)
    n = max_body_size;

  text.resize(n);

  while (i < n && !feof(stream)) {
    i += fread(&text[i], 1, n-i, stream);
  }
  text.resize(i);

  return nlohmann::json::parse(text);
}
