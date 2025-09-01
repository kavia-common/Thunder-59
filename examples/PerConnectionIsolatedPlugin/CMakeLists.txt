cmake_minimum_required(VERSION 3.10)

project(PerConnectionIsolatedPlugin)

find_package(Thunder REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${PROJECT_NAME} SHARED
    PerConnectionIsolatedPlugin.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${Thunder_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${Thunder_LIBRARIES}
        CompileSettingsDebug::CompileSettingsDebug
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES
    POSITION_INDEPENDENT_CODE ON
)

# Install to Thunder plugins dir layout if available
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib/wpeframework/plugins
)
