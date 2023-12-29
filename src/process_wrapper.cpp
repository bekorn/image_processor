#include "common.hpp"
#include "process.hpp"

#ifndef PROC_PATH
#error PROC_PATH NOT defined
#endif

namespace {
///--- User Code Begin
#include PROC_PATH
///--- User Code End
}

#define EXPORT extern "C" __declspec(dllexport)

// These will check for function signature and make the user code cleaner
EXPORT void EXPORTED_INIT_NAME(Image const & image) { init(image); }
EXPORT void EXPORTED_PROCESS_NAME(Image & image) { process(image); }
