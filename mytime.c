// MyTime Functions
// ======================
// (c) 2022 jsk@yesco.org
//

#include <time.h>

char* isotime() {
  // from: jml project
  static char iso[sizeof "2011-10-08T07:07:09Z"];
  time_t now;
  time(&now);
  strftime(iso, sizeof iso, "%FT%TZ", gmtime(&now));
  return iso;
}

long timems() {
  return clock()*1000/CLOCKS_PER_SEC;
  // really unreliable?
  //struct timespec t;
  //clock_gettime(CLOCK_MONOTONIC, &t);
  //return t.tv_sec*1000 + t.tv_nsec/1000;
}
