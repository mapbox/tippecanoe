// CLASSIFICATION: UNCLASSIFIED

#ifndef ObliqueMercatorParameters_H
#define ObliqueMercatorParameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API ObliqueMercatorParameters : public CoordinateSystemParameters
    {
    public:

      ObliqueMercatorParameters();
      ObliqueMercatorParameters( CoordinateType::Enum _coordinateType );
      ObliqueMercatorParameters( CoordinateType::Enum _coordinateType, double __originLatitude, double __longitude1, double __latitude1, double __longitude2, double __latitude2, double __falseEasting, double __falseNorthing, double __scaleFactor );
      ObliqueMercatorParameters( const ObliqueMercatorParameters& p );

      ~ObliqueMercatorParameters();

      ObliqueMercatorParameters& operator=( const ObliqueMercatorParameters &p );

      void setOriginLatitude( double __originLatitude );
      void setLongitude1( double __longitude1 );
      void setLatitude1( double __latitude1 );
      void setLongitude2( double __longitude2 );
      void setLatitude2( double __latitude2 );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );
      void setScaleFactor( double __scaleFactor );

      double originLatitude() const;
      double longitude1() const;
      double latitude1() const;
      double longitude2() const;
      double latitude2() const;
      double falseEasting() const;
      double falseNorthing() const;
      double scaleFactor() const;

    private:

      double _originLatitude;
      double _longitude1;
      double _latitude1;
      double _longitude2;
      double _latitude2;
      double _falseEasting;
      double _falseNorthing;
      double _scaleFactor;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
