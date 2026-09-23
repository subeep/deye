cmake_minimum_required(VERSION 3.16)

# Force system linker (avoid Linuxbrew ld which can't resolve system .so deps)
# Must be set before project() so the compiler tests also use the right linker.
set(CMAKE_LINKER /usr/bin/ld CACHE FILEPATH "Linker" FORCE)

project(drone_recorder C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Find System Packages ───────────────────────────────────────────────────
find_package(UHD 4.0 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3F REQUIRED fftw3f)

# ─── ImGui (vendored) ────────────────────────────────────────────────────────
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# ─── ImPlot (vendored) ───────────────────────────────────────────────────────
set(IMPLOT_DIR ${CMAKE_SOURCE_DIR}/vendor/implot)
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
)
add_library(implot STATIC ${IMPLOT_SOURCES})
target_include_directories(implot PUBLIC ${IMPLOT_DIR})
target_link_libraries(implot PUBLIC imgui)

# ─── Main Executable ─────────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.cpp
    src/usrp_worker.cpp
    src/signal_proc.cpp
    src/recorder.cpp
    src/ui/control_panel.cpp
    src/ui/plot_panel.cpp
    src/ui/droneid_panel.cpp
    src/detector/controller.cpp
    src/detector/detector.cpp
    src/detector/hopping.cpp
)

add_executable(drone_recorder ${APP_SOURCES})

