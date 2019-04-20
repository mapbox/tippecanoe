// CLASSIFICATION: UNCLASSIFIED

#ifndef NeysParameters_H
#define NeysParameters_H

#include "CoordinateSystemParameters.h"
#include "DtccApi.h"



namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API NeysParameters : public CoordinateSystemParameters
    {
    public:

      NeysParameters();
      NeysParameters( CoordinateType::Enum _coordinateType );
      NeysParameters( CoordinateType::Enum _coordinateType, double __centralMeridian, double __originLatitude, double __standardParallel1, double __falseEasting, double __falseNorthing );
      NeysParameters( const NeysParameters& p );

      ~NeysParameters();

      NeysParameters& operator=( const NeysParameters &p );

      void setCentralMeridian( double __centralMeridian );
      void setOriginLatitude( double __originLatitude );
      void setStandardParallel1( double __standardParallel1 );
      void setFalseEasting( double __falseEasting );
      void setFalseNorthing( double __falseNorthing );

      double centralMeridian() const;
      double originLatitude() const;
      double standardParallel1() const;
      double falseEasting() const;
      double falseNorthing() const;

    private:

      double _centralMeridian;
      double _originLatitude;
      double _standardParallel1;
      double _falseEasting;
      double _falseNorthing;

    };
  }
}
	
#endif 


// CLASSIFICATION: UNCLASSIFIED
