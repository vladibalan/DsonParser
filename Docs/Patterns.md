# Doc-system Patterns (framework core, text layer)

Two optional doc-system patterns a project can adopt. A **verbatim framework core**: domain-free
guidance, byte-identical across consumers. Use what you need and ignore the rest.

## Fenced cross-repo intent

When a project's docs must stay agnostic (a public, reusable, or multi-consumer corpus) yet the work is
*motivated* by a specific consumer, keep that motivation in the repo but out of the agnostic docs.
Fence it: the agnostic docs state principles on their own terms and never name a consumer; a single
fenced location (an out-of-scope note, a mapping table) records "this agnostic principle exists because
consumer X needed it". The mapping is discoverable to a maintainer but invisible to the agnostic corpus
- so the corpus never leaks a consumer's name and never bends a principle to one consumer's need.

## Repo-agnostic adoption notes

To export a discipline this project proved to a sibling project, write the note **free of this
project's specifics**: state the discipline, the problem it solves, and how to apply it, naming none of
this repo's files, domains, or types. Such a note is portable governance - the seed of a shared
framework - and drops into another repo without translation. (This is how a framework like this one is
born: a project generalizes its own proven disciplines into repo-agnostic notes, and those notes become
a layer.)
