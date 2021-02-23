// CLASSIFICATION: UNCLASSIFIED

#ifndef MapProjection4Parameters_H
#define MapProjection4Parameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API MapProjection4Parameters : public CoordinateSystemParameters
    {
    public:

      MapProjection4Parameters();
      MapProjection4Parameters( CoordinateType::Enum _coordinateType );
      MapProjection4Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __falseEasting, double __falseNorthing );
      MapProjection4Parameters( const MapProjection4Parameters& p );

      ~MapProjection4Parameters();

      MapProjection4Parameters& operator=( const MapProjection4Parameters &p );

      void setCentralMeridian( double __centralMeridian );
      void setOriginLatitude( double __originLatitude );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );

      double centralMeridian() const;
      double originLatitude() const;
      double falseEasting() const;
      double falseNorthing() const;

    private:

      double _centralMeridian;
      double _originLatitude;
      double _falseEasting;
      double _falseNorthing;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
