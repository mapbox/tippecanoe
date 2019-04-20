// CLASSIFICATION: UNCLASSIFIED



/***************************************************************************/
/*
 *                               INCLUDES
 */

#include <string.h>
#include "Ellipsoid.h"


using namespace MSP::CCS;


/************************************************************************/
/*                              FUNCTIONS     
 *
 */

Ellipsoid::Ellipsoid() :
   _index( 0 ),
   _semiMajorAxis( 6378137.0 ),
   _semiMinorAxis( 6356752.3142 ),
   _flattening( 1 / 298.257223563 ),
   _eccentricitySquared( 0.0066943799901413800 ),
   _userDefined( 0 ),
   _code( 0 ),
   _name( 0 )
   
{
}


Ellipsoid::Ellipsoid( long __index, char* __code,  char* __name, double __semiMajorAxis, double __semiMinorAxis, double __flattening, double __eccentricitySquared, bool __userDefined ) :
   _index( __index ),
   _semiMajorAxis( __semiMajorAxis ),
   _semiMinorAxis( __semiMinorAxis ),
   _flattening( __flattening ),
   _eccentricitySquared( __eccentricitySquared ),
   _userDefined( __userDefined )
{
  _code = new char[ strlen( __code ) + 1 ];
 strcpy( _code,  __code );

  _name = new char[ strlen( __name ) + 1 ];
 strcpy( _name, __name );
}


Ellipsoid::~Ellipsoid()
{
  delete [] _code;
  _code = 0;

  delete [] _name;
  _name = 0;
}


long Ellipsoid::index() const
{
  return _index;
}


char* Ellipsoid::code() const
{
  return _code;
}


char* Ellipsoid::name() const
{
  return _name;
}


double Ellipsoid::semiMajorAxis() const
{
  return _semiMajorAxis;
}


double Ellipsoid::semiMinorAxis() const
{
  return _semiMinorAxis;
}


double Ellipsoid::flattening() const
{
  return _flattening;
}


double Ellipsoid::eccentricitySquared() const
{
  return _eccentricitySquared;
}


bool Ellipsoid::userDefined() const
{
  return _userDefined;
}

// CLASSIFICATION: UNCLASSIFIED
