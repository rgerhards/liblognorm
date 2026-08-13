Lognormalizer
=============

Lognormalizer is a sample tool which is often used to test and debug 
rulebases before real use. Nevertheless, it can be used in production as 
a simple command line interface to liblognorm.

This tool reads log lines from its standard input and prints results 
to standard output. You need to use redirections if you want to read 
or write files.

An example of the command::

    $ lognormalizer -r messages.sampdb -e json <messages.log

Command line options
--------------------

::

    -V

Output version information, including information about the installed
version of liblognorm and its optional features. So this may also be
used to check the currently installed library version.

::

    -r <FILENAME>

Specifies name of the file containing the rulebase.

::

    -v
    
Increase verbosity level. Can be used several times. If used three
times, internal data structures are dumped (make sense to developers,
only).

::

    -p

Print only successfully parsed messages.

::

    -P

Print only messages **not** successfully parsed.

::

    -L

Add line number information to events not successfully parsed. This
is meant as a troubleshooting aid when working with unparsable events,
as the information can be used to directly go to the line in question
in the source data file. The line number is contained in a field
named ``lognormalizer.line_nbr``.

::

    -t <TAG>
    
Print only those messages which have this tag.
    
::

    -T

Include 'event.tags' attribute when output is in JSON format. This attribute contains list of tags of the matched 
rule.

::

    -E <DATA>

Encoder-specific data. For CSV, it is the list of fields to be output, 
separated by comma or space. It is currently unused for other formats.

::

    -d <FILENAME>

Generate DOT file describing parse tree. It is used to plot parse graph 
with GraphViz.

::

    -H

At end of run, print a summary line with number of messages processed,
parsed and unparsed to stdout.

::

    -U

At end of run, print a summary line with number of messages unparsed to
stdout. Note that this message is only printed if there was at least one
unparsable message.

::

    -o

Special options. The following ones can be set:

   * **allowRegex** Permits to use regular expressions inse the v1 engine
     This is deprecated and should not be used for new deployments.

   * **addExecPath** Includes metadata into the event on how it was
     (tried) to be parsed. Can be useful in troubleshooting normalization
     problems.

   * **addOriginalMsg** Always add the "original-msg" data item. By
     default, this is only done when a message could not be parsed.

   * **addRule** Add a mockup of the rule that was processed. Note that
     it is *not* an exact copy of the rule, but a rule that correctly
     describes the parsed message. Most importantly, prefixes are 
     appended and custom data types are expanded (and no longer visible
     as such). This option is primarily meant for postprocessing, e.g.
     as input to an anonymizer.

   * **addRuleLocation** For rules that successfully parsed, add the
     location of the rule inside the rulebase. Both the file name and
     the line number are given. If two rules evaluate to the same
     end node, only a single rule location is given. However, in
     practice this is extremely unlikely and as such for practical
     reasons the information can be considered reliable.

   * **addExpectedNext** Add a bounded ``parse-error`` object to JSON output
     for messages that could not be normalized. It reports the greatest byte
     offset reached by any attempted rule and the literals, parser motifs, or
     end of input that could have continued a rule at that offset. The option
     is intended for rulebase troubleshooting and has no effect on successful
     events. It is disabled by default and does not allocate or collect
     candidates unless explicitly enabled.

     The recursive rule walker is required to collect alternatives from all
     equally deep failed paths. Consequently, this option disables TurboVM for
     the context even if ``turbo`` is also requested. Disable the diagnostic
     option again for production Turbo performance.

   * **turbo** Enable TurboVM bytecode engine for normalization. This
     requires liblognorm to be built with ``--enable-turbo``. When
     enabled, normalization uses the compiled bytecode VM with SIMD
     acceleration. Output uses native JSON types (numbers as integers,
     not strings) and nested objects for dotted field names. Falls back
     to standard normalization if bytecode compilation failed.
     See :doc:`turbo` for details.

::

    -s <FILENAME>

At end of run, print internal parse DAG statistics and exit. This
option is meant for developers and researches which want to get insight
into the quality of the algorithm and/or how efficient the rulebase could
be processed. **NOT** intended for end users. This option is performance
intense.

::

    -S <FILENAME>

Even stronger statistics than -s. Requires that the version is compiled
with --enable-advanced-statistics, which causes a considerable
performance loss.

::

   -x <FILENAME>

Print statistics as a DOT file. In order to keep the graph readable,
information is only emitted for called nodes.

::

    -e <json|xml|csv|raw|cee-syslog>

Output format. By default, output is in JSON format. With this option,
you can change it to a different one.

Supported Output Formats
........................
The JSON, XML, and CSV formats should be self-explanatory.

The cee-syslog format emits messages according to the Mitre CEE spec.
Note that the cee-syslog format is primarily supported for
backward-compatibility. It does **not** support nested data items
and as such cannot be used when the rulebase makes use of this
feature (we assume this most often happens nowadays). We strongly
recommend not use it for new deployments. Support may be removed
in later releases.

The raw format outputs an exact copy of the input message, without
any normalization visible. The prime use case of "raw" is to extract
either all messages that could or could not be normalized. To do so
specify the -p or -P option. Also, it works in combination with the
-t option to extract a subset based on tagging. In any case, the core
use is to prepare a subset of the original file for further processing.

Troubleshooting rulebases with expected-next diagnostics
---------------------------------------------------------

Use ``addExpectedNext`` when ``unparsed-data`` alone does not explain why a
rule stopped. Preserve the original rulebase and input bytes while diagnosing
the problem; line-ending conversion can remove the evidence.

For example::

    $ printf '%s\n' 'fromhost-ip=10.33.245.213' | \
        lognormalizer -r rules.rb -e json -oaddExpectedNext

