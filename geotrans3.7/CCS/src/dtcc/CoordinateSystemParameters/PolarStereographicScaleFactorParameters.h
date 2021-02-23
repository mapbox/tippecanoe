// CLASSIFICATION: UNCLASSIFIED

#ifndef PolarStereographicScaleFactorParameters_H
#define PolarStereographicScaleFactorParameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"

namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API PolarStereographicScaleFactorParameters : public CoordinateSystemParameters
    {
    public:

      PolarStereographicScaleFactorParameters();
      PolarStereographicScaleFactorParameters( CoordinateType::Enum _coordinateType );
      PolarStereographicScaleFactorParameters( CoordinateType::Enum _coordinateType, double __longitudeDownFromPole, double __scaleFactor, char __hemisphere, double __falseEasting, double __falseNorthing );
      PolarStereographicScaleFactorParameters( const PolarStereographicScaleFactorParameters& p );

      ~PolarStereographicScaleFactorParameters();

      PolarStereographicScaleFactorParameters& operator=( const PolarStereographicScaleFactorParameters &p );

      void setCentralMeridian( double __centralMeridian );
      void setScaleFactor( double __scaleFactor );
      void setHemisphere( char _hemisphere );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );

      double centralMeridian() const;
      double scaleFactor() const;
      char hemisphere() const;
      double falseEasting() const;
      double falseNorthing() const;

    private:

      double _centralMeridian;
      double _scaleFactor;
	    char _hemisphere;
      double _falseEasting;
      double _falseNorthing;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
