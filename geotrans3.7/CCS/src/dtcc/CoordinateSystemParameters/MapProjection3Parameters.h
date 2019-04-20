// CLASSIFICATION: UNCLASSIFIED

#ifndef MapProjection3Parameters_H
#define MapProjection3Parameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API MapProjection3Parameters : public CoordinateSystemParameters
    {
    public:

      MapProjection3Parameters();
      MapProjection3Parameters( CoordinateType::Enum _coordinateType );
      MapProjection3Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __falseEasting, double __falseNorthing );
      MapProjection3Parameters( const MapProjection3Parameters& p );

      ~MapProjection3Parameters();

      MapProjection3Parameters& operator=( const MapProjection3Parameters &p );

      void setCentralMeridian( double __centralMeridian );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );

      double centralMeridian() const;
      double falseEasting() const;
      double falseNorthing() const;

    private:

      double _centralMeridian;
      double _falseEasting;
      double _falseNorthing;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
