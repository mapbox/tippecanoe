// CLASSIFICATION: UNCLASSIFIED

#ifndef PolarStereographicStandardParallelParameters_H
#define PolarStereographicStandardParallelParameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"

namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API PolarStereographicStandardParallelParameters : public CoordinateSystemParameters
    {
    public:

      PolarStereographicStandardParallelParameters();
      PolarStereographicStandardParallelParameters( CoordinateType::Enum _coordinateType );
      PolarStereographicStandardParallelParameters( CoordinateType::Enum _coordinateType, double __longitudeDownFromPole, double __latitudeOfTrueScale, double __falseEasting, double __falseNorthing );
      PolarStereographicStandardParallelParameters( const PolarStereographicStandardParallelParameters& p );

      ~PolarStereographicStandardParallelParameters();

      PolarStereographicStandardParallelParameters& operator=( const PolarStereographicStandardParallelParameters &p );

      void setCentralMeridian( double __centralMeridian );
      void setStandardParallel( double __standardParallel );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );

      double centralMeridian() const;
      double standardParallel() const;
      double falseEasting() const;
      double falseNorthing() const;

    private:

      double _centralMeridian;
      double _standardParallel;
      double _falseEasting;
      double _falseNorthing;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
