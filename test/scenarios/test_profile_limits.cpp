#include "test_support.hpp"
#include "core_state.hpp"
#include "service_public_types.hpp"
int main(){CHECK(core::kMaxDevices==16);CHECK(service::kServiceMaxDevices==16);}
