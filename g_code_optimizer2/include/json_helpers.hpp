#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

template <typename T>
T getJsonConfig(const std::filesystem::path& path)
{
  std::ifstream f(path);
  if(!f.is_open())
  {
    std::string err = "Failed to read " + path.string();
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
    std::string err = "Failed to parse " + path.string();
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

  void setBasePath(const std::filesystem::path& path)
  {
    m_initialized  = true;
    configBasePath = path;
  }

  const std::filesystem::path& getBasePath() const { return configBasePath; }

  const std::filesystem::path getAlgorithmsPath() const { return getBasePath() / "algorithms"; }

private:
  std::filesystem::path configBasePath = "";
};
