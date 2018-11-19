/*
** time.c - Time class
**
** See Copyright Notice in mruby.h
*/

#include <math.h>
#include <time.h>
#include <mruby.h>
#include <mruby/class.h>
#include <mruby/data.h>

#ifndef $DISABLE_STDIO
#include <stdio.h>
#else
#include <string.h>
#endif

#define NDIV(x,y) (-(-((x)+1)/(y))-1)

#if defined(_MSC_VER) && _MSC_VER < 1800
double round(double x) {
  return floor(x + 0.5);
}
#endif

#if !defined(__MINGW64__) && defined(_WIN32)
# define llround(x) round(x)
#endif

#if defined(__MINGW64__) || defined(__MINGW32__)
# include <sys/time.h>
#endif

/** Time class configuration */

/* gettimeofday(2) */
/* C99 does not have gettimeofday that is required to retrieve microseconds */
/* uncomment following macro on platforms without gettimeofday(2) */
/* #define NO_GETTIMEOFDAY */

/* gmtime(3) */
/* C99 does not have reentrant gmtime_r() so it might cause troubles under */
/* multi-threading environment.  undef following macro on platforms that */
/* does not have gmtime_r() and localtime_r(). */
/* #define NO_GMTIME_R */

#ifdef _WIN32
#if _MSC_VER
/* Win32 platform do not provide gmtime_r/localtime_r; emulate them using gmtime_s/localtime_s */
#define gmtime_r(tp, tm)    ((gmtime_s((tm), (tp)) == 0) ? (tm) : NULL)
#define localtime_r(tp, tm)    ((localtime_s((tm), (tp)) == 0) ? (tm) : NULL)
#else
#define NO_GMTIME_R
#endif
#endif

/* asctime(3) */
/* mruby usually use its own implementation of struct tm to string conversion */
/* except when DISABLE_STDIO is set. In that case, it uses asctime() or asctime_r(). */
/* By default mruby tries to use asctime_r() which is reentrant. */
/* Undef following macro on platforms that does not have asctime_r(). */
/* #define NO_ASCTIME_R */

/* timegm(3) */
/* mktime() creates tm structure for localtime; timegm() is for UTC time */
/* define following macro to use probably faster timegm() on the platform */
/* #define USE_SYSTEM_TIMEGM */

/* time_t */
/* If your platform supports time_t as uint (e.g. uint32_t, uint64_t), */
/* uncomment following macro. */
/* #define $TIME_T_UINT */

/** end of Time class configuration */

#ifndef NO_GETTIMEOFDAY
# ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN  /* don't include winsock.h */
#  include <windows.h>
#  define gettimeofday my_gettimeofday

#  ifdef _MSC_VER
#    define UI64(x) x##ui64
#  else
#    define UI64(x) x##ull
#  endif

typedef long suseconds_t;

# if (!defined __MINGW64__) && (!defined __MINGW32__)
struct timeval {
  time_t tv_sec;
  suseconds_t tv_usec;
};
# endif

static int
gettimeofday(struct timeval *tv, void *tz)
{
  if (tz) {
    $assert(0);  /* timezone is not supported */
  }
  if (tv) {
    union {
      FILETIME ft;
      unsigned __int64 u64;
    } t;
    GetSystemTimeAsFileTime(&t.ft);   /* 100 ns intervals since Windows epoch */
    t.u64 -= UI64(116444736000000000);  /* Unix epoch bias */
    t.u64 /= 10;                      /* to microseconds */
    tv->tv_sec = (time_t)(t.u64 / (1000 * 1000));
    tv->tv_usec = t.u64 % (1000 * 1000);
  }
  return 0;
}
# else
#  include <sys/time.h>
# endif
#endif
#ifdef NO_GMTIME_R
#define gmtime_r(t,r) gmtime(t)
#define localtime_r(t,r) localtime(t)
#endif

#ifndef USE_SYSTEM_TIMEGM
#define timegm my_timgm

static unsigned int
is_leapyear(unsigned int y)
{
  return (y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0);
}

