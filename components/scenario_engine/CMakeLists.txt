# ESP-IDF component; host builds consume the same source files.
idf_component_register(SRCS "device_model.cpp" INCLUDE_DIRS "include" REQUIRES core)
