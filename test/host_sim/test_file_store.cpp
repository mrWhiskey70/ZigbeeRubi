#include "file_store.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
int main() {
  const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
  auto dir = std::filesystem::temp_directory_path() /
             ("rubi-store-" + std::to_string(unique)) /
             std::filesystem::u8path(u8"Дмитрий и датчики");
  scenario::FileStore first(dir);
  for (const char *value : {"first", "second", "third"}) {
    if (!first.write_metadata(value)) {
      std::cerr << "Replacing metadata in a Unicode directory failed\n";
      return 1;
    }
  }
  scenario::FileStore reopened(dir);
  std::string loaded;
  if (!reopened.read_metadata(loaded) || loaded != "third" ||
      !std::filesystem::exists(dir / "devices0.bin")) {
    std::cerr << "Reopening the latest metadata failed\n";
    return 1;
  }
  std::filesystem::remove_all(dir.parent_path());
}
