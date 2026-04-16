# Agent Rules

This repository uses the following tooling and coordination rules for future
agent work:

- Do not add new Python tooling or new Python workflow code.
- Python may be used only to run existing code that is already present.
- For new higher-level test or workflow code, use Elixir.
- For performance tests, use `swift-testing` when the work belongs on the Swift
  side.
- This repo is also the coordination channel between FX and MX.
- For communication and handoff purposes, it is acceptable to place a small
  file in this repo locally and push it so the other side can pull it.
- Use that communication path deliberately: prefer narrow handoff files or
  shared artifacts, and do not commit large build trees or unrelated generated
  output.

Any remaining Python workflow code should be treated as transitional and should
be removed rather than expanded.
