// CLASSIFICATION: UNCLASSIFIED

#ifndef CoordinateSystem_H
#define CoordinateSystem_H

#include "CoordinateSystem.h"
#include "DtccApi.h"

namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API CoordinateSystem
    {
    public:

      /*
       * The constructor defaults the ellipsoid parameters to WGS84.
       *
       */

      CoordinateSystem();


      /*
       * The constructor receives the ellipsoid parameters and
       * as inputs.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (input)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (input)
       */

      CoordinateSystem( double _semiMajorAxis, double _flattening );


      ~CoordinateSystem();


      /*
       * The function getParameters returns the current ellipsoid
       * parameters.
       *
       *    ellipsoidSemiMajorAxis  : Semi-major axis of ellipsoid, in meters   (output)
       *    ellipsoidFlattening     : Flattening of ellipsoid                   (output)
       */

      void getEllipsoidParameters( double* _semiMajorAxis, double* _flattening );

    protected:

      double semiMajorAxis;
      double flattening;

    };
  }
}

#endif 



// CLASSIFICATION: UNCLASSIFIED
