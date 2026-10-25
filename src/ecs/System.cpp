#include "ecs/System.h"

System::System(const char* name_, const ComponentMask reads_, const ComponentMask writes_) : name(name_), reads(reads_),
    writes(writes_)
{
}
