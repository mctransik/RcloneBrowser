set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(SOURCES
  main.cpp
  main_window.cpp main_window_shell.cpp main_window.h main_window.ui
  remote_widget.cpp remote_widget.h remote_widget.ui
  dual_pane_widget.cpp dual_pane_widget.h
  local_browser_widget.cpp local_browser_widget.h
  file_manager_column_delegate.h
  transfer_dialog.cpp transfer_dialog.h transfer_dialog.ui
  export_dialog.cpp export_dialog.h export_dialog.ui
  progress_dialog.cpp progress_dialog.h progress_dialog.ui
  job_widget.cpp job_widget.h job_widget.ui
  proxy_credentials.cpp proxy_credentials.h
  proxy_process_environment.h
  credential_safety.h
  log_safety.cpp log_safety.h
  mount_widget.cpp mount_widget.h mount_widget.ui
  stream_widget.cpp stream_widget.h stream_widget.ui
  preferences_dialog.cpp preferences_dialog.h preferences_dialog.ui
  icon_cache.cpp icon_cache.h
  item_model.cpp item_model.h
  rclone_output.cpp rclone_output.h
  rclone_updater.cpp rclone_updater.h
  theme.cpp theme.h
  utils.cpp utils.h
  job_options.cpp job_options_data.cpp job_options.h
  task_job_codec.cpp task_job_codec.h
  list_of_job_options.cpp list_of_job_options.h
  task_storage.cpp task_storage.h
  task_list_transaction.h
  task_secret_risk.cpp task_secret_risk.h
  task_protection.cpp task_protection.h
  pch.h
  resources.qrc
)

if(WIN32)
  set(TARGET_NAME RcloneBrowser)
  list(APPEND SOURCES resources.rc)
else()
  set(TARGET_NAME rclone-browser)
endif()

add_executable(${TARGET_NAME} WIN32 ${SOURCES})

