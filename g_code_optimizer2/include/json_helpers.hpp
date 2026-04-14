#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>

#include <glm/gtx/quaternion.hpp>

inline std::string current_timestamp()
{
  auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  return std::format("{:%Y-%m-%d %H:%M:%S}", now);
}

template <typename T>
T getJsonConfig(const std::filesystem::path& path)
{
  std::ifstream f(path);
  if(!f.is_open())
  {
    std::string err = "Cannot open file for reading: " + path.string();
    throw std::runtime_error(err);
  }

  try
  {
    const auto conf = nlohmann::json::parse(f, nullptr).get<T>();
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

// Appends one record keyed by the current timestamp.
template <typename T>
void append_record(const std::filesystem::path& path, const T& params)
{
  // Load document
  nlohmann::json doc;
  if(std::filesystem::exists(path))
  {
    std::ifstream in(path);
    if(!in.is_open())
      throw std::runtime_error("Cannot open file for reading: " + path.string());

    // Load the json
    in >> doc;
  }

  // Insert entry
  nlohmann::json record = params;
  record["timestamp"]   = current_timestamp();
  doc.push_back(record);

  // Write back
  std::ofstream out(path);
  if(!out.is_open())
    throw std::runtime_error("Cannot open file for writing: " + path.string());
  out << doc.dump(2) << '\n';
  out.close();
}
