// CLASSIFICATION: UNCLASSIFIED

#ifndef MSP_CCSTHREADLOCK_H
#define MSP_CCSTHREADLOCK_H

#include "DtccApi.h"

namespace MSP
{
    class CCSThreadMutex;
}

namespace MSP
{
    class MSP_DTCC_API CCSThreadLock
    {
    public:
        /// Default Constructor.
        CCSThreadLock(const CCSThreadMutex *mutex);

        /// Destructor.
        ~CCSThreadLock();

    private:
        // no copy operators
        CCSThreadLock(const CCSThreadLock&);
        CCSThreadLock &operator=( const CCSThreadLock&);

        const CCSThreadMutex *mutex;
    };
}
#endif

// CLASSIFICATION: UNCLASSIFIED
