// CLASSIFICATION: UNCLASSIFIED

#include "CCSThreadLock.h"
#include "CCSThreadMutex.h"

using namespace MSP;

CCSThreadLock::CCSThreadLock(const CCSThreadMutex *theMutex)
    : mutex(theMutex)
{
  mutex->lock();
}

CCSThreadLock::~CCSThreadLock()
{
  mutex->unlock();
}


// CLASSIFICATION: UNCLASSIFIED