static time_t
timegm(struct tm *tm)
{
  static const unsigned int ndays[2][12] = {
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
  };
  time_t r = 0;
  int i;
  unsigned int *nday = (unsigned int*) ndays[is_leapyear(tm->tm_year+1900)];

  static const int epoch_year = 70;
  if(tm->tm_year >= epoch_year) {
    for (i = epoch_year; i < tm->tm_year; ++i)
      r += is_leapyear(i+1900) ? 366*24*60*60 : 365*24*60*60;
  } else {
    for (i = tm->tm_year; i < epoch_year; ++i)
      r -= is_leapyear(i+1900) ? 366*24*60*60 : 365*24*60*60;
  }
  for (i = 0; i < tm->tm_mon; ++i)
    r += nday[i] * 24 * 60 * 60;
  r += (tm->tm_mday - 1) * 24 * 60 * 60;
  r += tm->tm_hour * 60 * 60;
  r += tm->tm_min * 60;
  r += tm->tm_sec;
  return r;
}
#endif

/* Since we are limited to using ISO C99, this implementation is based
* on time_t. That means the resolution of time is only precise to the
* second level. Also, there are only 2 timezones, namely UTC and LOCAL.
*/

enum $timezone {
  $TIMEZONE_NONE   = 0,
  $TIMEZONE_UTC    = 1,
  $TIMEZONE_LOCAL  = 2,
  $TIMEZONE_LAST   = 3
};

typedef struct $timezone_name {
  const char name[8];
  size_t len;
} $timezone_name;

static const $timezone_name timezone_names[] = {
  { "none", sizeof("none") - 1 },
  { "UTC", sizeof("UTC") - 1 },
  { "LOCAL", sizeof("LOCAL") - 1 },
};

#ifndef $DISABLE_STDIO
static const char mon_names[12][4] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static const char wday_names[7][4] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};
#endif

struct $time {
  time_t              sec;
  time_t              usec;
  enum $timezone   timezone;
  struct tm           datetime;
};

static const struct $data_type $time_type = { "Time", $free };

/** Updates the datetime of a $time based on it's timezone and
seconds setting. Returns self on success, NULL of failure. */
static struct $time*
time_update_datetime($state *mrb, struct $time *self)
{
  struct tm *aid;

  if (self->timezone == $TIMEZONE_UTC) {
    aid = gmtime_r(&self->sec, &self->datetime);
  }
  else {
    aid = localtime_r(&self->sec, &self->datetime);
  }
  if (!aid) {
    $raisef(mrb, E_ARGUMENT_ERROR, "%S out of Time range", $float_value(mrb, ($float)self->sec));
    /* not reached */
    return NULL;
  }
#ifdef NO_GMTIME_R
  self->datetime = *aid; /* copy data */
#endif

  return self;
}

static $value
$time_wrap($state *mrb, struct RClass *tc, struct $time *tm)
{
  return $obj_value(Data_Wrap_Struct(mrb, tc, &$time_type, tm));
}

void $check_num_exact($state *mrb, $float num);

/* Allocates a $time object and initializes it. */
static struct $time*
time_alloc($state *mrb, double sec, double usec, enum $timezone timezone)
{
  struct $time *tm;
  time_t tsec = 0;

  $check_num_exact(mrb, ($float)sec);
  $check_num_exact(mrb, ($float)usec);
#ifndef $TIME_T_UINT
  if (sizeof(time_t) == 4 && (sec > (double)INT32_MAX || (double)INT32_MIN > sec)) {
    goto out_of_range;
  }
  if (sizeof(time_t) == 8 && (sec > (double)INT64_MAX || (double)INT64_MIN > sec)) {
    goto out_of_range;
  }
#else
  if (sizeof(time_t) == 4 && (sec > (double)UINT32_MAX || (double)0 > sec)) {
    goto out_of_range;
  }
  if (sizeof(time_t) == 8 && (sec > (double)UINT64_MAX || (double)0 > sec)) {
    goto out_of_range;
  }
#endif
  tsec  = (time_t)sec;
  if ((sec > 0 && tsec < 0) || (sec < 0 && (double)tsec > sec)) {
  out_of_range:
    $raisef(mrb, E_ARGUMENT_ERROR, "%S out of Time range", $float_value(mrb, sec));
  }
  tm = (struct $time *)$malloc(mrb, sizeof(struct $time));
  tm->sec  = tsec;
  tm->usec = (time_t)llround((sec - tm->sec) * 1.0e6 + usec);
  if (tm->usec < 0) {
    long sec2 = (long)NDIV(usec,1000000); /* negative div */
    tm->usec -= sec2 * 1000000;
    tm->sec += sec2;
  }
  else if (tm->usec >= 1000000) {
    long sec2 = (long)(usec / 1000000);
    tm->usec -= sec2 * 1000000;
    tm->sec += sec2;
  }
  tm->timezone = timezone;
  time_update_datetime(mrb, tm);

  return tm;
}

