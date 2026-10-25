#pragma once
#include "Component.h"

struct SystemContext;

/**
 * Base class for the systems in our ECS, Systems are meant to override this class and be owned by the System Registry
 */
class System
{
public:
    /**
     * Creates a system with the given name and info of components that it writes or reads
     * @param name_ Name to give to the system for debugging
     * @param reads_ Component mask of the components this system will read 
     * @param writes_ Component mask of the components this system will write 
     */
    System(const char* name_, ComponentMask reads_, ComponentMask writes_);
    virtual ~System() = default;
    
    /* The registry owns each system, so no copies or moving */
    System(const System&) = delete;
    System& operator=(const System&) = delete;
    System(System&&) = delete;
    System& operator=(System&&) = delete;

    /**
     * Runs the logic for this system
     * @param systemContext context this system needs to run
     */
    virtual void Run(const SystemContext& systemContext) = 0;

    /**
     * @return Returns the name of this system
     */
    const char* GetName() const { return name; }
    /**
     * @return Returns which components this system reads as a mask
     */
    ComponentMask GetReads() const { return reads; }
    /**
     * @return Returns which components this system writes as a mask
     */
    ComponentMask GetWrites() const { return writes; }
    
    
private:
    /** Name of the System, used for debugging */
    const char* name;
    /** Component mask of the components this system will read */
    ComponentMask reads;
    /** Component mask of the components this system will write */
    ComponentMask writes;
    
};
