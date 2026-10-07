# SPDX-License-Identifier: AGPL-3.0-only
# Copyright (C) 2026 Alex.K.

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ProjectCompileOptions.cmake")

idf_component_register(
    SRCS
        "web_server.cpp"
        "web_routes.cpp"
        "web_handlers_v1.cpp"
        "web_handlers_static.cpp"
        "web_handlers_device.cpp"
        "web_handlers_network.cpp"
        "web_handlers_config.cpp"
        "web_handlers_ota.cpp"
        "web_handlers_rcp.cpp"
        "web_handler_common.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core service common esp_http_server esp_timer json
)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(WEB_UI_ASSET_GZ_DIR "${CMAKE_CURRENT_BINARY_DIR}/assets_gz")
    set(WEB_UI_GZIP_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/../../scripts/gzip_asset.py")

    set(WEB_UI_INDEX_HTML_GZ "${WEB_UI_ASSET_GZ_DIR}/index.html.gz")
    set(WEB_UI_STYLE_CSS_GZ "${WEB_UI_ASSET_GZ_DIR}/style.css.gz")
    set(WEB_UI_APP_JS_GZ "${WEB_UI_ASSET_GZ_DIR}/app.js.gz")

    add_custom_command(
        OUTPUT
            "${WEB_UI_INDEX_HTML_GZ}"
            "${WEB_UI_STYLE_CSS_GZ}"
            "${WEB_UI_APP_JS_GZ}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${WEB_UI_ASSET_GZ_DIR}"
        COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/index.html" "${WEB_UI_INDEX_HTML_GZ}"
        COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/style.css" "${WEB_UI_STYLE_CSS_GZ}"
        COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/app.js" "${WEB_UI_APP_JS_GZ}"
        DEPENDS
            "${WEB_UI_GZIP_SCRIPT}"
            "${CMAKE_CURRENT_LIST_DIR}/assets/index.html"
            "${CMAKE_CURRENT_LIST_DIR}/assets/style.css"
            "${CMAKE_CURRENT_LIST_DIR}/assets/app.js"
        VERBATIM
    )

    add_custom_target(web_ui_assets_gz
        DEPENDS
            "${WEB_UI_INDEX_HTML_GZ}"
            "${WEB_UI_STYLE_CSS_GZ}"
            "${WEB_UI_APP_JS_GZ}"
    )

    add_dependencies(${COMPONENT_LIB} web_ui_assets_gz)
    target_add_binary_data(${COMPONENT_LIB} "${WEB_UI_INDEX_HTML_GZ}" BINARY)
    target_add_binary_data(${COMPONENT_LIB} "${WEB_UI_STYLE_CSS_GZ}" BINARY)
    target_add_binary_data(${COMPONENT_LIB} "${WEB_UI_APP_JS_GZ}" BINARY)
    set(MODULE_api "${WEB_UI_ASSET_GZ_DIR}/api.js.gz")
    add_custom_command(OUTPUT "${MODULE_api}" COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/api.js" "${MODULE_api}" DEPENDS "${CMAKE_CURRENT_LIST_DIR}/assets/api.js" "${WEB_UI_GZIP_SCRIPT}" VERBATIM)
    add_custom_target(web_ui_api_gz DEPENDS "${MODULE_api}")
    add_dependencies(${COMPONENT_LIB} web_ui_api_gz)
    target_add_binary_data(${COMPONENT_LIB} "${MODULE_api}" BINARY)
    set(MODULE_scenario_editor "${WEB_UI_ASSET_GZ_DIR}/scenario_editor.js.gz")
    add_custom_command(OUTPUT "${MODULE_scenario_editor}" COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/scenario_editor.js" "${MODULE_scenario_editor}" DEPENDS "${CMAKE_CURRENT_LIST_DIR}/assets/scenario_editor.js" "${WEB_UI_GZIP_SCRIPT}" VERBATIM)
    add_custom_target(web_ui_scenario_editor_gz DEPENDS "${MODULE_scenario_editor}")
    add_dependencies(${COMPONENT_LIB} web_ui_scenario_editor_gz)
    target_add_binary_data(${COMPONENT_LIB} "${MODULE_scenario_editor}" BINARY)
    set(MODULE_devices_view "${WEB_UI_ASSET_GZ_DIR}/devices_view.js.gz")
    add_custom_command(OUTPUT "${MODULE_devices_view}" COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/devices_view.js" "${MODULE_devices_view}" DEPENDS "${CMAKE_CURRENT_LIST_DIR}/assets/devices_view.js" "${WEB_UI_GZIP_SCRIPT}" VERBATIM)
    add_custom_target(web_ui_devices_view_gz DEPENDS "${MODULE_devices_view}")
    add_dependencies(${COMPONENT_LIB} web_ui_devices_view_gz)
    target_add_binary_data(${COMPONENT_LIB} "${MODULE_devices_view}" BINARY)
    set(MODULE_journal_view "${WEB_UI_ASSET_GZ_DIR}/journal_view.js.gz")
    add_custom_command(OUTPUT "${MODULE_journal_view}" COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/journal_view.js" "${MODULE_journal_view}" DEPENDS "${CMAKE_CURRENT_LIST_DIR}/assets/journal_view.js" "${WEB_UI_GZIP_SCRIPT}" VERBATIM)
    add_custom_target(web_ui_journal_view_gz DEPENDS "${MODULE_journal_view}")
    add_dependencies(${COMPONENT_LIB} web_ui_journal_view_gz)
    target_add_binary_data(${COMPONENT_LIB} "${MODULE_journal_view}" BINARY)
    set(MODULE_simulator_panel "${WEB_UI_ASSET_GZ_DIR}/simulator_panel.js.gz")
    add_custom_command(OUTPUT "${MODULE_simulator_panel}" COMMAND "${Python3_EXECUTABLE}" "${WEB_UI_GZIP_SCRIPT}" "${CMAKE_CURRENT_LIST_DIR}/assets/simulator_panel.js" "${MODULE_simulator_panel}" DEPENDS "${CMAKE_CURRENT_LIST_DIR}/assets/simulator_panel.js" "${WEB_UI_GZIP_SCRIPT}" VERBATIM)
    add_custom_target(web_ui_simulator_panel_gz DEPENDS "${MODULE_simulator_panel}")
    add_dependencies(${COMPONENT_LIB} web_ui_simulator_panel_gz)
    target_add_binary_data(${COMPONENT_LIB} "${MODULE_simulator_panel}" BINARY)

endif()

zg_apply_component_compile_policy(${COMPONENT_LIB})