static $value
$time_make($state *mrb, struct RClass *c, double sec, double usec, enum $timezone timezone)
{
  return $time_wrap(mrb, c, time_alloc(mrb, sec, usec, timezone));
}

static struct $time*
current_$time($state *mrb)
{
  struct $time *tm;

  tm = (struct $time *)$malloc(mrb, sizeof(*tm));
#if defined(TIME_UTC)
  {
    struct timespec ts;
    if (timespec_get(&ts, TIME_UTC) == 0) {
      $free(mrb, tm);
      $raise(mrb, E_RUNTIME_ERROR, "timespec_get() failed for unknown reasons");
    }
    tm->sec = ts.tv_sec;
    tm->usec = ts.tv_nsec / 1000;
  }
#elif defined(NO_GETTIMEOFDAY)
  {
    static time_t last_sec = 0, last_usec = 0;

    tm->sec  = time(NULL);
    if (tm->sec != last_sec) {
      last_sec = tm->sec;
      last_usec = 0;
    }
    else {
      /* add 1 usec to differentiate two times */
      last_usec += 1;
    }
    tm->usec = last_usec;
  }
#else
  {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    tm->sec = tv.tv_sec;
    tm->usec = tv.tv_usec;
  }
#endif
  tm->timezone = $TIMEZONE_LOCAL;
  time_update_datetime(mrb, tm);

  return tm;
}

/* Allocates a new Time object with given millis value. */
static $value
$time_now($state *mrb, $value self)
{
  return $time_wrap(mrb, $class_ptr(self), current_$time(mrb));
}

/* 15.2.19.6.1 */
/* Creates an instance of time at the given time in seconds, etc. */
static $value
$time_at($state *mrb, $value self)
{
  $float f, f2 = 0;

  $get_args(mrb, "f|f", &f, &f2);
  return $time_make(mrb, $class_ptr(self), f, f2, $TIMEZONE_LOCAL);
}

static struct $time*
time_mktime($state *mrb, $int ayear, $int amonth, $int aday,
  $int ahour, $int amin, $int asec, $int ausec,
  enum $timezone timezone)
{
  time_t nowsecs;
  struct tm nowtime = { 0 };

  nowtime.tm_year  = (int)ayear  - 1900;
  nowtime.tm_mon   = (int)amonth - 1;
  nowtime.tm_mday  = (int)aday;
  nowtime.tm_hour  = (int)ahour;
  nowtime.tm_min   = (int)amin;
  nowtime.tm_sec   = (int)asec;
  nowtime.tm_isdst = -1;

  if (nowtime.tm_mon  < 0 || nowtime.tm_mon  > 11
      || nowtime.tm_mday < 1 || nowtime.tm_mday > 31
      || nowtime.tm_hour < 0 || nowtime.tm_hour > 24
      || (nowtime.tm_hour == 24 && (nowtime.tm_min > 0 || nowtime.tm_sec > 0))
      || nowtime.tm_min  < 0 || nowtime.tm_min  > 59
      || nowtime.tm_sec  < 0 || nowtime.tm_sec  > 60)
    $raise(mrb, E_RUNTIME_ERROR, "argument out of range");

  if (timezone == $TIMEZONE_UTC) {
    nowsecs = timegm(&nowtime);
  }
  else {
    nowsecs = mktime(&nowtime);
  }
  if (nowsecs == (time_t)-1) {
    $raise(mrb, E_ARGUMENT_ERROR, "Not a valid time.");
  }

  return time_alloc(mrb, (double)nowsecs, (double)ausec, timezone);
}

