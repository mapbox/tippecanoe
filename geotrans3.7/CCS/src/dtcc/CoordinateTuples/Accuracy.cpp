// CLASSIFICATION: UNCLASSIFIED

#include "Accuracy.h"


using namespace MSP::CCS;


Accuracy::Accuracy() :
  _circularError90( -1.0 ),
  _linearError90( -1.0 ),
  _sphericalError90( -1.0 )
{
}


Accuracy::Accuracy( double __circularError90, double __linearError90, double __sphericalError90 ) :
  _circularError90( __circularError90 ),
  _linearError90( __linearError90 ),
  _sphericalError90( __sphericalError90 )
{
}


Accuracy::Accuracy( const Accuracy &a )
{
  _circularError90 = a._circularError90;
  _linearError90 = a._linearError90;
  _sphericalError90 = a._sphericalError90;
}


Accuracy::~Accuracy( void )
{
  _circularError90 = 0;
  _linearError90 = 0;
  _sphericalError90 = 0;
}


Accuracy& Accuracy::operator=( const Accuracy &a )
{
  if( this != &a )
  {
    _circularError90 = a._circularError90;
    _linearError90 = a._linearError90;
    _sphericalError90 = a._sphericalError90;
  }

  return *this;
}


void Accuracy::set( double __circularError90, double __linearError90, double __sphericalError90 )
{
  _circularError90 = __circularError90;
  _linearError90 = __linearError90;
  _sphericalError90 = __sphericalError90;
}


void Accuracy::setCircularError90( double __circularError90 )
{
  _circularError90 = __circularError90;
}


void Accuracy::setLinearError90( double __linearError90 )
{
  _linearError90 = __linearError90;
}


void Accuracy::setSphericalError90( double __sphericalError90 )
{
  _sphericalError90 = __sphericalError90;
}


double Accuracy::circularError90()
{
  return _circularError90;
}


double Accuracy::linearError90()
{
  return _linearError90;
}


double Accuracy::sphericalError90()
{
  return _sphericalError90;
}

// CLASSIFICATION: UNCLASSIFIED
