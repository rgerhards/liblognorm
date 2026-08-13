#!/bin/bash
# Verify lognormalizer exposes expected-next diagnostics for v1 and v2 typed
# motifs after a realistic multi-field prefix, rather than only for literals.
. "${srcdir:=.}/exec.sh"

export ln_opts='-oaddExpectedNext'
message='gateway=edgerouter17 tenant=customerblue source=10.33.245.213 operation=policycommit result='

add_rule 'version=2'
add_rule 'rule=:gateway=%gateway:word% tenant=%tenant:word% source=%source:ipv4% operation=%operation:alpha% result=%result:number%'
execute "$message"
assert_output_contains '"parse-error"'
assert_output_contains '"parser": "number"'
assert_output_contains '"field": "result"'

reset_rules
add_rule 'rule=:gateway=%gateway:word% tenant=%tenant:word% source=%source:ipv4% operation=%operation:alpha% result=%result:number%'
execute "$message"
assert_output_contains '"parse-error"'
assert_output_contains '"parser": "number"'
assert_output_contains '"field": "result"'
