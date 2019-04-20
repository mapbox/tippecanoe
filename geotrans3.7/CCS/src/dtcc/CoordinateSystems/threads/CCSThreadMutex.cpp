// CLASSIFICATION: UNCLASSIFIED

#ifdef WIN32
#  include <windows.h>
#endif

#include "CCSThreadMutex.h"

using MSP::CCSThreadMutex;

CCSThreadMutex::CCSThreadMutex()
{
#ifdef WIN32
   mutex = (void*)CreateMutex(NULL,FALSE,NULL);
#elif NDK_BUILD
   // do nothing for Android
#else
   // force a recursive mutex to match windows implementation.
   pthread_mutexattr_t attr;
   pthread_mutexattr_init(&attr);
   pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
   pthread_mutex_init(&mutex, &attr);
   pthread_mutexattr_destroy(&attr);
#endif
}

CCSThreadMutex::~CCSThreadMutex()
{
#ifdef WIN32
   CloseHandle((HANDLE)mutex);
#elif NDK_BUILD
   // do nothing for Android
#else
   // force a recursive mutex to match windows implementation.
   pthread_mutex_destroy(&mutex);
#endif
}
void
CCSThreadMutex::lock() const
{
#ifdef WIN32
   WaitForSingleObject((HANDLE)mutex, INFINITE);
#elif NDK_BUILD
   // do nothing for Android
#else
   // force a recursive mutex to match windows implementation.
   pthread_mutex_lock(&mutex);
#endif
}


void
CCSThreadMutex::unlock() const
{
#ifdef WIN32
   ReleaseMutex((HANDLE)mutex);
#elif NDK_BUILD
   // do nothing for Android
#else
   // force a recursive mutex to match windows implementation.
   pthread_mutex_unlock(&mutex);
#endif
}

// CLASSIFICATION: UNCLASSIFIED
