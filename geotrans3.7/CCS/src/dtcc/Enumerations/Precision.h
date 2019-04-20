// CLASSIFICATION: UNCLASSIFIED

#ifndef Precision_H
#define Precision_H


namespace MSP
{
  namespace CCS
  {
    class Precision
    {
    public:

      enum Enum
      {
        degree,
        tenMinute,
        minute,
        tenSecond,
        second,
        tenthOfSecond,
        hundrethOfSecond,
        thousandthOfSecond,
        tenThousandthOfSecond
      };

       static Enum toPrecision( int prec )
       {
          Enum val = tenthOfSecond;

          if( prec == degree )
             val = degree;
          else if( prec == tenMinute )
             val = tenMinute;
          else if( prec == minute )
             val = minute;
          else if( prec == tenSecond )
             val = tenSecond;
          else if( prec == second)
             val = second;
          else if( prec == tenthOfSecond)
             val = tenthOfSecond;
          else if( prec == hundrethOfSecond)
             val = hundrethOfSecond;
          else if( prec == thousandthOfSecond)
             val = thousandthOfSecond;
          else if( prec == tenThousandthOfSecond)
             val = tenThousandthOfSecond;

          return val;
       }

       static double toMeters( const Enum &prec )
       {
          double val = 0.0;
          if( prec == degree )
             val = 100000.0;
          else if( prec == tenMinute )
             val = 10000.0;
          else if( prec == minute )
             val = 1000.0;
          else if( prec == tenSecond )
             val = 100.0;
          else if( prec == second)
             val = 10.0;
          else if( prec == tenthOfSecond)
             val = 1.0;
          else if( prec == hundrethOfSecond)
             val = 0.1;
          else if( prec == thousandthOfSecond)
             val = 0.01;
          else if( prec == tenThousandthOfSecond)
             val = 0.001;

          return val;
       }
    };
  }
}

#endif 


// CLASSIFICATION: UNCLASSIFIED