A failed event can contain::

    {
      "originalmsg":"fromhost-ip=10.33.245.213",
      "unparsed-data":"",
      "parse-error":{
        "offset":25,
        "at-eof":true,
        "expected-next":[
          {"type":"literal", "value":"\r", "hex":"0d", "length":1}
        ]
      }
    }

``offset`` is a zero-based byte offset, not a character index. The reported
offset is the greatest position reached across all attempted and backtracked
rules. ``at-eof`` says that this position is at or beyond the end of the input
message. Candidates at that position are deduplicated and can be:

* ``literal``: fixed rule text. ``value`` is a JSON representation, while
  ``hex`` exposes invisible bytes such as CR (``0d``), LF (``0a``), or a tab
  (``09``). ``length`` is the complete literal length. Long literals include
  only a 64-byte preview and set ``truncated`` to true.
* ``parser``: a motif that could begin there. ``parser`` gives its type, such
  as ``ipv4`` or ``number``, and ``field`` is included for named fields.
* ``end-of-input``: at least one rule was terminal at that position. If it is
  listed with other candidates, one rule could end while other conflicting
  rules could continue.

At most 32 alternatives are retained. ``expected-next-truncated`` is true if
additional alternatives existed. The list is deliberately a union of all
equally deep failures; it is not a claim that every candidate belongs to the
same rule.

An empty ``unparsed-data`` does not mean that normalization succeeded. It can
mean that a rule consumed the complete message but its parse-DAG node was not
terminal, or that the rule required another token after end of input. The
``parse-error`` object distinguishes these cases. If failed input contains
control bytes, compare ``unparsed-data`` with ``unparsed-data-binary``; the
latter preserves the complete unparsed byte span as hexadecimal.

Rulebase line endings are especially important. ``version=2`` must be the
first physical line of a version 2 file. Its version check accepts either LF or
CRLF. Ordinary rule records, however, are terminated by LF and retain a CR
immediately before it. A CRLF rule ending after ``%source:ipv4%`` therefore
adds a literal CR after the IPv4 motif. An LF-framed input message normally
reaches the normalizer without its framing LF, so the IPv4 value can consume
the entire message and leave ``unparsed-data`` empty while the rule still
expects hexadecimal ``0d``. A visually blank CRLF line similarly contains a
CR record and may produce an ``invalid record type`` rulebase-load error.

Check the physical bytes rather than relying on an editor display::

    $ file rules.rb
    $ sed -n '1,20l' rules.rb
    $ od -An -tx1 -c rules.rb | less

``sed -n l`` displays CRLF endings as ``\\r$``. Also confirm that comments
start in column one and that ``version=2`` has not been displaced by a comment
or blank line. Do not rewrite the file until the original behavior has been
reproduced.

For deeper inspection, add ``-vvv`` to print rule loading, parse attempts,
backtracking, and the internal parse-DAG dump. Generate a graph separately
with ``-d graph.dot`` and render it with Graphviz. Search the verbose dump and
DOT output around the reported byte offset, parser name, or literal hex value;
terminal-node flags explain why an otherwise fully consumed message did or did
not count as parsed. ``-s`` and ``-S`` can add parse-DAG statistics, but are
expensive and intended for development diagnostics.

Examples
--------

These examples were created using sample rulebase from source package.

Default (CEE) output::

	$ lognormalizer -r rulebases/sample.rulebase
	Weight: 42kg
	[cee@115 event.tags="tag2" unit="kg" N="42" fat="free"]
	Snow White and the Seven Dwarfs
	[cee@115 event.tags="tale" company="the Seven Dwarfs"]
	2012-10-11 src=127.0.0.1 dst=88.111.222.19
	[cee@115 dst="88.111.222.19" src="127.0.0.1" date="2012-10-11"]

JSON output, flat tags enabled::

	$ lognormalizer -r rulebases/sample.rulebase -e json -T
	%%
	{ "event.tags": [ "tag3", "percent" ], "percent": "100", "part": "wha", "whole": "whale" }
	Weight: 42kg
	{ "unit": "kg", "N": "42", "event.tags": [ "tag2" ], "fat": "free" }

CSV output with fixed field list::

	$ lognormalizer -r rulebases/sample.rulebase -e csv -E'N unit'
	Weight: 42kg
	"42","kg"
	Weight: 115lbs
	"115","lbs"
	Anything not matching the rule
	,

Creating a graph of the rulebase
--------------------------------

To get a better overview of a rulebase you can create a graph that shows you 
the chain of normalization (parse-tree).

At first you have to install an additional package called graphviz. Graphviz 
is a tool that creates such a graph with the help of a control file (created 
with the rulebase). `Here <http://www.graphviz.org/>`_ you will find more 
information about graphviz.

To install it you can use the package manager. For example, on RedHat 
systems it is yum command::

    $ sudo yum install graphviz

The next step would be creating the control file for graphviz. Therefore we 
use the normalizer command with the options -d "preferred filename for the
control file" and -r "rulebase"::

    $ lognormalize -d control.dot -r messages.rb

Please note that there is no need for an input or output file.
If you have a look at the control file now you will see that the content is 
a little bit confusing, but it includes all information, like the nodes, 
fields and parser, that graphviz needs to create the graph. Of course you 
can edit that file, but please note that it is a lot of work.

Now we can create the graph by typing::

    $ dot control.dot -Tpng >graph.png

dot + name of control file + option -T -> file format + output file

That is just one example for using graphviz, of course you can do many 
other great things with it. But I think this "simple" graph could be very 
helpful for the normalizer.

Below you see sample for such a graph, but please note that this is 
not such a pretty one. Such a graph can grow very fast by editing your 
rulebase.

.. figure:: graph.png
   :width: 90 %
   :alt: graph sample
