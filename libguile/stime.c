/* Copyright (C) 1995,1996,1997,1998,1999,2000,2001, 2003, 2004 Free Software Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */




#define _GNU_SOURCE  /* ask glibc for everything, in particular strptime */
#define _POSIX_C_SOURCE 199506L  /* for gmtime_r prototype */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <errno.h>

#include "libguile/_scm.h"
#include "libguile/feature.h"
#include "libguile/strings.h"
#include "libguile/vectors.h"

#include "libguile/validate.h"
#include "libguile/stime.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif

#ifdef HAVE_SYS_TIMEB_H
# include <sys/timeb.h>
#endif

#if HAVE_CRT_EXTERNS_H
#include <crt_externs.h>  /* for Darwin _NSGetEnviron */
#endif

#ifndef tzname /* For SGI.  */
extern char *tzname[]; /* RS6000 and others reject char **tzname.  */
#endif
#if defined (__MINGW32__)
# define tzname _tzname
#endif

#if ! HAVE_DECL_STRPTIME
extern char *strptime ();
#endif

#ifdef __STDC__
# define timet time_t
#else
# define timet long
#endif

extern char ** environ;

/* On Apple Darwin in a shared library there's no "environ" to access
   directly, instead the address of that variable must be obtained with
   _NSGetEnviron().  */
#if HAVE__NSGETENVIRON && defined (PIC)
#define environ (*_NSGetEnviron())
#endif


#ifdef HAVE_TIMES
static
timet mytime()
{
  struct tms time_buffer;
  times(&time_buffer);
  return time_buffer.tms_utime + time_buffer.tms_stime;
}
#else
# ifdef LACK_CLOCK
#    define mytime() ((time((timet*)0) - scm_your_base) * SCM_TIME_UNITS_PER_SECOND)
# else
#  define mytime clock
# endif
#endif

#ifdef HAVE_FTIME
struct timeb scm_your_base = {0};
#else
timet scm_your_base = 0;
#endif

SCM_DEFINE (scm_get_internal_real_time, "get-internal-real-time", 0, 0, 0,
           (),
	    "Return the number of time units since the interpreter was\n"
	    "started.")
#define FUNC_NAME s_scm_get_internal_real_time
{
#ifdef HAVE_FTIME
  struct timeb time_buffer;

  SCM tmp;
  ftime (&time_buffer);
  time_buffer.time -= scm_your_base.time;
  tmp = scm_long2num (time_buffer.millitm - scm_your_base.millitm);
  tmp = scm_sum (tmp,
		 scm_product (scm_from_int (1000),
			      scm_from_int (time_buffer.time)));
  return scm_quotient (scm_product (tmp, SCM_I_MAKINUM (SCM_TIME_UNITS_PER_SECOND)),
		       scm_from_int (1000));
#else
  return scm_long2num((time((timet*)0) - scm_your_base) * (int)SCM_TIME_UNITS_PER_SECOND);
#endif /* HAVE_FTIME */
}
#undef FUNC_NAME


#ifdef HAVE_TIMES
SCM_DEFINE (scm_times, "times", 0, 0, 0,
            (void),
	    "Return an object with information about real and processor\n"
	    "time.  The following procedures accept such an object as an\n"
	    "argument and return a selected component:\n"
	    "\n"
	    "@table @code\n"
	    "@item tms:clock\n"
	    "The current real time, expressed as time units relative to an\n"
	    "arbitrary base.\n"
	    "@item tms:utime\n"
	    "The CPU time units used by the calling process.\n"
	    "@item tms:stime\n"
	    "The CPU time units used by the system on behalf of the calling\n"
	    "process.\n"
	    "@item tms:cutime\n"
	    "The CPU time units used by terminated child processes of the\n"
	    "calling process, whose status has been collected (e.g., using\n"
	    "@code{waitpid}).\n"
	    "@item tms:cstime\n"
	    "Similarly, the CPU times units used by the system on behalf of\n"
	    "terminated child processes.\n"
	    "@end table")
