#!/bin/bash
# shellcheck disable=SC2154 # Test paths and command options come from exec.sh.
# Verify that expected-next diagnostics deliberately select the recursive
# walker when Turbo is requested, preserving the complete failure report.
# shellcheck source=exec.sh
. "${srcdir:=.}/exec.sh"

export ln_opts='-oturbo -oaddExpectedNext'
add_rule 'version=2'
add_rule 'rule=:gateway=%gateway:word% tenant=%tenant:word% source=%source:ipv4% operation=%operation:word% result=%result:number%'
execute 'gateway=edge-router-17 tenant=customer-blue source=10.33.245.213 operation=policy-commit result='
assert_output_contains '"parse-error"'
assert_output_contains '"parser": "number"'
assert_output_contains '"field": "result"'

# The option combination must be visible to CLI users: stderr is the oracle
# because silently losing Turbo throughput would otherwise be hard to diagnose.
warning_out="$test_tmpdir/turbo-warning.out"
(
	cd "$test_tmpdir" || exit 1
	# shellcheck disable=SC2086 # cmd and ln_opts intentionally contain arguments.
	printf '%s\n' 'gateway=edge-router-17 tenant=customer-blue source=10.33.245.213 operation=policy-commit result=' |
		$cmd $ln_opts -r "$(rulebase_file_name)" -e json >/dev/null 2>"$warning_out"
)
grep -F 'liblognorm warning: addExpectedNext disables TurboVM; normalization will use the slower recursive walker' \
	"$warning_out"
