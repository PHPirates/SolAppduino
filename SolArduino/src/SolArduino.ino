#include <EtherCard.h>      // https://github.com/jcw/ethercard
#include <Time.h>           // http://www.arduino.cc/playground/Code/Time
#include <Timezone.h>       // https://github.com/JChristensen/Timezone

static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x30,0x31 };
static long daySecs = 60 * 60 * 24; //used for sun calculations
const char poolNTP[] PROGMEM = "0.pool.ntp.org"; //pool to get time server from
uint8_t ntpMyPort = 123; //port for the time server, TODO why is this needed?

byte Ethernet::buffer[700];

double rad = PI / 180;
double e = rad * 23.4397;

double days;
double position[2];
double locationLatitude = 51.546825;
double locationLongitude = 4.412033;

// TimeZone : GMT+1. Helpful for getting correct current time
TimeChangeRule summerTime = {"UTC+1", Last, Sun, Mar, 2, +120};
TimeChangeRule winterTime = {"UTC+2", Last, Sun, Oct, 3, +60};
Timezone timeZone(summerTime, winterTime);

void setup () {
  Serial.begin(9600);
  // Serial.println(F("\n[testDHCP]"));
  //
  // Serial.print("MAC: ");
  // for (byte i = 0; i < 6; ++i) {
  //   Serial.print(mymac[i], HEX);
  //   if (i < 5)
  //     Serial.print(':');
  // }
  // Serial.println();
  //
  // if (ether.begin(sizeof Ethernet::buffer, mymac, 10) == 0)
  //   Serial.println(F("Failed to access Ethernet controller"));
  //
  // Serial.println(F("Setting up DHCP"));
  // if (!ether.dhcpSetup())
  //   Serial.println(F("DHCP failed"));
  //
  // ether.printIp("My IP: ", ether.myip);
  // ether.printIp("Netmask: ", ether.netmask);
  // ether.printIp("GW IP: ", ether.gwip);
  // ether.printIp("DNS IP: ", ether.dnsip);
  //
  // //Find ip address of a time server from the pool
  // if (!ether.dnsLookup(poolNTP)) {
  //   Serial.println("DNS failed");
  // }
  // ether.printIp("Lookup IP   : ", ether.hisip);
  //
  // //sync arduino clock, current time in seconds can be found with now();
  // setTime(getNtpTime());

  long times[2];
  getTimes(&times[0],locationLatitude,locationLongitude);
  Serial.println(times[0]); //should print 6029649
  Serial.println(times[1]); //6030335
}

void loop () {
  // Serial.print("time: ");
  // Serial.println(now());
  // Serial.println("azimuth, altitude");
  // getSunPosition(&position[0], locationLatitude, locationLongitude);
}

void getSunPosition(double *position, double locationLatitude,
  double locationLongitude) { // previously getSunAltitude()

    double lw = rad * -locationLongitude; // what is lw?
    double phi = rad * locationLatitude;
    //int days = secondsToDays(); //days since 2000, -0.5 or something
    days = 6030.036844594906;

    double sun[2];
    sun[0] = days;
    sunCoords(&sun[0]);

    double h = siderealTime(days, lw) - sun[1]; // what is h?
    position[0] = azimuth(h, phi, sun[0]);
    position[1] = altitude(h, phi, sun[0]);

    for (size_t i = 0; i < 2; i++) {
      Serial.print(position[i]);
      Serial.println(" rad");
    }

    for (size_t i = 0; i < 2; i++) {
      position[i] = position[i] * 180 / PI;
      Serial.print(position[i]);
      Serial.println(" degree");
    }
}


double secondsToDays(long seconds) {
  return ((double)((seconds) / (36*2.4) - 500 - 10957000))/1000.0;
 //take care to not overflow, and avoid integer division when dividing by 100 again
}

unsigned long getNtpTime() {
  unsigned long timeFromNTP;
  const unsigned long seventy_years = 2208988800UL;
  ether.ntpRequest(ether.hisip, ntpMyPort);
  while(true) {
    word length = ether.packetReceive();
    ether.packetLoop(length);
    if(length > 0 && ether.ntpProcessAnswer(&timeFromNTP, ntpMyPort)) {
      // Serial.print("Time from NTP: ");
      // Serial.println(timeFromNTP);
      return timeZone.toLocal(timeFromNTP - seventy_years);
    }
  }
  return 0;
}

