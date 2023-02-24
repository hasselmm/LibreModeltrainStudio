#include "mainwindow.h"

#include <lmrs/core/fileformat.h>
#include <lmrs/core/localization.h>
#include <lmrs/core/staticinit.h>
#include <lmrs/core/symbolictrackplanmodel.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/roco/z21appfileformats.h>

#include <lmrs/esu/lp2device.h>
#include <lmrs/kpfzeller/scdevice.h>
#include <lmrs/roco/z21device.h>
#include <lmrs/zimo/mx1device.h>

#include <QApplication>

namespace lmrs::studio {

const QtMessageHandler defaultMessageHandler = qInstallMessageHandler([](auto type, auto context, auto message) {
    defaultMessageHandler(type, context, message);
});

class Application
        : public core::StaticInit<Application>
        , public QApplication
{
public:
    using QApplication::QApplication;
    static void staticConstructor();

    int run();
};

void Application::staticConstructor()
{
    setApplicationName("Libre Modelrail Studio"_L1);
    setApplicationVersion(LMRS_VERSION_STRING);
    setOrganizationDomain("taschenorakel.de"_L1);
//    setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Round);
    setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::Floor);
}

int Application::run()
{
    const auto languages = std::make_unique<core::l10n::LanguageManager>(this);

    core::DeviceFactory::addDeviceFactory(new esu::lp2::DeviceFactory{this});
    core::DeviceFactory::addDeviceFactory(new kpfzeller::DeviceFactory{this});
    core::DeviceFactory::addDeviceFactory(new roco::z21::DeviceFactory{this});
    core::DeviceFactory::addDeviceFactory(new zimo::mx1::DeviceFactory{this});

    core::registerFileFormat<roco::z21app::LayoutReader>(core::FileFormat::z21Layout());
    core::registerFileFormat<roco::z21app::LayoutWriter>(core::FileFormat::z21Layout());

    const auto mainWindow = std::make_unique<MainWindow>();
    mainWindow->setLanguageManager(languages.get());
    mainWindow->show();

    return exec();
}

} // namespace lmrs::studio

int main(int argc, char *argv[])
{
//    qputenv("QT_LOGGING_RULES", "lmrs.roco.z21.Client.stream.debug=true");

    qSetMessagePattern("%{time process}/%{pid} "
                       "[%{type}%{if-category} %{category}%{endif}] "
                       "%{message} (%{file}, line %{line})"_L1);

    return lmrs::studio::Application{argc, argv}.run();
}

// libre model train kit
