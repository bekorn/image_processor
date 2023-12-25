#pragma once

#include "common.hpp"

using f_init = void(Image const & image);
#define EXPORTED_INIT_NAME _exported_init
#define EXPORTED_INIT_NAME_STR "_exported_init"

using f_process = void(Image & image);
#define EXPORTED_PROCESS_NAME _exported_process
#define EXPORTED_PROCESS_NAME_STR "_exported_process"
