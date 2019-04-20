// CLASSIFICATION: UNCLASSIFIED

#ifndef MSP_CCSTHREADMUTEX_H
#define MSP_CCSTHREADMUTEX_H

#ifndef WIN32
#   include <pthread.h>
#endif

#include "DtccApi.h"

namespace MSP
{
    class MSP_DTCC_API CCSThreadMutex
    {
    public:
        /// Default Constructor.
        CCSThreadMutex();

        /// Destructor.
        ~CCSThreadMutex();

        void lock() const;

        void unlock() const;

    private:
        // no copy operators
        CCSThreadMutex(const CCSThreadMutex&);
        CCSThreadMutex &operator=( const CCSThreadMutex&);

#ifdef WIN32
        void *mutex;
#else
        mutable pthread_mutex_t   mutex;
#endif

    };
}
#endif

// CLASSIFICATION: UNCLASSIFIED
