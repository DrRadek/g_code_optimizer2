#pragma once
#include <filesystem>

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
  const std::filesystem::path getInputParamsConfigPath() const { return getBasePath() / "inputs_params.json"; }

private:
  std::filesystem::path configBasePath = "";
};
