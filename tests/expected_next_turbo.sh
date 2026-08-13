#!/bin/bash
# Verify that expected-next diagnostics deliberately select the recursive
# walker when Turbo is requested, preserving the complete failure report.
. "${srcdir:=.}/exec.sh"

export ln_opts='-oturbo -oaddExpectedNext'
add_rule 'version=2'
add_rule 'rule=:gateway=%gateway:word% tenant=%tenant:word% source=%source:ipv4% operation=%operation:word% result=%result:number%'
execute 'gateway=edge-router-17 tenant=customer-blue source=10.33.245.213 operation=policy-commit result='
assert_output_contains '"parse-error"'
assert_output_contains '"parser": "number"'
assert_output_contains '"field": "result"'
