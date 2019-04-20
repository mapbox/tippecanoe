// CLASSIFICATION: UNCLASSIFIED

#ifndef MapProjection6Parameters_H
#define MapProjection6Parameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API MapProjection6Parameters : public CoordinateSystemParameters
    {
    public:

      MapProjection6Parameters();
      MapProjection6Parameters( CoordinateType::Enum _coordinateType );
      MapProjection6Parameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __standardParallel1, double __standardParallel2, double __falseEasting, double __falseNorthing );
      MapProjection6Parameters( const MapProjection6Parameters& p );

      ~MapProjection6Parameters();

      MapProjection6Parameters& operator=( const MapProjection6Parameters &p );

      void setCentralMeridian( double __centralMeridian );
      void setOriginLatitude( double __originLatitude );
      void setStandardParallel1( double __standardParallel1 );
      void setStandardParallel2( double __standardParallel2 );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );

      double centralMeridian() const;
      double originLatitude() const;
      double standardParallel1() const;
      double standardParallel2() const;
      double falseEasting() const;
      double falseNorthing() const;

    private:

      double _centralMeridian;
      double _originLatitude;
      double _standardParallel1;
      double _standardParallel2;
      double _falseEasting;
      double _falseNorthing;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
