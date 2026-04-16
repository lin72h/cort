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
- If the user says `do a clean pull`, treat that as an operational instruction
  to fully normalize the repo update flow:
  - try `git pull` first when it can apply cleanly
  - if local changes block the update, run `git pull --rebase --autostash`
  - resolve any autostash conflicts
  - pop or drop any remaining stash entries created by the interrupted
    pull/rebase flow
  - leave the repo in a normal post-pull state, not mid-merge, and report what
    local modifications remain

Any remaining Python workflow code should be treated as transitional and should
be removed rather than expanded.
