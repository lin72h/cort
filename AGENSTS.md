# Agent Rules

This repository uses the following tooling rule for future agent work:

- Do not add new Python tooling or new Python workflow code.
- Python may be used only to run existing code that is already present.
- For new higher-level test or workflow code, use Elixir.
- For performance tests, use `swift-testing` when the work belongs on the Swift side.

Existing Python tools in this repo should be treated as transitional and should
not be expanded further.