/* 15.2.19.6.2 */
/* Creates an instance of time at the given time in UTC. */
static $value
$time_gm($state *mrb, $value self)
{
  $int ayear = 0, amonth = 1, aday = 1, ahour = 0, amin = 0, asec = 0, ausec = 0;

  $get_args(mrb, "i|iiiiii",
                &ayear, &amonth, &aday, &ahour, &amin, &asec, &ausec);
  return $time_wrap(mrb, $class_ptr(self),
          time_mktime(mrb, ayear, amonth, aday, ahour, amin, asec, ausec, $TIMEZONE_UTC));
}


/* 15.2.19.6.3 */
/* Creates an instance of time at the given time in local time zone. */
static $value
$time_local($state *mrb, $value self)
{
  $int ayear = 0, amonth = 1, aday = 1, ahour = 0, amin = 0, asec = 0, ausec = 0;

  $get_args(mrb, "i|iiiiii",
                &ayear, &amonth, &aday, &ahour, &amin, &asec, &ausec);
  return $time_wrap(mrb, $class_ptr(self),
          time_mktime(mrb, ayear, amonth, aday, ahour, amin, asec, ausec, $TIMEZONE_LOCAL));
}

static struct $time*
time_get_ptr($state *mrb, $value time)
{
  struct $time *tm;

  tm = DATA_GET_PTR(mrb, time, &$time_type, struct $time);
  if (!tm) {
    $raise(mrb, E_ARGUMENT_ERROR, "uninitialized time");
  }
  return tm;
}

static $value
$time_eq($state *mrb, $value self)
{
  $value other;
  struct $time *tm1, *tm2;
  $bool eq_p;

  $get_args(mrb, "o", &other);
  tm1 = DATA_GET_PTR(mrb, self, &$time_type, struct $time);
  tm2 = DATA_CHECK_GET_PTR(mrb, other, &$time_type, struct $time);
  eq_p = tm1 && tm2 && tm1->sec == tm2->sec && tm1->usec == tm2->usec;

  return $bool_value(eq_p);
}

static $value
$time_cmp($state *mrb, $value self)
{
  $value other;
  struct $time *tm1, *tm2;

  $get_args(mrb, "o", &other);
  tm1 = DATA_GET_PTR(mrb, self, &$time_type, struct $time);
  tm2 = DATA_CHECK_GET_PTR(mrb, other, &$time_type, struct $time);
  if (!tm1 || !tm2) return $nil_value();
  if (tm1->sec > tm2->sec) {
    return $fixnum_value(1);
  }
  else if (tm1->sec < tm2->sec) {
    return $fixnum_value(-1);
  }
  /* tm1->sec == tm2->sec */
  if (tm1->usec > tm2->usec) {
    return $fixnum_value(1);
  }
  else if (tm1->usec < tm2->usec) {
    return $fixnum_value(-1);
  }
  return $fixnum_value(0);
}

static $value
$time_plus($state *mrb, $value self)
{
  $float f;
  struct $time *tm;

  $get_args(mrb, "f", &f);
  tm = time_get_ptr(mrb, self);
  return $time_make(mrb, $obj_class(mrb, self), (double)tm->sec+f, (double)tm->usec, tm->timezone);
}

static $value
$time_minus($state *mrb, $value self)
{
  $float f;
  $value other;
  struct $time *tm, *tm2;

  $get_args(mrb, "o", &other);
  tm = time_get_ptr(mrb, self);
  tm2 = DATA_CHECK_GET_PTR(mrb, other, &$time_type, struct $time);
  if (tm2) {
    f = ($float)(tm->sec - tm2->sec)
      + ($float)(tm->usec - tm2->usec) / 1.0e6;
    return $float_value(mrb, f);
  }
  else {
    $get_args(mrb, "f", &f);
    return $time_make(mrb, $obj_class(mrb, self), (double)tm->sec-f, (double)tm->usec, tm->timezone);
  }
}

/* 15.2.19.7.30 */
/* Returns week day number of time. */
static $value
$time_wday($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_wday);
}

/* 15.2.19.7.31 */
/* Returns year day number of time. */
static $value
$time_yday($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_yday + 1);
}

/* 15.2.19.7.32 */
/* Returns year of time. */
static $value
$time_year($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_year + 1900);
}

