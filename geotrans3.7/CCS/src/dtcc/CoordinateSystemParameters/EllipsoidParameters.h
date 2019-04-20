// CLASSIFICATION: UNCLASSIFIED

#ifndef EllipsoidParameters_H
#define EllipsoidParameters_H

#include "DtccApi.h"


namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API EllipsoidParameters
    {
    public:

      EllipsoidParameters();
      EllipsoidParameters(
         double      __semiMajorAxis,
         double      __flattening,
         const char* __ellipsoidCode );
      EllipsoidParameters( const EllipsoidParameters& ecp );

      ~EllipsoidParameters();

      EllipsoidParameters& operator=( const EllipsoidParameters &ecp );

      void setSemiMajorAxis( double __semiMajorAxis );
      void setFlattening( double __flattening );
      void setEllipsoidCode( char __ellipsoidCode[4] );

      double semiMajorAxis() const;
      double flattening() const;
      char* ellipsoidCode();

    private:

      double _semiMajorAxis;
      double _flattening;
      char _ellipsoidCode[4];

    };
  }
}
  
#endif


// CLASSIFICATION: UNCLASSIFIED
