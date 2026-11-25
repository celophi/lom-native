#ifndef LOM_LAUNCHER_SPEC_MATCHERS_H
#define LOM_LAUNCHER_SPEC_MATCHERS_H

#include "spec_printers.h"

#include <gmock/gmock.h>

#include <QString>

namespace lom::launcher::test {

/** Matches text containing the expected passage. */
MATCHER_P(ContainsText, passage, "contains \"" + QString(passage).toStdString() + "\"") {
    return arg.contains(QString(passage));
}

/** Matches text beginning with the expected passage. */
MATCHER_P(StartsWithText, passage, "starts with \"" + QString(passage).toStdString() + "\"") {
    return arg.startsWith(QString(passage));
}

}

#endif