/* 15.2.19.7.33 */
/* Returns name of time's timezone. */
static $value
$time_zone($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  if (tm->timezone <= $TIMEZONE_NONE) return $nil_value();
  if (tm->timezone >= $TIMEZONE_LAST) return $nil_value();
  return $str_new_static(mrb,
                            timezone_names[tm->timezone].name,
                            timezone_names[tm->timezone].len);
}

/* 15.2.19.7.4 */
/* Returns a string that describes the time. */
static $value
$time_asctime($state *mrb, $value self)
{
  struct $time *tm = time_get_ptr(mrb, self);
  struct tm *d = &tm->datetime;
  int len;

#if defined($DISABLE_STDIO)
  char *s;
# ifdef NO_ASCTIME_R
  s = asctime(d);
# else
  char buf[32];
  s = asctime_r(d, buf);
# endif
  len = strlen(s)-1;            /* truncate the last newline */
#else
  char buf[256];

  len = snprintf(buf, sizeof(buf), "%s %s %02d %02d:%02d:%02d %s%d",
    wday_names[d->tm_wday], mon_names[d->tm_mon], d->tm_mday,
    d->tm_hour, d->tm_min, d->tm_sec,
    tm->timezone == $TIMEZONE_UTC ? "UTC " : "",
    d->tm_year + 1900);
#endif
  return $str_new(mrb, buf, len);
}

/* 15.2.19.7.6 */
/* Returns the day in the month of the time. */
static $value
$time_day($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_mday);
}


/* 15.2.19.7.7 */
/* Returns true if daylight saving was applied for this time. */
static $value
$time_dst_p($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $bool_value(tm->datetime.tm_isdst);
}

/* 15.2.19.7.8 */
/* 15.2.19.7.10 */
/* Returns the Time object of the UTC(GMT) timezone. */
static $value
$time_getutc($state *mrb, $value self)
{
  struct $time *tm, *tm2;

  tm = time_get_ptr(mrb, self);
  tm2 = (struct $time *)$malloc(mrb, sizeof(*tm));
  *tm2 = *tm;
  tm2->timezone = $TIMEZONE_UTC;
  time_update_datetime(mrb, tm2);
  return $time_wrap(mrb, $obj_class(mrb, self), tm2);
}

/* 15.2.19.7.9 */
/* Returns the Time object of the LOCAL timezone. */
static $value
$time_getlocal($state *mrb, $value self)
{
  struct $time *tm, *tm2;

  tm = time_get_ptr(mrb, self);
  tm2 = (struct $time *)$malloc(mrb, sizeof(*tm));
  *tm2 = *tm;
  tm2->timezone = $TIMEZONE_LOCAL;
  time_update_datetime(mrb, tm2);
  return $time_wrap(mrb, $obj_class(mrb, self), tm2);
}

/* 15.2.19.7.15 */
/* Returns hour of time. */
static $value
$time_hour($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_hour);
}

/* 15.2.19.7.16 */
/* Initializes a time by setting the amount of milliseconds since the epoch.*/
static $value
$time_initialize($state *mrb, $value self)
{
  $int ayear = 0, amonth = 1, aday = 1, ahour = 0,
  amin = 0, asec = 0, ausec = 0;
  $int n;
  struct $time *tm;

  n = $get_args(mrb, "|iiiiiii",
       &ayear, &amonth, &aday, &ahour, &amin, &asec, &ausec);
  tm = (struct $time*)DATA_PTR(self);
  if (tm) {
    $free(mrb, tm);
  }
  $data_init(self, NULL, &$time_type);

  if (n == 0) {
    tm = current_$time(mrb);
  }
  else {
    tm = time_mktime(mrb, ayear, amonth, aday, ahour, amin, asec, ausec, $TIMEZONE_LOCAL);
  }
  $data_init(self, tm, &$time_type);
  return self;
}

