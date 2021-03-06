# eealloc.m4 serial 1
dnl Copyright (C) 2003 Free Software Foundation, Inc.
dnl This file is free software, distributed under the terms of the GNU
dnl General Public License.  As a special exception to the GNU General
dnl Public License, this file may be distributed as part of a program
dnl that contains a configuration script generated by Autoconf, under
dnl the same distribution terms as the rest of that program.

AC_DEFUN([gl_EEALLOC],
[
  AC_REQUIRE([gl_EEMALLOC])
  AC_REQUIRE([gl_EEREALLOC])
  AC_REQUIRE([AC_C_INLINE])
])

AC_DEFUN([gl_EEMALLOC],
[
  _AC_FUNC_MALLOC_IF(
    [gl_cv_func_malloc_0_nonnull=1],
    [gl_cv_func_malloc_0_nonnull=0])
  AC_DEFINE_UNQUOTED([MALLOC_0_IS_NONNULL], $gl_cv_func_malloc_0_nonnull,
    [If malloc(0) is != NULL, define this to 1.  Otherwise define this
     to 0.])
])

AC_DEFUN([gl_EEREALLOC],
[
  _AC_FUNC_REALLOC_IF(
    [gl_cv_func_realloc_0_nonnull=1],
    [gl_cv_func_realloc_0_nonnull=0])
  AC_DEFINE_UNQUOTED([REALLOC_0_IS_NONNULL], $gl_cv_func_realloc_0_nonnull,
    [If realloc(NULL,0) is != NULL, define this to 1.  Otherwise define this
     to 0.])
])
