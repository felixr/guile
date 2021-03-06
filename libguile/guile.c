/* Copyright 1996-1997,2000-2001,2006,2008,2011,2013,2018,2021
     Free Software Foundation, Inc.

   This file is part of Guile.

   Guile is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Guile is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Guile.  If not, see
   <https://www.gnu.org/licenses/>.  */

/* This is the 'main' function for the `guile' executable.  It is not
   included in libguile.a.

   Eventually, we hope this file will be automatically generated,
   based on the list of installed, statically linked libraries on the
   system.  For now, please don't put interesting code in here.  */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <locale.h>
#include <stdio.h>

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#include <libguile.h>

static void
inner_main (void *closure SCM_UNUSED, int argc, char **argv)
{
#ifdef __MINGW32__
  /* This is necessary to startup the Winsock API under Win32. */
  WSADATA WSAData;
  WSAStartup (0x0202, &WSAData);
#endif /* __MINGW32__ */

  /* module initializations would go here */
  scm_shell (argc, argv);

#ifdef __MINGW32__
  WSACleanup ();
#endif /* __MINGW32__ */
}

static int
get_integer_from_environment (const char *var, int def)
{
  char *end = 0;
  char *val = getenv (var);
  long res = def;
  if (!val)
    return def;
  res = strtol (val, &end, 10);
  if (end == val)
    {
      fprintf (stderr, "guile: warning: invalid %s: %s\n", var, val);
      return def;
    }
  return res;
}

static int
should_install_locale (void)
{
  /* If the GUILE_INSTALL_LOCALE environment variable is unset,
     or set to a nonzero value, we should install the locale via
     setlocale().  */
  return get_integer_from_environment ("GUILE_INSTALL_LOCALE", 1);
}

int
main (int argc, char **argv)
{
  /* If we should install a locale, do it right at the beginning so that
     string conversion for command-line arguments, along with possible
     error messages, use the right locale.  See
     <https://lists.gnu.org/archive/html/guile-devel/2011-11/msg00041.html>
     for the rationale.  */
  if (should_install_locale () && setlocale (LC_ALL, "") == NULL)
    fprintf (stderr, "guile: warning: failed to install locale\n");

  scm_boot_guile (argc, argv, inner_main, 0);
  return 0; /* never reached */
}