target_compile_definitions(${TARGET_NAME} PRIVATE
  RCLONE_BROWSER_VERSION="${RCLONE_BROWSER_VERSION}"
  QT_NO_DEPRECATED_WARNINGS_SINCE=0x050F00
)
if(WIN32)
  target_compile_definitions(${TARGET_NAME} PRIVATE
    _UNICODE UNICODE NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

target_precompile_headers(${TARGET_NAME} PRIVATE pch.h)

# Every SVG in icons/fluent is compiled in automatically as :/fluent/<name>.svg
file(GLOB FLUENT_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/fluent/*.svg")
qt_add_resources(${TARGET_NAME} "fluent_icons"
  PREFIX "/fluent"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons/fluent"
  FILES ${FLUENT_ICONS})

# Folder icon, pre-rendered at each size the app uses (crisp outline)
file(GLOB FOLDER_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/folder/*.png")
qt_add_resources(${TARGET_NAME} "folder_icons"
  PREFIX "/folder"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons/folder"
  FILES ${FOLDER_ICONS})
file(GLOB DRIVE_ICONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/icons/drive/*.png")
qt_add_resources(${TARGET_NAME} "drive_icons"
  PREFIX "/drive"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/icons/drive"
  FILES ${DRIVE_ICONS})

# Warnings are on, but never fatal by default: -Werror turned every new
# compiler release into a build break for packagers. Opt in with RB_WERROR=ON.
option(RB_WERROR "Treat compiler warnings as errors" OFF)
if(MSVC)
  target_compile_options(${TARGET_NAME} PRIVATE /W4 /wd4100 /wd4189 /utf-8 /MP)
  # Exploit mitigations: Control Flow Guard, CET shadow-stack compatibility,
  # high-entropy ASLR and DEP (the last two are MSVC defaults, set explicitly).
  target_compile_options(${TARGET_NAME} PRIVATE /guard:cf)
  target_link_options(${TARGET_NAME} PRIVATE /guard:cf /CETCOMPAT /DYNAMICBASE
                      /HIGHENTROPYVA /NXCOMPAT)
  if(RB_WERROR)
    target_compile_options(${TARGET_NAME} PRIVATE /WX)
  endif()
else()
  target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra)
  if(RB_WERROR)
    target_compile_options(${TARGET_NAME} PRIVATE -Werror)
  endif()
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
  Qt::Core Qt::Gui Qt::Widgets Qt::Network Qt::Svg)

if(WIN32)
  target_link_libraries(${TARGET_NAME} PRIVATE shell32 ole32 dwmapi advapi32 crypt32)
endif()

# Isolated libsodium research suite. Never linked into the application or
# packaged binaries. OFF unless CI explicitly opts in with a system library.
option(RB_BUILD_PORTABLE_CRYPTO_PROBE
       "Build libsodium primitive tests (experimental, never packaged)" OFF)

# Parser unit tests (no GUI, no rclone needed).
option(RB_BUILD_TESTS "Build unit tests" ON)
if(RB_BUILD_TESTS)
  find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Test)
  if(Qt${QT_VERSION_MAJOR}Test_FOUND)
    enable_testing()
    add_executable(test_rclone_output ../tests/test_rclone_output.cpp rclone_output.cpp rclone_output.h)
    target_include_directories(test_rclone_output PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_rclone_output PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_rclone_output PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME rclone_output COMMAND test_rclone_output)

    add_executable(test_log_safety ../tests/test_log_safety.cpp
                   log_safety.cpp log_safety.h)
    target_include_directories(test_log_safety PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_log_safety PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_log_safety PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME log_safety COMMAND test_log_safety)

    add_executable(test_task_storage ../tests/test_task_storage.cpp
                   task_storage.cpp task_storage.h)
    target_include_directories(test_task_storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_task_storage PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_task_storage PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME task_storage COMMAND test_task_storage)

    add_executable(test_task_restore ../tests/test_task_restore.cpp task_restore.h)
    target_include_directories(test_task_restore PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_task_restore PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_task_restore PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME task_restore COMMAND test_task_restore)

    add_executable(test_task_job_codec ../tests/test_task_job_codec.cpp
                   task_job_codec.cpp task_job_codec.h
                   job_options_data.cpp job_options.h task_restore.h)
    target_include_directories(test_task_job_codec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_task_job_codec PRIVATE
                          Qt::Core Qt::Gui Qt::Widgets Qt::Test)
    set_target_properties(test_task_job_codec PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME task_job_codec COMMAND test_task_job_codec)

    if(RB_BUILD_PORTABLE_CRYPTO_PROBE)
      # Research target only. Never link libsodium into the actual application.
      # The CI Windows probe uses a SHA-256-pinned upstream MSVC DLL+import
      # library. Linux uses the distro's development package via pkg-config.
      if(WIN32)
        if(NOT DEFINED ENV{RB_SODIUM_PROBE_ROOT} OR
           "$ENV{RB_SODIUM_PROBE_ROOT}" STREQUAL "")
          message(FATAL_ERROR
            "RB_SODIUM_PROBE_ROOT is required for the Windows test-only probe")
        endif()
        set(SODIUM_PROBE_ROOT "$ENV{RB_SODIUM_PROBE_ROOT}")
        set(SODIUM_PROBE_DLL
            "${SODIUM_PROBE_ROOT}/x64/Release/v143/dynamic/libsodium.dll")
        set(SODIUM_PROBE_LIB
            "${SODIUM_PROBE_ROOT}/x64/Release/v143/dynamic/libsodium.lib")
        if(NOT EXISTS "${SODIUM_PROBE_ROOT}/include/sodium.h" OR
           NOT EXISTS "${SODIUM_PROBE_DLL}" OR
           NOT EXISTS "${SODIUM_PROBE_LIB}")
          message(FATAL_ERROR
            "Expected libsodium MSVC test archive layout is missing")
        endif()
        add_library(test_sodium_dependency SHARED IMPORTED)
        set_target_properties(test_sodium_dependency PROPERTIES
          IMPORTED_LOCATION "${SODIUM_PROBE_DLL}"
          IMPORTED_IMPLIB "${SODIUM_PROBE_LIB}"
          INTERFACE_INCLUDE_DIRECTORIES "${SODIUM_PROBE_ROOT}/include")
        set(SODIUM_PROBE_TARGET test_sodium_dependency)
      else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
        set(SODIUM_PROBE_TARGET PkgConfig::SODIUM)
      endif()

      add_executable(test_portable_crypto_probe
                     ../tests/test_portable_crypto_probe.cpp)
      target_include_directories(test_portable_crypto_probe PRIVATE
                                 ${CMAKE_CURRENT_SOURCE_DIR})
      target_link_libraries(test_portable_crypto_probe PRIVATE
                            Qt::Core Qt::Test ${SODIUM_PROBE_TARGET})
      set_target_properties(test_portable_crypto_probe PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
      if(WIN32)
        # DLL only beside the throwaway test executable. Never copy to dist/.
        add_custom_command(TARGET test_portable_crypto_probe POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SODIUM_PROBE_DLL}" "$<TARGET_FILE_DIR:test_portable_crypto_probe>"
          VERBATIM)
      endif()
      add_test(NAME portable_crypto_probe COMMAND test_portable_crypto_probe)

      # Research-only RBTSENC2 wire-format fixture. Intentionally NOT compiled
      # into RcloneBrowser, and not copied to release artifacts.
      add_executable(test_portable_v2_envelope
                     ../tests/test_portable_v2_envelope.cpp)
      target_link_libraries(test_portable_v2_envelope PRIVATE
                            Qt::Core Qt::Test ${SODIUM_PROBE_TARGET})
      set_target_properties(test_portable_v2_envelope PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
      # This reviewed, synthetic encrypted byte fixture is independent of
      # the in-test seal() implementation. Use the same file on both OSes;
      # never package it with RcloneBrowser itself.
      file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
      configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/rbtsenc2-synthetic-v1.hex"
        "${CMAKE_BINARY_DIR}/tests/rbtsenc2-synthetic-v1.hex" COPYONLY)
      if(WIN32)
        add_custom_command(TARGET test_portable_v2_envelope POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SODIUM_PROBE_DLL}" "$<TARGET_FILE_DIR:test_portable_v2_envelope>"
          VERBATIM)
      endif()
      add_test(NAME portable_v2_envelope COMMAND test_portable_v2_envelope)

      # Separate research-only guarded-memory lifetime tests. This executable
      # never links into the application and has NO install/package rule.
      add_executable(test_portable_guarded_key
                     ../tests/test_portable_guarded_key.cpp)
      target_link_libraries(test_portable_guarded_key PRIVATE
                            Qt::Core Qt::Test ${SODIUM_PROBE_TARGET})
      set_target_properties(test_portable_guarded_key PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
      if(WIN32)
        add_custom_command(TARGET test_portable_guarded_key POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SODIUM_PROBE_DLL}" "$<TARGET_FILE_DIR:test_portable_guarded_key>"
          VERBATIM)
      endif()
      add_test(NAME portable_guarded_key COMMAND test_portable_guarded_key)

      # Research-only decision matrix for mandatory vs degraded OS memory
      # locking, with a real-OS probe using public synthetic bytes only.
      # This is NOT an application policy or release-packaged executable.
      add_executable(test_portable_memory_lock_policy
                     ../tests/test_portable_memory_lock_policy.cpp)
      target_link_libraries(test_portable_memory_lock_policy PRIVATE
                            Qt::Core Qt::Test ${SODIUM_PROBE_TARGET})
      set_target_properties(test_portable_memory_lock_policy PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
      if(WIN32)
        add_custom_command(TARGET test_portable_memory_lock_policy POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SODIUM_PROBE_DLL}" "$<TARGET_FILE_DIR:test_portable_memory_lock_policy>"
          VERBATIM)
      endif()
      add_test(NAME portable_memory_lock_policy COMMAND test_portable_memory_lock_policy)

      # Compile-check a developer-only bounded Argon2id performance probe.
      # No CTest registration: CI must NEVER benchmark ephemeral runners and
      # interpret their results as an end-user hardware security policy.
      # No install/package rule references this executable.
      add_executable(portable_kdf_benchmark
                     ../tests/benchmark_portable_kdf.cpp)
      target_link_libraries(portable_kdf_benchmark PRIVATE ${SODIUM_PROBE_TARGET})
      set_target_properties(portable_kdf_benchmark PROPERTIES
                            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
      if(WIN32)
        add_custom_command(TARGET portable_kdf_benchmark POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SODIUM_PROBE_DLL}" "$<TARGET_FILE_DIR:portable_kdf_benchmark>"
          VERBATIM)
      endif()
    endif()

    add_executable(test_task_protection ../tests/test_task_protection.cpp
                   task_protection.cpp task_protection.h)
    target_include_directories(test_task_protection PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_task_protection PRIVATE Qt::Core Qt::Test)
    if(WIN32)
      target_link_libraries(test_task_protection PRIVATE crypt32)
    endif()
    set_target_properties(test_task_protection PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME task_protection COMMAND test_task_protection)

    add_executable(test_task_secret_risk
                   ../tests/test_task_secret_risk.cpp
                   task_secret_risk.cpp task_secret_risk.h)
    target_include_directories(test_task_secret_risk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_task_secret_risk PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_task_secret_risk PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME task_secret_risk COMMAND test_task_secret_risk)

    add_executable(test_task_list_transaction
                   ../tests/test_task_list_transaction.cpp task_list_transaction.h)
    target_include_directories(test_task_list_transaction PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_task_list_transaction PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_task_list_transaction PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME task_list_transaction COMMAND test_task_list_transaction)

    add_executable(test_credential_safety ../tests/test_credential_safety.cpp credential_safety.h)
    target_include_directories(test_credential_safety PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_credential_safety PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_credential_safety PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME credential_safety COMMAND test_credential_safety)

    add_executable(proxy_env_probe ../tests/proxy_env_probe.cpp)
    target_link_libraries(proxy_env_probe PRIVATE Qt::Core)
    set_target_properties(proxy_env_probe PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    add_executable(test_proxy_process_environment
                   ../tests/test_proxy_process_environment.cpp
                   proxy_process_environment.h)
    target_include_directories(test_proxy_process_environment PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_proxy_process_environment PRIVATE Qt::Core Qt::Test)
    set_target_properties(test_proxy_process_environment PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_dependencies(test_proxy_process_environment proxy_env_probe)
    add_test(NAME proxy_process_environment COMMAND test_proxy_process_environment)

    add_executable(test_proxy_credentials ../tests/test_proxy_credentials.cpp
                   proxy_credentials.cpp proxy_credentials.h)
    target_include_directories(test_proxy_credentials PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_proxy_credentials PRIVATE Qt::Core Qt::Test)
    if(WIN32)
      target_link_libraries(test_proxy_credentials PRIVATE advapi32)
    endif()
    set_target_properties(test_proxy_credentials PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    # Write a machine-readable diagnostic file even when Windows QtTest
    # cannot attach its output to the console on GitHub-hosted runners.
    add_test(NAME proxy_credentials COMMAND test_proxy_credentials
             -o "${CMAKE_BINARY_DIR}/tests/proxy_credentials.xml,xml")
  endif()
endif()
