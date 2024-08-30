#include <QCoreApplication>
#include "modbusservice.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    ModBusService obj;
    return a.exec();
}
