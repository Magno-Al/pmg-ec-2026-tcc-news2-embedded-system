#include "app/MonitorController.h"
#include "mock/ConsoleAlarmIndicator.h"
#include "mock/MockDeteriorationPredictor.h"
#include "mock/MockScenario.h"
#include "mock/MockVitalSignsSource.h"

#ifdef MONITOR_NEWS2_HARDWARE
#include "sensors/Max30102VitalSignsSource.h"
#endif

#include <fcntl.h>
#include <unistd.h>

#include <QCommandLineParser>
#include <QDateTime>
#include <QDirIterator>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSocketNotifier>

#include <csignal>

namespace
{

int terminationPipe[2] = {-1, -1};
int screenshotPipe[2] = {-1, -1};

// Só funções async-signal-safe aqui: o aviso vai por um pipe e o Qt encerra o laço de eventos
// normalmente, o que roda os destrutores — inclusive o que desliga o buffer IIO e os LEDs do
// sensor. Sem isso, "killall monitor-news2" deixava o MAX30102 aceso.
void onTerminationSignal(int)
{
    const char signalled = 1;
    [[maybe_unused]] const ssize_t written = ::write(terminationPipe[1], &signalled, 1);
}

// Depuração no equipamento, sem servidor gráfico (eglfs desenha direto no DRM): "kill -USR1"
// salva a janela em /tmp/monitor-news2-<hora>.png.
void onScreenshotSignal(int)
{
    const char signalled = 1;
    [[maybe_unused]] const ssize_t written = ::write(screenshotPipe[1], &signalled, 1);
}

void saveScreenshot(QQmlApplicationEngine& engine)
{
    const QList<QObject*> roots = engine.rootObjects();
    auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
    if(!window)
    {
        return;
    }
    const QString path = QStringLiteral("/tmp/monitor-news2-%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HHmmss")));
    if(window->grabWindow().save(path))
    {
        qInfo().noquote() << "captura de tela:" << path;
    }
}

void loadBundledFonts()
{
    QDirIterator fonts(QStringLiteral(":/qt/qml/MonitorNews2/fonts"), {"*.ttf"});
    while(fonts.hasNext())
    {
        if(QFontDatabase::addApplicationFont(fonts.next()) < 0)
        {
            qWarning() << "Falha ao carregar fonte" << fonts.filePath();
        }
    }
}

}

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("monitor-news2"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption deteriorationOption(
        QStringLiteral("deterioration-seconds"),
        QStringLiteral("Intervalo da subida simulada de FR (+1 irpm até 25); 0 desliga."),
        QStringLiteral("seconds"),
        QStringLiteral("75")
    );
    QCommandLineOption fullscreenOption(
        QStringLiteral("fullscreen"),
        QStringLiteral("Ocupa a tela inteira, como no equipamento.")
    );
    parser.addOption(deteriorationOption);
    parser.addOption(fullscreenOption);
    parser.process(application);

    loadBundledFonts();

    QSocketNotifier* terminationNotifier = nullptr;
    if(pipe2(terminationPipe, O_CLOEXEC) == 0)
    {
        terminationNotifier = new QSocketNotifier(
            terminationPipe[0], QSocketNotifier::Read, &application
        );
        QObject::connect(
            terminationNotifier, &QSocketNotifier::activated,
            &application, &QCoreApplication::quit
        );
        std::signal(SIGTERM, onTerminationSignal);
        std::signal(SIGINT, onTerminationSignal);
        std::signal(SIGHUP, onTerminationSignal);
    }

#ifdef MONITOR_NEWS2_HARDWARE
    // build do equipamento (Buildroot): FC, SpO₂ e FR do MAX30102; --deterioration-seconds
    // não se aplica
    Max30102VitalSignsSource vitalSignsSource;
#else
    MockVitalSignsSource vitalSignsSource(
        MockScenario::initialVitals(),
        parser.value(deteriorationOption).toInt()
    );
#endif
    MockDeteriorationPredictor predictor;
    ConsoleAlarmIndicator alarmIndicator;
    MonitorController controller(
        MockScenario::createSession(),
        vitalSignsSource,
        predictor,
        alarmIndicator
    );
    MonitorController::setInstance(&controller);
    controller.start();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"isFullScreen", parser.isSet(fullscreenOption)}});
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection
    );
    engine.loadFromModule("MonitorNews2", "Main");

    if(pipe2(screenshotPipe, O_CLOEXEC) == 0)
    {
        auto* screenshotNotifier = new QSocketNotifier(
            screenshotPipe[0], QSocketNotifier::Read, &application
        );
        QObject::connect(screenshotNotifier, &QSocketNotifier::activated, &application, [&engine] {
            char signalled = 0;
            [[maybe_unused]] const ssize_t consumed = ::read(screenshotPipe[0], &signalled, 1);
            saveScreenshot(engine);
        });
        std::signal(SIGUSR1, onScreenshotSignal);
    }

    return application.exec();
}