#define FUNC_NAME s_scm_times
{
  struct tms t;
  clock_t rv;

  SCM result = scm_c_make_vector (5, SCM_UNDEFINED);
  rv = times (&t);
  if (rv == -1)
    SCM_SYSERROR;
  SCM_VECTOR_SET (result, 0, scm_long2num (rv));
  SCM_VECTOR_SET (result, 1, scm_long2num (t.tms_utime));
  SCM_VECTOR_SET (result, 2, scm_long2num (t.tms_stime));
  SCM_VECTOR_SET (result ,3, scm_long2num (t.tms_cutime));
  SCM_VECTOR_SET (result, 4, scm_long2num (t.tms_cstime));
  return result;
}
#undef FUNC_NAME
#endif /* HAVE_TIMES */

static long scm_my_base = 0;

long
scm_c_get_internal_run_time ()
{
  return mytime () - scm_my_base;
}

SCM_DEFINE (scm_get_internal_run_time, "get-internal-run-time", 0, 0, 0,
           (void),
	    "Return the number of time units of processor time used by the\n"
	    "interpreter.  Both @emph{system} and @emph{user} time are\n"
	    "included but subprocesses are not.")
#define FUNC_NAME s_scm_get_internal_run_time
{
  return scm_long2num (scm_c_get_internal_run_time ());
}
#undef FUNC_NAME

/* For reference, note that current-time and gettimeofday both should be
   protected against setzone/restorezone changes in another thread, since on
   DOS the system time is normally kept as local time, which means TZ
   affects the return from current-time and gettimeofday.  Not sure if DJGPP
   etc actually has concurrent multi-threading, but it seems prudent not to
   make assumptions about this.  */

SCM_DEFINE (scm_current_time, "current-time", 0, 0, 0,
           (void),
	    "Return the number of seconds since 1970-01-01 00:00:00 UTC,\n"
	    "excluding leap seconds.")
#define FUNC_NAME s_scm_current_time
{
  timet timv;

  SCM_DEFER_INTS;
  if ((timv = time (0)) == -1)
    SCM_MISC_ERROR ("current time not available", SCM_EOL);
  SCM_ALLOW_INTS;
  return scm_long2num((long) timv);
}
#undef FUNC_NAME

SCM_DEFINE (scm_gettimeofday, "gettimeofday", 0, 0, 0,
            (void),
	    "Return a pair containing the number of seconds and microseconds\n"
	    "since 1970-01-01 00:00:00 UTC, excluding leap seconds.  Note:\n"
	    "whether true microsecond resolution is available depends on the\n"
	    "operating system.")
#define FUNC_NAME s_scm_gettimeofday
{
#ifdef HAVE_GETTIMEOFDAY
  struct timeval time;

  SCM_DEFER_INTS;
  if (gettimeofday (&time, NULL) == -1)
    SCM_SYSERROR;
  SCM_ALLOW_INTS;
  return scm_cons (scm_long2num ((long) time.tv_sec),
		   scm_long2num ((long) time.tv_usec));
#else
# ifdef HAVE_FTIME
  struct timeb time;

  ftime(&time);
  return scm_cons (scm_long2num ((long) time.time),
		   scm_from_int (time.millitm * 1000));
# else
  timet timv;

  SCM_DEFER_INTS;
  if ((timv = time (0)) == -1)
    SCM_SYSERROR;
  SCM_ALLOW_INTS;
  return scm_cons (scm_long2num (timv), scm_from_int (0));
# endif
#endif
}
#undef FUNC_NAME

static SCM
filltime (struct tm *bd_time, int zoff, const char *zname)
{
  SCM result = scm_c_make_vector (11, SCM_UNDEFINED);

  SCM_VECTOR_SET (result,0, scm_from_int (bd_time->tm_sec));
  SCM_VECTOR_SET (result,1, scm_from_int (bd_time->tm_min));
  SCM_VECTOR_SET (result,2, scm_from_int (bd_time->tm_hour));
  SCM_VECTOR_SET (result,3, scm_from_int (bd_time->tm_mday));
  SCM_VECTOR_SET (result,4, scm_from_int (bd_time->tm_mon));
  SCM_VECTOR_SET (result,5, scm_from_int (bd_time->tm_year));
  SCM_VECTOR_SET (result,6, scm_from_int (bd_time->tm_wday));
  SCM_VECTOR_SET (result,7, scm_from_int (bd_time->tm_yday));
  SCM_VECTOR_SET (result,8, scm_from_int (bd_time->tm_isdst));
  SCM_VECTOR_SET (result,9, scm_from_int (zoff));
  SCM_VECTOR_SET (result,10, zname ? scm_makfrom0str (zname) : SCM_BOOL_F);
  return result;
}

