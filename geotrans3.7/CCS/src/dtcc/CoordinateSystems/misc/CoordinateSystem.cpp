// CLASSIFICATION: UNCLASSIFIED

#include "CoordinateSystem.h"


using namespace MSP::CCS;


CoordinateSystem::CoordinateSystem() :
  semiMajorAxis( 6378137.0  ),
  flattening( 1 / 298.257223563 )
{
/*
 * The constructor defaults the ellipsoid parameters to WGS84.
 *
 */
}


CoordinateSystem::CoordinateSystem( double _semiMajorAxis, double _flattening ) :
  semiMajorAxis( _semiMajorAxis  ),
  flattening( _flattening )
{
/*
 * The constructor receives the ellipsoid parameters and
 * as inputs.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 */
}


CoordinateSystem::~CoordinateSystem()
{
  semiMajorAxis = 0;
  flattening = 0;
}


/*long CoordinateSystem::setParameters( double _semiMajorAxis, double _flattening )
{

 * The function setParameters sets the current ellipsoid
 * parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
 

  semiMajorAxis = _semiMajorAxis;
  flattening = _flattening;
  
  return 0;
}*/


void CoordinateSystem::getEllipsoidParameters( double* _semiMajorAxis, double* _flattening )
{
/*
 * The function getParameters returns the current ellipsoid
 * parameters.
 *
 *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
 *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
 */

  *_semiMajorAxis = semiMajorAxis;
  *_flattening = flattening;

}

// CLASSIFICATION: UNCLASSIFIED
