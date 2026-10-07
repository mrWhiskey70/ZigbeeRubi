#include "simulator_runtime.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
namespace {
int run(const std::filesystem::path &directory, bool real) {
  scenario::SimulatorRuntime runtime(directory);
  auto last = std::chrono::steady_clock::now();
  std::string line;
  std::vector<char> buffer(65536);
  while (std::getline(std::cin, line)) {
    if (real) {
      auto now = std::chrono::steady_clock::now();
      runtime.advance_real(
          std::chrono::duration_cast<std::chrono::milliseconds>(now - last)
              .count());
      last = now;
    }
    size_t n = 0;
    auto result = runtime.handle_request(line, buffer.data(), buffer.size(), n);
    if (result)
      std::cout.write(buffer.data(), n);
    else
      std::cout
          << "{\"status\":503,\"body\":{\"error\":\"response_capacity\"}}";
    std::cout << std::endl;
  }
  return 0;
}
} // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
  if (argc < 2)
    return 2;
  return run(argv[1], argc > 2 && std::wstring(argv[2]) == L"--real-time");
}
#else
int main(int argc, char **argv) {
  if (argc < 2)
    return 2;
  return run(argv[1], argc > 2 && std::string(argv[2]) == "--real-time");
}
#endif