static char tzvar[3] = "TZ";

/* if zone is set, create a temporary environment with only a TZ
   string.  other threads or interrupt handlers shouldn't be allowed
   to run until the corresponding restorezone is called.  hence the use
   of a static variable for tmpenv is no big deal.  */
static char **
setzone (SCM zone, int pos, const char *subr)
{
  char **oldenv = 0;

  if (!SCM_UNBNDP (zone))
    {
      static char *tmpenv[2];
      char *buf;

      SCM_ASSERT (SCM_STRINGP (zone), zone, pos, subr);
      buf = scm_malloc (SCM_STRING_LENGTH (zone) + sizeof (tzvar) + 1);
      sprintf (buf, "%s=%s", tzvar, SCM_STRING_CHARS (zone));
      oldenv = environ;
      tmpenv[0] = buf;
      tmpenv[1] = 0;
      environ = tmpenv;
    }
  return oldenv;
}

static void
restorezone (SCM zone, char **oldenv, const char *subr SCM_UNUSED)
{
  if (!SCM_UNBNDP (zone))
    {
      free (environ[0]);
      environ = oldenv;
#ifdef HAVE_TZSET
      /* for the possible benefit of user code linked with libguile.  */
      tzset();
#endif
    }
}

SCM_DEFINE (scm_localtime, "localtime", 1, 1, 0,
            (SCM time, SCM zone),
	    "Return an object representing the broken down components of\n"
	    "@var{time}, an integer like the one returned by\n"
	    "@code{current-time}.  The time zone for the calculation is\n"
	    "optionally specified by @var{zone} (a string), otherwise the\n"
	    "@code{TZ} environment variable or the system default is used.")
#define FUNC_NAME s_scm_localtime
{
  timet itime;
  struct tm *ltptr, lt, *utc;
  SCM result;
  int zoff;
  char *zname = 0;
  char **oldenv;
  int err;

  itime = SCM_NUM2LONG (1, time);

  /* deferring interupts is essential since a) setzone may install a temporary
     environment b) localtime uses a static buffer.  */
  SCM_DEFER_INTS;
  oldenv = setzone (zone, SCM_ARG2, FUNC_NAME);
#ifdef LOCALTIME_CACHE
  tzset ();
#endif
  /* POSIX says localtime sets errno, but C99 doesn't say that.
     Give a sensible default value in case localtime doesn't set it.  */
  errno = EINVAL;
  ltptr = localtime (&itime);
  err = errno;
  if (ltptr)
    {
      const char *ptr;

      /* copy zone name before calling gmtime or restoring zone.  */
#if defined (HAVE_TM_ZONE)
      ptr = ltptr->tm_zone;
#elif defined (HAVE_TZNAME)
      ptr = tzname[ (ltptr->tm_isdst == 1) ? 1 : 0 ];
#else
      ptr = "";
#endif
      zname = scm_malloc (strlen (ptr) + 1);
      strcpy (zname, ptr);
    }
  /* the struct is copied in case localtime and gmtime share a buffer.  */
  if (ltptr)
    lt = *ltptr;
  /* POSIX says gmtime sets errno, but C99 doesn't say that.
     Give a sensible default value in case gmtime doesn't set it.  */
  errno = EINVAL;
  utc = gmtime (&itime);
  if (utc == NULL)
    err = errno;
  restorezone (zone, oldenv, FUNC_NAME);
  /* delayed until zone has been restored.  */
  errno = err;
  if (utc == NULL || ltptr == NULL)
    SCM_SYSERROR;

  /* calculate timezone offset in seconds west of UTC.  */
  zoff = (utc->tm_hour - lt.tm_hour) * 3600 + (utc->tm_min - lt.tm_min) * 60
    + utc->tm_sec - lt.tm_sec;
  if (utc->tm_year < lt.tm_year)
    zoff -= 24 * 60 * 60;
  else if (utc->tm_year > lt.tm_year)
    zoff += 24 * 60 * 60;
  else if (utc->tm_yday < lt.tm_yday)
    zoff -= 24 * 60 * 60;
  else if (utc->tm_yday > lt.tm_yday)
    zoff += 24 * 60 * 60;

  result = filltime (&lt, zoff, zname);
  SCM_ALLOW_INTS;
  if (zname)
    free (zname);
  return result;
}
#undef FUNC_NAME