/* 15.2.19.7.17(x) */
/* Initializes a copy of this time object. */
static $value
$time_initialize_copy($state *mrb, $value copy)
{
  $value src;
  struct $time *t1, *t2;

  $get_args(mrb, "o", &src);
  if ($obj_equal(mrb, copy, src)) return copy;
  if (!$obj_is_instance_of(mrb, src, $obj_class(mrb, copy))) {
    $raise(mrb, E_TYPE_ERROR, "wrong argument class");
  }
  t1 = (struct $time *)DATA_PTR(copy);
  t2 = (struct $time *)DATA_PTR(src);
  if (!t2) {
    $raise(mrb, E_ARGUMENT_ERROR, "uninitialized time");
  }
  if (!t1) {
    t1 = (struct $time *)$malloc(mrb, sizeof(struct $time));
    $data_init(copy, t1, &$time_type);
  }
  *t1 = *t2;
  return copy;
}

/* 15.2.19.7.18 */
/* Sets the timezone attribute of the Time object to LOCAL. */
static $value
$time_localtime($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  tm->timezone = $TIMEZONE_LOCAL;
  time_update_datetime(mrb, tm);
  return self;
}

/* 15.2.19.7.19 */
/* Returns day of month of time. */
static $value
$time_mday($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_mday);
}

/* 15.2.19.7.20 */
/* Returns minutes of time. */
static $value
$time_min($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_min);
}

/* 15.2.19.7.21 and 15.2.19.7.22 */
/* Returns month of time. */
static $value
$time_mon($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_mon + 1);
}

/* 15.2.19.7.23 */
/* Returns seconds in minute of time. */
static $value
$time_sec($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $fixnum_value(tm->datetime.tm_sec);
}


/* 15.2.19.7.24 */
/* Returns a Float with the time since the epoch in seconds. */
static $value
$time_to_f($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $float_value(mrb, ($float)tm->sec + ($float)tm->usec/1.0e6);
}

/* 15.2.19.7.25 */
/* Returns a Fixnum with the time since the epoch in seconds. */
static $value
$time_to_i($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  if (tm->sec > $INT_MAX || tm->sec < $INT_MIN) {
    return $float_value(mrb, ($float)tm->sec);
  }
  return $fixnum_value(($int)tm->sec);
}

/* 15.2.19.7.26 */
/* Returns a Float with the time since the epoch in microseconds. */
static $value
$time_usec($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  if (tm->usec > $INT_MAX || tm->usec < $INT_MIN) {
    return $float_value(mrb, ($float)tm->usec);
  }
  return $fixnum_value(($int)tm->usec);
}

/* 15.2.19.7.27 */
/* Sets the timezone attribute of the Time object to UTC. */
static $value
$time_utc($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  tm->timezone = $TIMEZONE_UTC;
  time_update_datetime(mrb, tm);
  return self;
}

/* 15.2.19.7.28 */
/* Returns true if this time is in the UTC timezone false if not. */
static $value
$time_utc_p($state *mrb, $value self)
{
  struct $time *tm;

  tm = time_get_ptr(mrb, self);
  return $bool_value(tm->timezone == $TIMEZONE_UTC);
}