void sunCoords(double *sun) {
  sun[0] = 1;
  sun[1] = 3;

  double mean = solarMeanAnomaly(days);
  double longitude = eclipticLongitude(mean);

  sun[0] = declination(longitude, 0);
  sun[1] = rightAscension(longitude, 0);
}

double solarMeanAnomaly(double days) {
  return rad * (357.5291 + 0.98560028 * days);
}

double eclipticLongitude(double mean) {
  double center = rad * (sin(mean) + 0.02 * sin(2 * mean) + 0.0003 * sin(3 * mean));
  double perihelion = rad * 102.9372;

  return mean + center + perihelion + PI;
}

double declination(double longitude, double b) {
  return asin(sin(b) * cos(e) + cos(b) * sin(e) * sin(longitude));
}

double rightAscension(double longitude, double b) {
  return atan2(sin(longitude) * cos(e) - tan(b) * sin(e), cos(longitude));
}

double siderealTime(int days, double lw) {
  return rad * (280.16 + 360.9856235 * days) - lw;
}

double azimuth(double h, double phi, double sun) {
  return PI + atan2(sin(h), cos(h) * sin(phi) - tan(sun) * cos(phi));
}

double altitude(double h, double phi, double sun) {
  return asin(sin(phi) * sin(sun) + cos(phi) * cos(sun) * cos(h));
}

/*returns an array with first today's sunrise in days,
then the hoursOfDay of the sunset, and then minutes.*/
void getTimes(long *times, double locationLatitude,
  double locationLongitude) {

    double lw = rad * -locationLongitude; // what is lw?
    double phi = rad * locationLatitude;
    //long d = secondsToDays(); //days since epoch*1000, gets current time in seconds
    //TODO convert long to double
    double d = 6030.037; //TODO debug magik
    int n = julianCycle(d,lw);
    double ds = approxTransit(0,lw,n); //n=6030?
    double M = solarMeanAnomaly(ds);
    double L = eclipticLongitude(M);
    double dec = declination(L, 0); //uses asin
    double Jnoon = solarTransitJ(ds, M, L);

    //result
    double h = -0.833 * rad;
    Serial.println(h); //-0.0145
    Serial.println(lw); //-0.08
    Serial.println(phi); //0.90
    Serial.println(dec); //0.30 //uses asin
    Serial.println(n); //6030
    Serial.println(M); //109.97
    Serial.println(L); //114.90
    Serial.println();
    unsigned long Jset = getSetJ(h, lw, phi, dec, n, M, L);
    unsigned long Jrise = Jnoon - (Jset - Jnoon);
    //10 is about 15mins, 1 about 1.5 mins (accurate enough)

    //convert to seconds since epoch
    times[0] = (long) secondsToDays(fromJulian(Jrise))*1000;
    times[1] = (long) secondsToDays(fromJulian(Jset))*1000;

}

double julianCycle(double d, double lw) {
  //first adding half and then casting to int is equal to rounding
  return (int)((d - 0.0009 - ( lw / (2 * PI) ) ) + 0.5);
}

double approxTransit(double Ht, double lw, double n) {
  return 0.0009 + (Ht + lw) / (2 * PI) + n;
}

unsigned long solarTransitJ(double ds, double M, double L) {
	//2451545 + 6030.332 was a problem, result had too much digits for double to be precise...
	//fix is to do everything times 1000 and cast to long and eventually unsigned long
	//which is just long enough to contain for example 2457575335
	//unsigned long + 6030.332*1000 to long + round (sth with sin,cos)*1000
	return 2451545000 + (long) (ds * 1000)  + (int) ( ( 0.0053 * sin(M) - 0.0069 * sin(2 * L) ) * 1000 + 0.5 );
}

//warning! returns time in seconds, contrary to js
long fromJulian(double j) {
  return (j + 500 - 2440588000) * 36 * 2.4;
  //*24 and then /10 doesn't work.
}

unsigned long getSetJ(double h, double lw, double phi, double dec,
  double n, double M, double L) {
  //double w = hourAngle(h, phi, dec);
  double w = 2.158; //TODO debug magik
  double a = approxTransit(w, lw, n);
  return solarTransitJ(a, M, L); //should be a long to retain enough precision
}

double hourAngle(double h, double phi, double d) {
  return acos((sin(h) - sin (phi) * sin (d)) / (cos(phi) * cos(d)));
}