target_include_directories(drone_recorder PRIVATE
    src
    ${UHD_INCLUDE_DIRS}
    ${FFTW3F_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
)

target_compile_options(drone_recorder PRIVATE
    ${FFTW3F_CFLAGS_OTHER}
    -O2
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# ─── Extra transitive dependencies ──────────────────────────────────────────
# libuhd.so needs Boost, Python3 (for scripting hooks), libusb, dl, rt
# libglfw.so (X11 mode) needs libX11, etc.
find_package(Boost REQUIRED COMPONENTS thread filesystem serialization program_options)
find_package(Python3 REQUIRED COMPONENTS Development)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Receive-only DJI decoder dependency (GPL-3.0-or-later; see docs/DETECTOR.md).
add_library(drone_turbo SHARED
    src/detector/turbo_bridge.c
    vendor/turbofec/src/turbo_dec.c
    vendor/turbofec/src/turbo_enc.c
    vendor/turbofec/src/turbo_rate_match.c)
target_include_directories(drone_turbo PRIVATE vendor/turbofec/include)
target_link_libraries(drone_turbo PRIVATE m)
target_compile_options(drone_turbo PRIVATE -O2)
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i.86")
    message(FATAL_ERROR "The bundled DJI turbo decoder requires x86 SSSE3")
endif()
target_compile_options(drone_turbo PRIVATE -mssse3)
target_compile_definitions(drone_turbo PRIVATE HAVE_SSE3)
target_compile_definitions(drone_recorder PRIVATE
    DETECTOR_PYTHON="${Python3_EXECUTABLE}"
    DETECTOR_SCRIPT="${CMAKE_SOURCE_DIR}/src/detector/python/backend.py"
    DETECTOR_TURBO="$<TARGET_FILE:drone_turbo>")
add_dependencies(drone_recorder drone_turbo)
find_package(X11 REQUIRED)
# libusb: prefer -dev symlink, fall back to versioned .so
find_library(LIBUSB_1
    NAMES usb-1.0 libusb-1.0.so.0
    PATHS /usr/lib/x86_64-linux-gnu
    NO_DEFAULT_PATH)
if(NOT LIBUSB_1)
    set(LIBUSB_1 /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0)
endif()
find_library(LIBDPDK dpdk PATHS /usr/lib/x86_64-linux-gnu)  # optional

target_link_libraries(drone_recorder PRIVATE
    imgui
    implot
    ${UHD_LIBRARIES}
    ${FFTW3F_LIBRARIES}
    OpenGL::GL
    glfw
    ${Boost_LIBRARIES}
    Python3::Python
    ${X11_LIBRARIES}
    ${LIBUSB_1}
    dl
    rt
    pthread
)

target_link_options(drone_recorder PRIVATE "-B/usr/bin/")

message(STATUS "UHD version: ${UHD_VERSION}")
message(STATUS "UHD include: ${UHD_INCLUDE_DIRS}")
message(STATUS "UHD libs: ${UHD_LIBRARIES}")
message(STATUS "FFTW3F: ${FFTW3F_LIBRARIES}")

include(CTest)
if(BUILD_TESTING)
    add_executable(test_hopping tests/test_hopping.cpp src/detector/hopping.cpp)
    target_include_directories(test_hopping PRIVATE src)
    add_test(NAME hopping COMMAND test_hopping ${CMAKE_SOURCE_DIR}/tests/data/elrs_seed_01020304_80.csv)
    add_test(NAME detector_dsp COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/test_detector.py $<TARGET_FILE:drone_turbo>)
    set_tests_properties(detector_dsp PROPERTIES TIMEOUT 60)
    add_executable(test_detector_worker tests/test_detector_worker.cpp src/detector/detector.cpp)
    target_include_directories(test_detector_worker PRIVATE src)
    target_compile_definitions(test_detector_worker PRIVATE
        DETECTOR_PYTHON="${Python3_EXECUTABLE}"
        DETECTOR_SCRIPT="${CMAKE_SOURCE_DIR}/src/detector/python/backend.py"
        DETECTOR_TURBO="$<TARGET_FILE:drone_turbo>")
    target_link_libraries(test_detector_worker PRIVATE pthread)
    add_dependencies(test_detector_worker drone_turbo)
    add_test(NAME detector_worker COMMAND test_detector_worker ${CMAKE_SOURCE_DIR}/tests/data/dji_mavic_air_2.fc32)
    set_tests_properties(detector_worker PROPERTIES TIMEOUT 30)
    add_executable(detector_live_check tests/detector_live_check.cpp src/usrp_worker.cpp src/detector/detector.cpp)
    target_include_directories(detector_live_check PRIVATE src ${UHD_INCLUDE_DIRS})
    target_compile_definitions(detector_live_check PRIVATE
        DETECTOR_PYTHON="${Python3_EXECUTABLE}"
        DETECTOR_SCRIPT="${CMAKE_SOURCE_DIR}/src/detector/python/backend.py"
        DETECTOR_TURBO="$<TARGET_FILE:drone_turbo>")
    target_link_libraries(detector_live_check PRIVATE ${UHD_LIBRARIES} ${Boost_LIBRARIES}
        Python3::Python ${LIBUSB_1} dl rt pthread)
    target_link_options(detector_live_check PRIVATE "-B/usr/bin/")
    add_dependencies(detector_live_check drone_turbo)
    add_executable(test_controller tests/test_controller.cpp src/detector/controller.cpp
        src/detector/detector.cpp src/detector/hopping.cpp)
    target_include_directories(test_controller PRIVATE src ${UHD_INCLUDE_DIRS} ${FFTW3F_INCLUDE_DIRS})
    target_compile_definitions(test_controller PRIVATE
        DETECTOR_PYTHON="${Python3_EXECUTABLE}"
        DETECTOR_SCRIPT="${CMAKE_SOURCE_DIR}/src/detector/python/backend.py"
        DETECTOR_TURBO="$<TARGET_FILE:drone_turbo>")
    target_link_libraries(test_controller PRIVATE pthread)
    add_dependencies(test_controller drone_turbo)
    add_test(NAME detector_controller COMMAND test_controller)
    add_test(NAME detector_benchmark COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/test_benchmark.py)
    add_test(NAME detector_preprocessing COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/test_preprocessing.py)
    add_test(NAME detector_dataset COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/tests/test_dronedetect.py)
endif()
