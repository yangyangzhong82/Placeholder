#include "Entry/Entry.h"
#include "ll/api/io/Logger.h"

namespace VS{
inline ll::io::Logger& logger = Entry::getInstance().getSelf().getLogger(); // logger.Trace Debug Info Warn Error Fatal
} // namespace CT