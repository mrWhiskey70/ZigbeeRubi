#include "simulator_runtime.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
int main(int argc, char **argv) {
  if (argc < 2)
    return 2;
  scenario::SimulatorRuntime runtime(argv[1]);
  const bool real = argc > 2 && std::string(argv[2]) == "--real-time";
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
}
