#include "http_server.h"
#include "../utils/json.h" // https://github.com/nlohmann/json/tree/develop/single_include/nlohmann/json.hpp
#include <fstream>

using json = nlohmann::json;
static const std::string CONFIG_FILEPATH = "../config/conf_http_server.json";

bool HttpServer::SetPropertyFromFile(const std::string& path)
{
    std::ifstream fin(path);
    json j;
    fin >> j;
    fin.close();
}