/* tm_zone is normally a pointer, not an array within struct tm, so we might
   have to worry about the lifespan of what it points to.  The posix specs
   don't seem to say anything about this, let's assume here that tm_zone
   will be a constant and therefore no protection or anything is needed
   until we copy it in filltime().  */

SCM_DEFINE (scm_gmtime, "gmtime", 1, 0, 0,
            (SCM time),
	    "Return an object representing the broken down components of\n"
	    "@var{time}, an integer like the one returned by\n"
	    "@code{current-time}.  The values are calculated for UTC.")
#define FUNC_NAME s_scm_gmtime
{
  timet itime;
  struct tm bd_buf, *bd_time;
  const char *zname;

  itime = SCM_NUM2LONG (1, time);

  /* POSIX says gmtime sets errno, but C99 doesn't say that.
     Give a sensible default value in case gmtime doesn't set it.  */
  errno = EINVAL;

#if HAVE_GMTIME_R
  bd_time = gmtime_r (&itime, &bd_buf);
#else
  SCM_DEFER_INTS;
  bd_time = gmtime (&itime);
  if (bd_time != NULL)
    bd_buf = *bd_time;
  SCM_ALLOW_INTS;
#endif
  if (bd_time == NULL)
    SCM_SYSERROR;

#if HAVE_STRUCT_TM_TM_ZONE
  zname = bd_buf.tm_zone;
#else
  zname = "GMT";
#endif
  return filltime (&bd_buf, 0, zname);
}
#undef FUNC_NAME

/* copy time components from a Scheme object to a struct tm.  */
static void
bdtime2c (SCM sbd_time, struct tm *lt, int pos, const char *subr)
{
  SCM const *velts;
  int i;

  SCM_ASSERT (SCM_VECTORP (sbd_time)
	      && SCM_VECTOR_LENGTH (sbd_time) == 11,
	      sbd_time, pos, subr);
  velts = SCM_VELTS (sbd_time);
  for (i = 0; i < 10; i++)
    {
      SCM_ASSERT (scm_is_integer (velts[i]), sbd_time, pos, subr);
    }
  SCM_ASSERT (scm_is_false (velts[10]) || SCM_STRINGP (velts[10]),
	      sbd_time, pos, subr);

  lt->tm_sec = scm_to_int (velts[0]);
  lt->tm_min = scm_to_int (velts[1]);
  lt->tm_hour = scm_to_int (velts[2]);
  lt->tm_mday = scm_to_int (velts[3]);
  lt->tm_mon = scm_to_int (velts[4]);
  lt->tm_year = scm_to_int (velts[5]);
  lt->tm_wday = scm_to_int (velts[6]);
  lt->tm_yday = scm_to_int (velts[7]);
  lt->tm_isdst = scm_to_int (velts[8]);
#ifdef HAVE_TM_ZONE
  lt->tm_gmtoff = scm_to_int (velts[9]);
  if (scm_is_false (velts[10]))
    lt->tm_zone = NULL;
  else
    lt->tm_zone  = SCM_STRING_CHARS (velts[10]);
#endif
}

SCM_DEFINE (scm_mktime, "mktime", 1, 1, 0,
            (SCM sbd_time, SCM zone),
	    "@var{bd-time} is an object representing broken down time and @code{zone}\n"
	    "is an optional time zone specifier (otherwise the TZ environment variable\n"
	    "or the system default is used).\n\n"
	    "Returns a pair: the car is a corresponding\n"
	    "integer time value like that returned\n"
	    "by @code{current-time}; the cdr is a broken down time object, similar to\n"
	    "as @var{bd-time} but with normalized values.")