void
$mruby_time_gem_init($state* mrb)
{
  struct RClass *tc;
  /* ISO 15.2.19.2 */
  tc = $define_class(mrb, "Time", mrb->object_class);
  $SET_INSTANCE_TT(tc, $TT_DATA);
  $include_module(mrb, tc, $module_get(mrb, "Comparable"));
  $define_class_method(mrb, tc, "at", $time_at, $ARGS_ARG(1, 1));      /* 15.2.19.6.1 */
  $define_class_method(mrb, tc, "gm", $time_gm, $ARGS_ARG(1,6));       /* 15.2.19.6.2 */
  $define_class_method(mrb, tc, "local", $time_local, $ARGS_ARG(1,6)); /* 15.2.19.6.3 */
  $define_class_method(mrb, tc, "mktime", $time_local, $ARGS_ARG(1,6));/* 15.2.19.6.4 */
  $define_class_method(mrb, tc, "now", $time_now, $ARGS_NONE());       /* 15.2.19.6.5 */
  $define_class_method(mrb, tc, "utc", $time_gm, $ARGS_ARG(1,6));      /* 15.2.19.6.6 */

  $define_method(mrb, tc, "=="     , $time_eq     , $ARGS_REQ(1));
  $define_method(mrb, tc, "<=>"    , $time_cmp    , $ARGS_REQ(1)); /* 15.2.19.7.1 */
  $define_method(mrb, tc, "+"      , $time_plus   , $ARGS_REQ(1)); /* 15.2.19.7.2 */
  $define_method(mrb, tc, "-"      , $time_minus  , $ARGS_REQ(1)); /* 15.2.19.7.3 */
  $define_method(mrb, tc, "to_s"   , $time_asctime, $ARGS_NONE());
  $define_method(mrb, tc, "inspect", $time_asctime, $ARGS_NONE());
  $define_method(mrb, tc, "asctime", $time_asctime, $ARGS_NONE()); /* 15.2.19.7.4 */
  $define_method(mrb, tc, "ctime"  , $time_asctime, $ARGS_NONE()); /* 15.2.19.7.5 */
  $define_method(mrb, tc, "day"    , $time_day    , $ARGS_NONE()); /* 15.2.19.7.6 */
  $define_method(mrb, tc, "dst?"   , $time_dst_p  , $ARGS_NONE()); /* 15.2.19.7.7 */
  $define_method(mrb, tc, "getgm"  , $time_getutc , $ARGS_NONE()); /* 15.2.19.7.8 */
  $define_method(mrb, tc, "getlocal",$time_getlocal,$ARGS_NONE()); /* 15.2.19.7.9 */
  $define_method(mrb, tc, "getutc" , $time_getutc , $ARGS_NONE()); /* 15.2.19.7.10 */
  $define_method(mrb, tc, "gmt?"   , $time_utc_p  , $ARGS_NONE()); /* 15.2.19.7.11 */
  $define_method(mrb, tc, "gmtime" , $time_utc    , $ARGS_NONE()); /* 15.2.19.7.13 */
  $define_method(mrb, tc, "hour"   , $time_hour, $ARGS_NONE());    /* 15.2.19.7.15 */
  $define_method(mrb, tc, "localtime", $time_localtime, $ARGS_NONE()); /* 15.2.19.7.18 */
  $define_method(mrb, tc, "mday"   , $time_mday, $ARGS_NONE());    /* 15.2.19.7.19 */
  $define_method(mrb, tc, "min"    , $time_min, $ARGS_NONE());     /* 15.2.19.7.20 */

  $define_method(mrb, tc, "mon"  , $time_mon, $ARGS_NONE());       /* 15.2.19.7.21 */
  $define_method(mrb, tc, "month", $time_mon, $ARGS_NONE());       /* 15.2.19.7.22 */

  $define_method(mrb, tc, "sec" , $time_sec, $ARGS_NONE());        /* 15.2.19.7.23 */
  $define_method(mrb, tc, "to_i", $time_to_i, $ARGS_NONE());       /* 15.2.19.7.25 */
  $define_method(mrb, tc, "to_f", $time_to_f, $ARGS_NONE());       /* 15.2.19.7.24 */
  $define_method(mrb, tc, "usec", $time_usec, $ARGS_NONE());       /* 15.2.19.7.26 */
  $define_method(mrb, tc, "utc" , $time_utc, $ARGS_NONE());        /* 15.2.19.7.27 */
  $define_method(mrb, tc, "utc?", $time_utc_p,$ARGS_NONE());       /* 15.2.19.7.28 */
  $define_method(mrb, tc, "wday", $time_wday, $ARGS_NONE());       /* 15.2.19.7.30 */
  $define_method(mrb, tc, "yday", $time_yday, $ARGS_NONE());       /* 15.2.19.7.31 */
  $define_method(mrb, tc, "year", $time_year, $ARGS_NONE());       /* 15.2.19.7.32 */
  $define_method(mrb, tc, "zone", $time_zone, $ARGS_NONE());       /* 15.2.19.7.33 */

  $define_method(mrb, tc, "initialize", $time_initialize, $ARGS_REQ(1)); /* 15.2.19.7.16 */
  $define_method(mrb, tc, "initialize_copy", $time_initialize_copy, $ARGS_REQ(1)); /* 15.2.19.7.17 */

  /*
    methods not available:
      gmt_offset(15.2.19.7.12)
      gmtoff(15.2.19.7.14)
      utc_offset(15.2.19.7.29)
  */
}

void
$mruby_time_gem_final($state* mrb)
{
}
