cmake_minimum_required(VERSION 3.21)

project(MonitorNews2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ON no Buildroot: lê o MAX30102 em vez dos dados simulados
option(MONITOR_NEWS2_HARDWARE "Usa os sensores reais em vez dos mocks" OFF)
# Como o app fala com o MAX30102: "i2cdev" (registradores direto pela /dev/i2c-1, polling) ou
# "iio" (driver do kernel + overlay i2c-sensor,max30102,int_pin=17, por interrupção)
set(MONITOR_NEWS2_SENSOR_BACKEND "i2cdev" CACHE STRING "Acesso ao MAX30102: i2cdev ou iio")
set_property(CACHE MONITOR_NEWS2_SENSOR_BACKEND PROPERTY STRINGS i2cdev iio)
# Quem calcula FC, SpO₂ e FR a partir do PPG: "processor" (PpgProcessor deste app), "pipeline"
# (método do ../ppg-pipeline: MSPTD + Smart Fusion), "hybrid" (FC e SpO₂ do pipeline, FR do
# PpgProcessor) ou "refined" (PpgProcessor, com a FC refinada pelo pipeline sobre as amostras de
# 100 Hz do driver IIO; exige MONITOR_NEWS2_SENSOR_BACKEND=iio). Contato e curvas são sempre do
# PpgProcessor. Comparação na BIDMC e no sensor real (../ppg-pipeline/scripts/comparison): o
# pipeline erra menos a FC, e o PpgProcessor a FR, sem o falso 6–7 irpm das ondas de Mayer.
set(MONITOR_NEWS2_PPG_ALGORITHM "hybrid" CACHE STRING
    "Cálculo de FC/SpO2/FR: processor, pipeline, hybrid ou refined")
set_property(CACHE MONITOR_NEWS2_PPG_ALGORITHM
    PROPERTY STRINGS processor pipeline hybrid refined)

add_subdirectory(../ppg-pipeline ppg-pipeline)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Test)

qt_standard_project_setup(REQUIRES 6.5)

add_library(monitor_core STATIC
    src/core/News2Score.h
    src/core/News2Score.cpp
    src/core/VitalSigns.h
    src/core/PatientSession.h
    src/core/AlarmPolicy.h
    src/core/AlarmPolicy.cpp
    src/core/TimeFormat.h
    src/core/TimeFormat.cpp
    src/core/SpokenNumber.h
    src/core/SpokenNumber.cpp
    src/core/TrendTracker.h
    src/core/TrendTracker.cpp
    src/core/Biquad.h
    src/core/PpgProcessor.h
    src/core/PpgProcessor.cpp
    src/core/LiveSignalBuffer.h
    src/core/LiveSignalBuffer.cpp
    src/core/PipelineVitalsEstimator.h
    src/core/PipelineVitalsEstimator.cpp
    src/sensors/Ds18b20Thermometer.h
    src/sensors/Ds18b20Thermometer.cpp
    src/devices/VitalSignsSource.h
    src/devices/AlarmIndicator.h
    src/prediction/DeteriorationPredictor.h
    src/mock/MockVitalSignsSource.h
    src/mock/MockVitalSignsSource.cpp
    src/mock/MockDeteriorationPredictor.h
    src/mock/MockDeteriorationPredictor.cpp
    src/mock/ConsoleAlarmIndicator.h
    src/mock/ConsoleAlarmIndicator.cpp
    src/mock/MockScenario.h
    src/mock/MockScenario.cpp
)
target_include_directories(monitor_core PUBLIC src)
target_link_libraries(monitor_core PUBLIC Qt6::Core ppg_pipeline)
if(MONITOR_NEWS2_PPG_ALGORITHM STREQUAL "pipeline")
    target_compile_definitions(monitor_core PUBLIC MONITOR_NEWS2_PPG_PIPELINE)
elseif(MONITOR_NEWS2_PPG_ALGORITHM STREQUAL "hybrid")
    target_compile_definitions(monitor_core PUBLIC
        MONITOR_NEWS2_PPG_PIPELINE
        MONITOR_NEWS2_PPG_HYBRID
    )
elseif(MONITOR_NEWS2_PPG_ALGORITHM STREQUAL "refined")
    if(MONITOR_NEWS2_HARDWARE AND NOT MONITOR_NEWS2_SENSOR_BACKEND STREQUAL "iio")
        message(FATAL_ERROR "refined usa as amostras de 100 Hz do driver IIO: "
                            "MONITOR_NEWS2_SENSOR_BACKEND precisa ser iio")
    endif()
    target_compile_definitions(monitor_core PUBLIC MONITOR_NEWS2_PPG_REFINED_HEART_RATE)
