#
# PC Platform
#
add_library(${COMPONENT_LIB} STATIC
    ${COMPONENT_SRCS_C}
    ${COMPONENT_SRCS_CPP}
)

target_include_directories(${COMPONENT_LIB} PUBLIC ${COMPONENT_INCLUDE_DIRS})
target_include_directories(${COMPONENT_LIB} PRIVATE ${COMPONENT_PRIVATE_INCLUDE_DIRS})
target_link_libraries(${COMPONENT_LIB} PUBLIC
    brookesia_system_core
    brookesia_service_helper
    brookesia_service_display
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,-U,app_ess_provider_symbol")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,-u,app_ess_provider_symbol")
endif()

if(NOT DEFINED BROOKESIA_SYSTEM_CORE_DIR)
    set(BROOKESIA_SYSTEM_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../system/brookesia_system_core")
endif()

set(app_ess_package_id "brookesia.ess")
include("${BROOKESIA_SYSTEM_CORE_DIR}/cmake/runtime_app_stage.cmake")
brookesia_system_core_get_pc_runtime_app_stage_root(app_ess_stage_root)
brookesia_stage_runtime_app_package(
    PACKAGE_ID "${app_ess_package_id}"
    SOURCE_DIR "${COMPONENT_DIR}/package"
    STAGE_ROOT "${app_ess_stage_root}"
    NO_INDEX
)
