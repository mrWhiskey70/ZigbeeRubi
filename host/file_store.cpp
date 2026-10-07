#include "file_store.hpp"
#include <cstdio>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace scenario {
FileStore::FileStore(const std::filesystem::path &p) : directory_(p) {
  std::error_code ec;
  std::filesystem::create_directories(p, ec);
}
bool FileStore::read(const char *prefix, unsigned index,
                     std::vector<uint8_t> &out) noexcept {
  std::ifstream f(directory_ /
                      (std::string(prefix) + std::to_string(index) + ".bin"),
                  std::ios::binary | std::ios::ate);
  if (!f)
    return false;
  auto n = f.tellg();
  if (n < 0 || n > 65560)
    return false;
  out.resize(static_cast<size_t>(n));
  f.seekg(0);
  f.read(reinterpret_cast<char *>(out.data()), out.size());
  return static_cast<bool>(f);
}
bool FileStore::write(const char *prefix, unsigned index,
                      std::string_view data) noexcept {
  auto path =
      directory_ / (std::string(prefix) + std::to_string(index) + ".bin");
  auto temp = path;
  temp += ".tmp";
#ifdef _WIN32
  FILE *f = _wfopen(temp.c_str(), L"wb");
#else
  FILE *f = std::fopen(temp.string().c_str(), "wb");
#endif
  if (!f)
    return false;
  bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size() &&
            std::fflush(f) == 0;
#ifdef _WIN32
  ok = ok && _commit(_fileno(f)) == 0;
#else
  ok = ok && fsync(fileno(f)) == 0;
#endif
  ok = std::fclose(f) == 0 && ok;
  if (!ok)
    return false;
#ifdef _WIN32
  return MoveFileExW(temp.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
  std::error_code ec;
  std::filesystem::rename(temp, path, ec);
  if (ec)
    return false;
  int fd = open(directory_.string().c_str(), O_RDONLY | O_DIRECTORY);
  if (fd < 0)
    return false;
  ok = fsync(fd) == 0;
  close(fd);
  return ok;
#endif
}
bool FileStore::read_slot(unsigned i, std::vector<uint8_t> &v) noexcept {
  return read("slot", i, v);
}
bool FileStore::write_slot(unsigned i, std::string_view v) noexcept {
  return write("slot", i, v);
}
bool FileStore::read_active_generation(unsigned i,
                                       std::vector<uint8_t> &v) noexcept {
  return read("commit", i, v);
}
bool FileStore::write_active_generation(unsigned i,
                                        std::string_view v) noexcept {
  return write("commit", i, v);
}
bool FileStore::read_metadata(std::string &v) noexcept {
  std::vector<uint8_t> b;
  if (!read("devices", 0, b))
    return false;
  v.assign(b.begin(), b.end());
  return true;
}
bool FileStore::write_metadata(std::string_view v) noexcept {
  return write("devices", 0, v);
}
} // namespace scenario
