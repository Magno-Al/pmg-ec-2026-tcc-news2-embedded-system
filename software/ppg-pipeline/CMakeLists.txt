cmake_minimum_required(VERSION 3.16)

project(tcc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_MOCK_PPG_SOURCE "Usa o CSV do OpenOximetry como fonte de dados PPG em vez do MAX30102 real" ON)

if(USE_MOCK_PPG_SOURCE)
    set(PPG_SOURCE_FILE src/PPGSourceMock.cpp)
else()
    set(PPG_SOURCE_FILE src/PPGSourceMAX30102.cpp)
endif()

add_executable(tcc src/main.cpp src/DS18B20.cpp src/PPGBuffer.cpp src/PPGFilter.cpp src/PPGPeaks.cpp src/PPGMsptdBeatDetector.cpp src/PPGHeartRate.cpp src/PPGQuality.cpp src/PPGSpO2.cpp src/PPGRespiratorySignal.cpp src/PPGRespiratoryRate.cpp src/PPGDebug.cpp ${PPG_SOURCE_FILE})
target_include_directories(tcc PRIVATE include)

enable_testing()
add_executable(ppg_heart_rate_tests tests/test_heart_rate.cpp src/PPGHeartRate.cpp src/PPGPeaks.cpp src/PPGMsptdBeatDetector.cpp src/PPGBuffer.cpp)
target_include_directories(ppg_heart_rate_tests PRIVATE include)
add_test(NAME heart_rate COMMAND ppg_heart_rate_tests)

add_executable(ppg_quality_tests tests/test_quality.cpp src/PPGQuality.cpp src/PPGBuffer.cpp)
target_include_directories(ppg_quality_tests PRIVATE include)
add_test(NAME quality COMMAND ppg_quality_tests)

add_executable(ppg_spo2_tests tests/test_spo2.cpp src/PPGSpO2.cpp src/PPGQuality.cpp src/PPGBuffer.cpp)
target_include_directories(ppg_spo2_tests PRIVATE include)
add_test(NAME spo2 COMMAND ppg_spo2_tests)

add_executable(ppg_peaks_tests tests/test_peaks.cpp src/PPGPeaks.cpp src/PPGMsptdBeatDetector.cpp src/PPGBuffer.cpp)
target_include_directories(ppg_peaks_tests PRIVATE include)
add_test(NAME peaks COMMAND ppg_peaks_tests)

add_executable(ppg_rr_tests tests/test_respiratory_rate.cpp src/PPGRespiratoryRate.cpp src/PPGRespiratorySignal.cpp src/PPGPeaks.cpp src/PPGMsptdBeatDetector.cpp src/PPGBuffer.cpp src/PPGQuality.cpp)
target_include_directories(ppg_rr_tests PRIVATE include)
add_test(NAME respiratory_rate COMMAND ppg_rr_tests)

add_executable(ppg_msptd_tests tests/test_msptd.cpp src/PPGMsptdBeatDetector.cpp)
target_include_directories(ppg_msptd_tests PRIVATE include)
add_test(NAME msptd COMMAND ppg_msptd_tests)