#include "Utils.h"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define C360 360.0000000000000000000

#define PI 3.14159265358979323846

double CalGPSDistance(double latitud1, double longitud1, double latitud2, double longitud2){
    double haversine;
    double temp;
    double distancia_puntos;

    latitud1  = latitud1  * GRADOS_RADIANES;
    longitud1 = longitud1 * GRADOS_RADIANES;
    latitud2  = latitud2  * GRADOS_RADIANES;
    longitud2 = longitud2 * GRADOS_RADIANES;

    haversine = (pow(sin((1.0 / 2) * (latitud2 - latitud1)), 2)) + ((cos(latitud1)) * (cos(latitud2)) * (pow(sin((1.0 / 2) * (longitud2 - longitud1)), 2)));
    temp = 2 * asin(MIN(1.0, sqrt(haversine)));
    distancia_puntos = RADIO_TERRESTRE * temp;

   return distancia_puntos;
}

double CalDifangdeg(double x, double y)
{
  double arg;
  arg = fmod(y - x, C360);
  if (arg < 0)
    arg = arg + C360;
  if (arg > 180)
    arg = arg - C360;
  return (-arg);
}

double CalPerpendicularHeight(double distBtnCorners, double b, double c){

    double s = (distBtnCorners+b+c)/2;
    double area = sqrt(s*(s-distBtnCorners)*(s-b)*(s-c));
    
    return 2*area/distBtnCorners;

}

double rad2deg(double rad) {
  return (rad * 180 / PI);
}

double radians(double deg) {
  return (deg * PI / 180);
}

double CalBearing(double lat,double lon,double lat2,double lon2){


    double teta1 = radians(lat);
    double teta2 = radians(lat2);
    // double delta1 = radians(lat2-lat);
    double delta2 = radians(lon2-lon);

    //==================Heading Formula Calculation================//

    double y = sin(delta2) * cos(teta2);
    double x = cos(teta1)*sin(teta2) - sin(teta1)*cos(teta2)*cos(delta2);
    double brng = atan2(y,x);
    brng = rad2deg(brng);// radians to degrees
    brng = ( ((int)brng + 360) % 360 ); 

    //Serial.print("Heading GPS: ");
    //Serial.println(brng);

    return brng;


  }

