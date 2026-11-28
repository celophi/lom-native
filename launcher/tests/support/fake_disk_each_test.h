#ifndef LOM_LAUNCHER_FAKE_DISK_EACH_TEST_H
#define LOM_LAUNCHER_FAKE_DISK_EACH_TEST_H

#include "files_provider_fake.h"

namespace lom::launcher::test {

/**
 * @brief The fake disk the running test has. Unit tests each get a fresh one, so they touch nothing outside.
 * @return The fake.
 */
FilesProviderFake& fake_disk();

}

#endif
