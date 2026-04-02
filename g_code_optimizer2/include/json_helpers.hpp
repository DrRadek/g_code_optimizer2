#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

template <typename T>
const T& getJsonConfig(const std::string& path)
{
  std::ifstream f(path);
  if(!f.is_open())
  {
    std::string err = "Failed to read " + path;
    std::cerr << err << "\n";
    throw std::runtime_error(err);
  }

  try
  {
    const auto conf = nlohmann::json::parse(f, nullptr, true, true).get<T>();
    f.close();
    return conf;
  }
  catch(...)
  {
    std::string err = "Failed to parse " + path + "\n";
    std::cerr << err << "\n";
    throw std::runtime_error(err);
  }
};


class AppConfig
{
  bool m_initialized = false;

public:
  static AppConfig& instance()
  {
    static AppConfig s;
    return s;
  }

  void setBasePath(const std::string& path)
  {
    m_initialized  = true;
    configBasePath = path;
  }

  const std::string& getBasePath() const { return configBasePath; }

  const std::string getAlgorithmsPath() const { return getBasePath() + "\\algorithms"; }

private:
  std::string configBasePath = "./config";
};