#define FUNC_NAME s_scm_mktime
{
  timet itime;
  struct tm lt, *utc;
  SCM result;
  int zoff;
  char *zname = 0;
  char **oldenv;
  int err;

  bdtime2c (sbd_time, &lt, SCM_ARG1, FUNC_NAME);

  SCM_DEFER_INTS;
  oldenv = setzone (zone, SCM_ARG2, FUNC_NAME);
#ifdef LOCALTIME_CACHE
  tzset ();
#endif
  itime = mktime (&lt);
  /* POSIX doesn't say mktime sets errno, and on glibc 2.3.2 for instance it
     doesn't.  Force a sensible value for our error message.  */
  err = EINVAL;

  if (itime != -1)
    {
      const char *ptr;

      /* copy zone name before calling gmtime or restoring the zone.  */
#if defined (HAVE_TM_ZONE)
      ptr = lt.tm_zone;
#elif defined (HAVE_TZNAME)
      ptr = tzname[ (lt.tm_isdst == 1) ? 1 : 0 ];
#else
      ptr = "";
#endif
      zname = scm_malloc (strlen (ptr) + 1);
      strcpy (zname, ptr);
    }

  /* get timezone offset in seconds west of UTC.  */
  /* POSIX says gmtime sets errno, but C99 doesn't say that.
     Give a sensible default value in case gmtime doesn't set it.  */
  utc = gmtime (&itime);
  if (utc == NULL)
    err = errno;

  restorezone (zone, oldenv, FUNC_NAME);
  /* delayed until zone has been restored.  */
  errno = err;
  if (utc == NULL || itime == -1)
    SCM_SYSERROR;

  zoff = (utc->tm_hour - lt.tm_hour) * 3600 + (utc->tm_min - lt.tm_min) * 60
    + utc->tm_sec - lt.tm_sec;
  if (utc->tm_year < lt.tm_year)
    zoff -= 24 * 60 * 60;
  else if (utc->tm_year > lt.tm_year)
    zoff += 24 * 60 * 60;
  else if (utc->tm_yday < lt.tm_yday)
    zoff -= 24 * 60 * 60;
  else if (utc->tm_yday > lt.tm_yday)
    zoff += 24 * 60 * 60;

  result = scm_cons (scm_long2num ((long) itime),
		     filltime (&lt, zoff, zname));
  SCM_ALLOW_INTS;
  if (zname)
    free (zname);
  return result;
}
#undef FUNC_NAME

#ifdef HAVE_TZSET
SCM_DEFINE (scm_tzset, "tzset", 0, 0, 0,
            (void),
	    "Initialize the timezone from the TZ environment variable\n"
	    "or the system default.  It's not usually necessary to call this procedure\n"
	    "since it's done automatically by other procedures that depend on the\n"
	    "timezone.")
#define FUNC_NAME s_scm_tzset
{
  tzset();
  return SCM_UNSPECIFIED;
}
#undef FUNC_NAME
#endif /* HAVE_TZSET */

SCM_DEFINE (scm_strftime, "strftime", 2, 0, 0,
            (SCM format, SCM stime),
	    "Formats a time specification @var{time} using @var{template}.  @var{time}\n"
	    "is an object with time components in the form returned by @code{localtime}\n"
	    "or @code{gmtime}.  @var{template} is a string which can include formatting\n"
	    "specifications introduced by a @code{%} character.  The formatting of\n"
	    "month and day names is dependent on the current locale.  The value returned\n"
	    "is the formatted string.\n"
	    "@xref{Formatting Date and Time, , , libc, The GNU C Library Reference Manual}.)")
