Liblognorm Architecture
=======================

Introduction
------------

Liblognorm is designed for high-performance log normalization. Its core architecture relies on a concept called the **Parse DAG (PDAG)**.

This document provides a high-level overview of the architecture. For detailed implementation notes (including how the C code corresponds to the theoretical model), please refer to :doc:`pdag_implementation_model`.

The Parse DAG (PDAG)
--------------------

Unlike traditional regex-based parsers that evaluate rules sequentially, liblognorm compiles all rules into a single directed acyclic graph (DAG). This approach offers several advantages:

1.  **Speed**: Parsing performance is roughly proportional to the length of the log message, not the number of rules. Adding more rules does not significantly slow down the normalizer.
2.  **Prefix Sharing**: Common prefixes in log messages are shared in the graph. If ten rules start with the same timestamp format, the normalizer only parses that timestamp once.
3.  **Ambiguity Handling**: The graph structure allows for controlled priority handling when multiple rules could potentially match a message.

How it Works
------------

1.  **Loading**: When you load a rulebase, liblognorm parses the rule strings and builds the PDAG in memory.
2.  **Optimization**: The library optimizes the graph, compressing literal strings and organizing edges for efficient traversal.
3.  **Normalization**: When a log message arrives:
    *   The normalizer starts at the root of the PDAG.
    *   It traverses the graph by matching "motifs" (parsers like `word`, `number`, or literal strings) against the message.
    *   If it reaches a "terminal node" (the end of a valid rule), the normalization is successful, and the extracted fields are returned as a JSON object.

Key Concepts
------------

*   **Rulebase**: The collection of all normalization rules.
*   **Motif**: A basic building block of a rule. Examples include:
    *   `literal`: Matches exact text (e.g., "Connection from").
    *   `word`: Matches a sequence of non-whitespace characters.
    *   `ipv4`: Matches an IP address.
*   **Component**: A named subgraph (like a subroutine) that can be reused across different rules.
*   **Sample**: A specific rule definition.

For a mapping of these concepts to the actual source code files, see ``doc/ai_architecture_map.md`` in the source repository.
