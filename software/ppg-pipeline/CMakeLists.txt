cmake_minimum_required(VERSION 3.16)

project(tcc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Núcleo do método (FC, SpO2, FR, qualidade, DS18B20), sem fonte de dados: usado pelo
# executável de validação abaixo e pelo app monitor-news2, que fornece as amostras do MAX30102.
add_library(ppg_pipeline STATIC
    src/DS18B20.cpp src/PPGBuffer.cpp src/PPGFilter.cpp src/PPGPeaks.cpp
    src/PPGMsptdBeatDetector.cpp src/PPGHeartRate.cpp src/PPGQuality.cpp src/PPGSpO2.cpp
    src/PPGRespiratorySignal.cpp src/PPGRespiratoryRate.cpp
)
target_include_directories(ppg_pipeline PUBLIC include)

# O executável de validação e os testes só entram quando este projeto é compilado sozinho;
# dentro do app (add_subdirectory) fica só a biblioteca.
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    return()
endif()

# Lê um CSV preparado das bases (BIDMC, OpenOximetry) no lugar do sensor; ver README, seção 2.
add_executable(tcc src/main.cpp src/PPGDebug.cpp src/PPGSourceMock.cpp)
target_link_libraries(tcc PRIVATE ppg_pipeline)

enable_testing()
add_executable(ppg_heart_rate_tests tests/test_heart_rate.cpp)
target_link_libraries(ppg_heart_rate_tests PRIVATE ppg_pipeline)
add_test(NAME heart_rate COMMAND ppg_heart_rate_tests)

add_executable(ppg_quality_tests tests/test_quality.cpp)
target_link_libraries(ppg_quality_tests PRIVATE ppg_pipeline)
add_test(NAME quality COMMAND ppg_quality_tests)

add_executable(ppg_spo2_tests tests/test_spo2.cpp)
target_link_libraries(ppg_spo2_tests PRIVATE ppg_pipeline)
add_test(NAME spo2 COMMAND ppg_spo2_tests)

add_executable(ppg_peaks_tests tests/test_peaks.cpp)
target_link_libraries(ppg_peaks_tests PRIVATE ppg_pipeline)
add_test(NAME peaks COMMAND ppg_peaks_tests)

add_executable(ppg_rr_tests tests/test_respiratory_rate.cpp)
target_link_libraries(ppg_rr_tests PRIVATE ppg_pipeline)
add_test(NAME respiratory_rate COMMAND ppg_rr_tests)

add_executable(ppg_msptd_tests tests/test_msptd.cpp)
target_link_libraries(ppg_msptd_tests PRIVATE ppg_pipeline)
add_test(NAME msptd COMMAND ppg_msptd_tests)
