// Bazel-only shim -- deliberately absent from p4orch/tests/Makefile.am.
//
// Make runs this suite inside the build container, where the libswsscommon deb
// has already installed /var/run/redis/sonic-db/database_config.json (see
// sonic-swss-common/debian/libswsscommon.install). test_main.cpp never
// initializes SonicDBConfig itself -- fake_dbconnector.cpp's by-name DBConnector
// constructor just maps the name to a db id -- so the first swsscommon call that
// lazily does SonicDBConfig::initialize(DEFAULT_SONIC_DB_CONFIG_FILE) finds a
// config there. That call is real: fake_producertable.cpp replaces ProducerTable
// but not ProducerStateTable, whose constructor asks
// SonicDBConfig::getSeparator(pipeline->getDBConnector()).
//
// A bazel test has no installed debs, cannot write to /var/run, and
// SonicDBConfig exposes no environment override for that absolute path, so the
// suite aborts in main() with "Sonic database config file doesn't exist".
// The same state is set up explicitly instead, from the fixture staged beside
// the binary.
//
// Unlike the tests/mock_tests copy of this shim, the work cannot hang off a
// gtest global Environment: RUN_ALL_TESTS() is the last statement of
// test_main.cpp's main(), and every orch that needs the separator is constructed
// before it. A static initializer runs early enough -- libswsscommon.so is a
// load-time dependency, so its own statics are already up by then.

#include <unistd.h>

#include "dbconnector.h"

namespace
{
    struct SonicDbConfigInit
    {
        SonicDbConfigInit()
        {
            const char *path = getenv("SONIC_TEST_DB_CONFIG_FILE");
            if (path == nullptr)
            {
                path = "./database_config.json";
            }
            if (access(path, R_OK) != 0)
            {
                return;
            }
            if (!swss::SonicDBConfig::isInit())
            {
                swss::SonicDBConfig::initialize(path);
            }
        }
    };

    const SonicDbConfigInit sonic_db_config_init;
}
