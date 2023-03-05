#include "mainwindow.h"

#include <lmrs/core/staticinit.h>
#include <lmrs/core/userliterals.h>

#include <QApplication>

namespace lmrs::studio {

class Application : public core::StaticInit<Application, QApplication>
{
public:
    using StaticInit::StaticInit;
    static void staticConstructor();

    int run();
};

void Application::staticConstructor()
{
    setApplicationName("ESU Studio"_L1);
    setOrganizationDomain("taschenorakel.de"_L1);
}

int Application::run()
{
    const auto mainWindow = new MainWindow();
    mainWindow->show();

    return exec();
}

} // namespace lmrs::studio

int main(int argc, char *argv[])
{
    return lmrs::studio::Application{argc, argv}.run();
}
