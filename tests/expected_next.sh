#!/bin/bash
# Verify bounded expected-next diagnostics through the public liblognorm API.

if [ -x "./expected_next" ] && [ -d "../src/.libs" ]; then
	test_bin="./expected_next"
	build_libdir="../src/.libs"
else
	script_dir="$(
		CDPATH=
		cd -- "$(dirname "$0")" || exit 1
		pwd
	)"
	top_builddir="${top_builddir:-${script_dir}/..}"
	test_bin="${top_builddir}/tests/expected_next"
	build_libdir="${top_builddir}/src/.libs"
fi

if [ -n "${LD_LIBRARY_PATH}" ]; then
	export LD_LIBRARY_PATH="${build_libdir}:${LD_LIBRARY_PATH}"
else
	export LD_LIBRARY_PATH="${build_libdir}"
fi

exec "${test_bin}"
