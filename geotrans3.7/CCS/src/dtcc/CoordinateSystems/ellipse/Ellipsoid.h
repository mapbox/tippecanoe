// CLASSIFICATION: UNCLASSIFIED

#ifndef Ellipsoid_H
#define Ellipsoid_H

#include "DtccApi.h"

namespace MSP
{
  namespace CCS
  {
    class MSP_DTCC_API Ellipsoid
    {
    public:

	    Ellipsoid();

	    Ellipsoid( long __index, char* __code,  char* __name, double __semiMajorAxis, double __semiMinorAxis, double __flattening, double __eccentricitySquared, bool __userDefined );

	    ~Ellipsoid( void );

      long index() const;

      char* code() const;

      char* name() const;

      double semiMajorAxis() const;

      double semiMinorAxis() const;

      double flattening() const;

      double eccentricitySquared() const;

      bool userDefined() const;


    private:

      long _index;
      char* _code;
      char* _name;
      double _semiMajorAxis;
      double _semiMinorAxis;
      double _flattening;
      double _eccentricitySquared;
      bool _userDefined;

    };
  }
}
  

#endif // CLASSIFICATION: UNCLASSIFIED