#define FUNC_NAME s_scm_strftime
{
  struct tm t;

  char *tbuf;
  int size = 50;
  char *fmt, *myfmt;
  int len;
  SCM result;

  SCM_VALIDATE_STRING (1, format);
  bdtime2c (stime, &t, SCM_ARG2, FUNC_NAME);

  fmt = SCM_STRING_CHARS (format);
  len = SCM_STRING_LENGTH (format);

  /* Ugly hack: strftime can return 0 if its buffer is too small,
     but some valid time strings (e.g. "%p") can sometimes produce
     a zero-byte output string!  Workaround is to prepend a junk
     character to the format string, so that valid returns are always
     nonzero. */
  myfmt = scm_malloc (len+2);
  *myfmt = 'x';
  strncpy(myfmt+1, fmt, len);
  myfmt[len+1] = 0;

  tbuf = scm_malloc (size);
  {
#if !defined (HAVE_TM_ZONE)
    /* it seems the only way to tell non-GNU versions of strftime what
       zone to use (for the %Z format) is to set TZ in the
       environment.  interrupts and thread switching must be deferred
       until TZ is restored.  */
    char **oldenv = NULL;
    SCM *velts = (SCM *) SCM_VELTS (stime);
    int have_zone = 0;

    if (scm_is_true (velts[10]) && *SCM_STRING_CHARS (velts[10]) != 0)
      {
	/* it's not required that the TZ setting be correct, just that
	   it has the right name.  so try something like TZ=EST0.
	   using only TZ=EST would be simpler but it doesn't work on
	   some OSs, e.g., Solaris.  */
	SCM zone =
	  scm_string_append (scm_cons (velts[10],
				       scm_cons (scm_makfrom0str ("0"),
						 SCM_EOL)));

	have_zone = 1;
	SCM_DEFER_INTS;
	oldenv = setzone (zone, SCM_ARG2, FUNC_NAME);
      }
#endif

#ifdef LOCALTIME_CACHE
    tzset ();
#endif

    /* POSIX says strftime returns 0 on buffer overrun, but old
       systems (i.e. libc 4 on GNU/Linux) might return `size' in that
       case. */
    while ((len = strftime (tbuf, size, myfmt, &t)) == 0 || len == size)
      {
	free (tbuf);
	size *= 2;
	tbuf = scm_malloc (size);
      }

#if !defined (HAVE_TM_ZONE)
    if (have_zone)
      {
	restorezone (velts[10], oldenv, FUNC_NAME);
	SCM_ALLOW_INTS;
      }
#endif
    }

  result = scm_mem2string (tbuf + 1, len - 1);
  free (tbuf);
  free (myfmt);
  return result;
}
#undef FUNC_NAME

#ifdef HAVE_STRPTIME
SCM_DEFINE (scm_strptime, "strptime", 2, 0, 0,
            (SCM format, SCM string),
	    "Performs the reverse action to @code{strftime}, parsing\n"
	    "@var{string} according to the specification supplied in\n"
	    "@var{template}.  The interpretation of month and day names is\n"
	    "dependent on the current locale.  The value returned is a pair.\n"
	    "The car has an object with time components\n"
	    "in the form returned by @code{localtime} or @code{gmtime},\n"
	    "but the time zone components\n"
	    "are not usefully set.\n"
	    "The cdr reports the number of characters from @var{string}\n"
	    "which were used for the conversion.")
#define FUNC_NAME s_scm_strptime
{
  struct tm t;
  char *fmt, *str, *rest;

  SCM_VALIDATE_STRING (1, format);
  SCM_VALIDATE_STRING (2, string);

  fmt = SCM_STRING_CHARS (format);
  str = SCM_STRING_CHARS (string);

  /* initialize the struct tm */
#define tm_init(field) t.field = 0
  tm_init (tm_sec);
  tm_init (tm_min);
  tm_init (tm_hour);
  tm_init (tm_mday);
  tm_init (tm_mon);
  tm_init (tm_year);
  tm_init (tm_wday);
  tm_init (tm_yday);
#undef tm_init

  /* GNU glibc strptime() "%s" is affected by the current timezone, since it
     reads a UTC time_t value and converts with localtime_r() to set the tm
     fields, hence the use of SCM_DEFER_INTS.  */
  t.tm_isdst = -1;
  SCM_DEFER_INTS;
  if ((rest = strptime (str, fmt, &t)) == NULL)
    {
      /* POSIX doesn't say strptime sets errno, and on glibc 2.3.2 for
         instance it doesn't.  Force a sensible value for our error
         message.  */
      errno = EINVAL;
      SCM_SYSERROR;
    }

  SCM_ALLOW_INTS;
  return scm_cons (filltime (&t, 0, NULL),
		   scm_from_signed_integer (rest - str));
}
#undef FUNC_NAME
#endif /* HAVE_STRPTIME */

void
scm_init_stime()
{
  scm_c_define ("internal-time-units-per-second",
		scm_long2num((long) SCM_TIME_UNITS_PER_SECOND));

#ifdef HAVE_FTIME
  if (!scm_your_base.time) ftime(&scm_your_base);
#else
  if (!scm_your_base) time(&scm_your_base);
#endif

  if (!scm_my_base) scm_my_base = mytime();

  scm_add_feature ("current-time");
#include "libguile/stime.x"
}


/*
  Local Variables:
  c-file-style: "gnu"
  End:
*/
