// CLASSIFICATION: UNCLASSIFIED

#ifndef Datum_H
#define Datum_H

#include "DtccApi.h"
#include "DatumType.h"


namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API Datum
//    class Datum
    {
    public:

	    Datum();

	    Datum(
          long __index,
          const char* __code,
          const char* __ellipsoidCode,
          const char* __name,
          DatumType::Enum __datumType,
          double __deltaX,
          double __deltaY,
          double __deltaZ, 
          double __westLongitude,
          double __eastLongitude,
          double __southLatitude,
          double __northLatitude,
          bool __userDefined );

	    ~Datum( void );

      long index() const;

      char* code() const;

      char* ellipsoidCode() const;

      char* name() const;
  
      DatumType::Enum datumType() const;

      double deltaX() const;

      double deltaY() const;

      double deltaZ() const;

      double westLongitude() const;

      double eastLongitude() const;

      double southLatitude() const;

      double northLatitude() const;

      bool userDefined() const;


    private:

      long _index;
      char* _code;
      char* _ellipsoidCode;
      char* _name;
      DatumType::Enum _datumType;
      double _deltaX;
      double _deltaY;
      double _deltaZ;
      double _westLongitude;
      double _eastLongitude;
      double _southLatitude;
      double _northLatitude;
      bool _userDefined;

    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
