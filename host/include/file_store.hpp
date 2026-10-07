#pragma once
#include "scenario_store.hpp"
#include <filesystem>
namespace scenario {
class FileStore : public StoreBackend {
  std::filesystem::path directory_;
  bool read(const char *, unsigned, std::vector<uint8_t> &) noexcept;
  bool write(const char *, unsigned, std::string_view) noexcept;

public:
  explicit FileStore(const std::filesystem::path &);
  bool read_slot(unsigned, std::vector<uint8_t> &) noexcept override;
  bool write_slot(unsigned, std::string_view) noexcept override;
  bool read_active_generation(unsigned,
                              std::vector<uint8_t> &) noexcept override;
  bool write_active_generation(unsigned, std::string_view) noexcept override;
  bool read_metadata(std::string &) noexcept;
  bool write_metadata(std::string_view) noexcept;
};
} // namespace scenario