elseif(NOT MONITOR_NEWS2_PPG_ALGORITHM STREQUAL "processor")
    message(FATAL_ERROR
        "MONITOR_NEWS2_PPG_ALGORITHM deve ser processor, pipeline, hybrid ou refined")
endif()
message(STATUS "FC/SpO2/FR via ${MONITOR_NEWS2_PPG_ALGORITHM}")

if(MONITOR_NEWS2_HARDWARE)
    target_sources(monitor_core PRIVATE
        src/sensors/Max30102.h
        src/sensors/Max30102.cpp
        src/sensors/Max30102Iio.h
        src/sensors/Max30102Iio.cpp
        src/sensors/Max30102VitalSignsSource.h
        src/sensors/Max30102VitalSignsSource.cpp
    )
    target_compile_definitions(monitor_core PUBLIC MONITOR_NEWS2_HARDWARE)
    if(MONITOR_NEWS2_SENSOR_BACKEND STREQUAL "iio")
        target_compile_definitions(monitor_core PUBLIC MONITOR_NEWS2_SENSOR_IIO)
    elseif(NOT MONITOR_NEWS2_SENSOR_BACKEND STREQUAL "i2cdev")
        message(FATAL_ERROR "MONITOR_NEWS2_SENSOR_BACKEND deve ser i2cdev ou iio")
    endif()
    message(STATUS "MAX30102 via ${MONITOR_NEWS2_SENSOR_BACKEND}")
endif()

qt_add_executable(monitor-news2 src/main.cpp)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(monitor-news2
    URI MonitorNews2
    VERSION 1.0
    SOURCES
        src/app/MonitorController.h
        src/app/MonitorController.cpp
        src/app/PressureEntry.h
        src/app/PressureEntry.cpp
        src/ui/WaveformItem.h
        src/ui/WaveformItem.cpp
        src/ui/TrendPlotItem.h
        src/ui/TrendPlotItem.cpp
        src/ui/HatchItem.h
        src/ui/HatchItem.cpp
        src/ui/FrameItem.h
        src/ui/FrameItem.cpp
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Box.qml
        qml/ScoreBadge.qml
        qml/TouchButton.qml
        qml/SpeakerIcon.qml
        qml/MuteButton.qml
        qml/ProvenanceMark.qml
        qml/TopBar.qml
        qml/MonitorScreen.qml
        qml/WaveRow.qml
        qml/TemperatureTile.qml
        qml/PressureTile.qml
        qml/ScorePanel.qml
        qml/BottomBar.qml
        qml/PredictionHelpModal.qml
        qml/ScoreHistoryChart.qml
        qml/UiText.qml
        qml/ScreenHeader.qml
        qml/RegisterScreen.qml
        qml/TrendsScreen.qml
        qml/HandoffScreen.qml
        qml/HoldButton.qml
    RESOURCES
        fonts/IBMPlexSansCondensed-Regular.ttf
        fonts/IBMPlexSansCondensed-Medium.ttf
        fonts/IBMPlexSansCondensed-SemiBold.ttf
        fonts/IBMPlexSansCondensed-Bold.ttf
        fonts/IBMPlexMono-Regular.ttf
        fonts/IBMPlexMono-Medium.ttf
        fonts/IBMPlexMono-SemiBold.ttf
)

target_include_directories(monitor-news2 PRIVATE src/app src/ui)
target_link_libraries(monitor-news2 PRIVATE monitor_core Qt6::Gui Qt6::Quick)

# exporta FC/FR do PpgProcessor no formato do ppg-pipeline, para a validação nas bases públicas
add_executable(ppg-processor-export tools/PpgProcessorExport.cpp)
target_link_libraries(ppg-processor-export PRIVATE monitor_core)

enable_testing()

function(add_monitor_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE monitor_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_monitor_test(tst_news2score)
add_monitor_test(tst_alarmpolicy)
add_monitor_test(tst_spokennumber)
add_monitor_test(tst_trendtracker)
add_monitor_test(tst_ppgprocessor)
add_monitor_test(tst_pipelinevitalsestimator)
add_monitor_test(tst_ds18b20thermometer)
