#pragma once

#include <QStringList>

namespace rolly {

int runCli(QStringList arguments, QStringList *files = nullptr);
void printHelp();

}
