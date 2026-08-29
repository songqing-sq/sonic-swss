// Bazel-only shim -- deliberately absent from tests/mock_tests/Makefile.am.
//
// Make runs these suites inside the build container, where the libswsscommon
// deb has already installed /var/run/redis/sonic-db/database_config.json (see
// sonic-swss-common/debian/libswsscommon.install). Any swsscommon call that
// lazily does SonicDBConfig::initialize(DEFAULT_SONIC_DB_CONFIG_FILE) therefore
// finds a config there -- for instance the ProducerStateTable constructor,
// reached from suites that build DBConnector by db id and so never go through
// mock_dbconnector.cpp's by-name constructor (the one place that points
// SonicDBConfig at ./database_config.json).
//
// A bazel test has no installed debs, cannot write to /var/run, and
// SonicDBConfig exposes no environment override for that absolute path. So the
// same state is set up explicitly instead: load the config from the fixture the
// test already stages beside itself, which is the very file
// mock_dbconnector.cpp reads. Suites that stage no such file are left with an
// uninitialized SonicDBConfig, exactly as before.

#include <stdlib.h>
#include <unistd.h>

#include "dbconnector.h"
#include "gtest/gtest.h"

namespace
{
    class SonicDbConfigEnvironment : public ::testing::Environment
    {
    public:
        void SetUp() override
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

    [[maybe_unused]] const bool registered =
        (::testing::AddGlobalTestEnvironment(new SonicDbConfigEnvironment), true);
}
