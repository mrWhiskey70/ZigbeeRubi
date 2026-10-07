#pragma once
#include "file_store.hpp"
#include "hub_runtime.hpp"
namespace scenario {
class SimulatorRuntime {
 FileStore backend_;
 HubRuntime runtime_;
public:
 explicit SimulatorRuntime(const std::filesystem::path &p) : backend_(p),runtime_(backend_) {
   if(!runtime_.ready()) throw std::runtime_error("configuration corrupt; kept for recovery");
 }
 ValidationResult handle_request(std::string_view s,char *out,size_t cap,size_t &n) noexcept {return runtime_.handle_request(s,out,cap,n);}
 void advance_real(uint64_t delta) noexcept {runtime_.advance_real(delta);}
};
}
