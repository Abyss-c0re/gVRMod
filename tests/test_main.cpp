#include "test_framework.h"

#ifdef VRMOD_DEV_BUILD
#include "core/vrmod_log.h"
#endif

int main() {
#ifdef VRMOD_DEV_BUILD
    vrmod_log_init("vrmod_test.log");
#endif

    int result = RunAllTests();

#ifdef VRMOD_DEV_BUILD
    vrmod_log_close();
#endif

    return result;
}
