#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule WorkflowSelfcheck do
  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        artifacts_root: :string,
        keep_artifacts: :boolean
      )

    repo_root = Path.expand("..", __DIR__)
    host_os = :os.type()

    artifacts_root =
      CortTooling.string_flag(options, :artifacts_root) ||
        Path.join(System.tmp_dir!(), "cort-workflow-selfcheck-#{System.system_time(:millisecond)}")

    keep_artifacts = Keyword.get(options, :keep_artifacts, false)

    File.rm_rf!(artifacts_root)
    File.mkdir_p!(artifacts_root)

    IO.puts("Workflow selfcheck artifact root: #{artifacts_root}")
    maybe_print_host_note(host_os)

    checks(repo_root, artifacts_root, host_os)
    |> Enum.each(fn {title, fun} ->
      IO.puts("==> #{title}")
      fun.()
      IO.puts("ok: #{title}")
    end)

    unless keep_artifacts do
      File.rm_rf!(artifacts_root)
    end

    IO.puts("workflow selfcheck passed")
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      IO.puts(:stderr, Exception.message(error))
      System.halt(1)
  end

  defp checks(repo_root, artifacts_root, host_os) do
    base_checks = [
      {"shell syntax and execute bits", fn -> shell_syntax_check!(repo_root) end},
      {"elixir launcher version", fn -> elixir_version_check!(repo_root) end},
      {"subset0 runtime manifest report", fn -> subset0_runtime_report_check!(repo_root) end},
      {"subset1 manifest report", fn -> subset1_manifest_report_check!(repo_root) end},
      {"subset1b manifest report", fn -> subset1b_manifest_report_check!(repo_root) end},
      {"subset2a manifest report", fn -> subset2a_manifest_report_check!(repo_root) end},
      {"subset3a manifest report", fn -> subset3a_manifest_report_check!(repo_root) end},
      {"subset0 compare tool", fn -> subset0_compare_check!(repo_root) end},
      {"subset1 compare tool", fn -> subset1_compare_check!(repo_root) end},
      {"subset1b compare tool", fn -> subset1b_compare_check!(repo_root) end},
      {"subset2a compare tool", fn -> subset2a_compare_check!(repo_root) end},
      {"subset3a compare tool", fn -> subset3a_compare_check!(repo_root) end},
      {"subset7a expected builder", fn -> subset7a_expected_builder_check!(repo_root, artifacts_root) end},
      {"subset7a cases header builder", fn -> subset7a_cases_header_builder_check!(repo_root, artifacts_root) end},
      {"subset7a compare tool", fn -> subset7a_compare_check!(repo_root) end},
      {"subset1 MX compare artifact wrapper", fn -> subset1_mx_compare_artifact_check!(repo_root, artifacts_root) end},
      {"subset1b MX compare artifact wrapper", fn -> subset1b_mx_compare_artifact_check!(repo_root, artifacts_root) end},
      {"subset2a MX compare artifact wrapper", fn -> subset2a_mx_compare_artifact_check!(repo_root, artifacts_root) end},
      {"subset3a MX compare artifact wrapper", fn -> subset3a_mx_compare_artifact_check!(repo_root, artifacts_root) end},
      {"subset1 FX test target", fn -> subset1_fx_test_target_check!(repo_root, artifacts_root) end},
      {"shared FX handoff artifacts", fn -> shared_fx_handoff_artifacts_check!(repo_root, artifacts_root) end},
      {"subset1 FX installed target", fn -> subset1_fx_test_installed_check!(repo_root, artifacts_root) end},
      {"subset1 FX make compare wrapper", fn -> subset1_make_compare_check!(repo_root, artifacts_root) end},
      {"subset1b FX make compare wrapper", fn -> subset1b_make_compare_check!(repo_root, artifacts_root) end},
      {"subset2a FX make compare wrapper", fn -> subset2a_make_compare_check!(repo_root, artifacts_root) end},
      {"subset7a FX make compare wrapper", fn -> subset7a_make_compare_check!(repo_root, artifacts_root) end},
      {"subset3a FX make compare wrapper", fn -> subset3a_make_compare_check!(repo_root, artifacts_root) end},
      {"subset1 FX compare artifact wrapper", fn -> subset1_compare_artifact_check!(repo_root, artifacts_root) end},
      {"subset2a FX compare artifact wrapper", fn -> subset2a_compare_artifact_check!(repo_root, artifacts_root) end},
      {"subset3a FX compare artifact wrapper", fn -> subset3a_compare_artifact_check!(repo_root, artifacts_root) end}
    ]

    mx_checks =
      if darwin_host?(host_os) do
        [
          {"subset0 MX suite script", fn -> subset0_mx_suite_check!(repo_root, artifacts_root) end},
          {"subset1 MX scalar-core script", fn -> subset1_mx_scalar_core_check!(repo_root, artifacts_root) end},
          {"subset1 MX suite script", fn -> subset1_mx_suite_check!(repo_root, artifacts_root) end},
          {"subset1b MX CFString script", fn -> subset1b_mx_cfstring_check!(repo_root, artifacts_root) end},
          {"subset1b MX suite script", fn -> subset1b_mx_suite_check!(repo_root, artifacts_root) end},
          {"subset2a MX container script", fn -> subset2a_mx_container_check!(repo_root, artifacts_root) end},
          {"subset2a MX suite script", fn -> subset2a_mx_suite_check!(repo_root, artifacts_root) end},
          {"subset3a MX bplist script", fn -> subset3a_mx_bplist_check!(repo_root, artifacts_root) end},
          {"subset3a MX suite script", fn -> subset3a_mx_suite_check!(repo_root, artifacts_root) end}
        ]
      else
        []
      end

    List.insert_at(base_checks, 6, mx_checks)
    |> List.flatten()
  end

  defp shell_syntax_check!(repo_root) do
    scripts = [
      "tools/run_elixir.sh",
      "cort-mx/scripts/run_subset0_runtime_ownership.sh",
      "cort-mx/scripts/run_subset0_public_allocator_compare.sh",
      "cort-mx/scripts/run_subset0_suite.sh",
      "cort-mx/scripts/run_subset1_scalar_core.sh",
      "cort-mx/scripts/run_subset1_scalar_core_compare.sh",
      "cort-mx/scripts/run_subset1_suite.sh",
      "cort-mx/scripts/run_subset1b_cfstring_core.sh",
      "cort-mx/scripts/run_subset1b_cfstring_compare.sh",
      "cort-mx/scripts/run_subset1b_suite.sh",
      "cort-mx/scripts/run_subset2a_container_core.sh",
      "cort-mx/scripts/run_subset2a_container_compare.sh",
      "cort-mx/scripts/run_subset2a_suite.sh",
      "cort-mx/scripts/run_subset3a_bplist_core.sh",
      "cort-mx/scripts/run_subset3a_bplist_compare.sh",
      "cort-mx/scripts/run_subset3a_suite.sh",
      "tools/sync_subset7a_control_corpora.sh",
      "cort-fx/scripts/run_subset0_fx_artifacts.sh",
      "cort-fx/scripts/run_subset1a_compare_artifact.sh",
      "cort-fx/scripts/run_subset2a_compare_artifact.sh",
      "cort-fx/scripts/run_subset3a_compare_artifact.sh",
      "cort-fx/scripts/publish_shared_json.sh"
    ]

    Enum.each(scripts, fn script ->
      path = Path.join(repo_root, script)
      run_cmd!("sh", ["-n", path], cd: repo_root, expect_exit: 0)
      run_cmd!("test", ["-x", path], cd: repo_root, expect_exit: 0)
    end)
  end

  defp elixir_version_check!(repo_root) do
    output = run_cmd!(Path.join(repo_root, "tools/run_elixir.sh"), ["--version"], cd: repo_root, expect_exit: 0)
    ensure_contains!(output, "Elixir ")
    ensure_contains!(output, "Erlang/OTP ")
  end

  defp subset0_runtime_report_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/report_subset0_runtime_ownership.exs"),
          "--json",
          Path.join(repo_root, "cort-mx/fixtures/subset0_runtime_ownership_sample.json"),
          "--json-label",
          "out/subset0_runtime_ownership.json",
          "--expected",
          Path.join(repo_root, "cort-mx/expectations/subset0_runtime_ownership_expected.json"),
          "--expected-label",
          "expectations/subset0_runtime_ownership_expected.json"
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
  end

  defp subset1_manifest_report_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/report_case_manifest.exs"),
          "--title",
          "Subset 1A Scalar Core Report",
          "--json",
          Path.join(repo_root, "cort-mx/fixtures/subset1_scalar_core_sample.json"),
          "--json-label",
          "out/subset1_scalar_core.json",
          "--expected",
          Path.join(repo_root, "cort-mx/expectations/subset1_scalar_core_expected.json"),
          "--expected-label",
          "expectations/subset1_scalar_core_expected.json"
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
  end

  defp subset1b_manifest_report_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/report_case_manifest.exs"),
          "--title",
          "Subset 1B Minimal CFString Report",
          "--json",
          Path.join(repo_root, "cort-mx/fixtures/subset1b_cfstring_core_sample.json"),
          "--json-label",
          "out/subset1b_cfstring_core.json",
          "--expected",
          Path.join(repo_root, "cort-mx/expectations/subset1b_cfstring_core_expected.json"),
          "--expected-label",
          "expectations/subset1b_cfstring_core_expected.json"
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
  end

  defp subset2a_manifest_report_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/report_case_manifest.exs"),
          "--title",
          "Subset 2A Container Core Report",
          "--json",
          Path.join(repo_root, "cort-mx/fixtures/subset2a_container_core_sample.json"),
          "--json-label",
          "out/subset2a_container_core.json",
          "--expected",
          Path.join(repo_root, "cort-mx/expectations/subset2a_container_core_expected.json"),
          "--expected-label",
          "expectations/subset2a_container_core_expected.json"
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
  end

  defp subset3a_manifest_report_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/report_case_manifest.exs"),
          "--title",
          "Subset 3A Binary Plist Core Report",
          "--json",
          Path.join(repo_root, "cort-mx/fixtures/subset3a_bplist_core_sample.json"),
          "--json-label",
          "out/subset3a_bplist_core.json",
          "--expected",
          Path.join(repo_root, "cort-mx/expectations/subset3a_bplist_core_expected.json"),
          "--expected-label",
          "expectations/subset3a_bplist_core_expected.json"
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
  end

  defp subset0_compare_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/compare_subset0_json.exs"),
          "--fx-json",
          Path.join(repo_root, "tools/fixtures/subset0_public_compare_fx_sample.json"),
          "--mx-json",
          Path.join(repo_root, "tools/fixtures/subset0_public_compare_mx_sample.json")
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(output, "allocator_singleton_retain_identity")
  end

  defp subset1_compare_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/compare_subset1_scalar_core_json.exs"),
          "--fx-json",
          Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_fx_sample.json"),
          "--mx-json",
          Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_mx_sample.json")
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(output, "`cfnumber_cross_type_equality` | match")
  end

  defp subset1b_compare_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/compare_subset1b_cfstring_json.exs"),
          "--fx-json",
          Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_fx_sample.json"),
          "--mx-json",
          Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_mx_sample.json")
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(output, "`cfstring_bytes_invalid_utf8_rejected` | match")
  end

  defp subset2a_compare_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/compare_subset2a_container_json.exs"),
          "--fx-json",
          Path.join(repo_root, "tools/fixtures/subset2a_container_compare_fx_sample.json"),
          "--mx-json",
          Path.join(repo_root, "tools/fixtures/subset2a_container_compare_mx_sample.json")
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(output, "`cfdictionary_mutable_set_replace_remove_retains` | match")
  end

  defp subset3a_compare_check!(repo_root) do
    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/compare_subset3a_bplist_json.exs"),
          "--fx-json",
          Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_fx_sample.json"),
          "--mx-json",
          Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_mx_sample.json")
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(output, "`bplist_truncated_trailer_rejected` | match")
  end

  defp subset7a_expected_builder_check!(repo_root, artifacts_root) do
    output_path = Path.join([artifacts_root, "subset7a_control_packet_expected_v1.json"])
    expected_path = Path.join(repo_root, "fixtures/control/subset7a_control_packet_expected_v1.json")

    run_cmd!(
      Path.join(repo_root, "tools/run_elixir.sh"),
      [
        Path.join(repo_root, "tools/build_subset7a_control_packet_expected.exs"),
        "--accept-source",
        Path.join(repo_root, "fixtures/control/bplist_packet_corpus_v1.source.json"),
        "--reject-source",
        Path.join(repo_root, "fixtures/control/bplist_packet_rejection_corpus_v1.source.json"),
        "--output",
        output_path
      ],
      cd: repo_root,
      expect_exit: 0
    )

    run_cmd!("diff", ["-u", expected_path, output_path], cd: repo_root, expect_exit: 0)
  end

  defp subset7a_cases_header_builder_check!(repo_root, artifacts_root) do
    output_path = Path.join([artifacts_root, "subset7a_control_packet_cases.h"])
    expected_path = Path.join(repo_root, "cort-fx/tests/generated/subset7a_control_packet_cases.h")

    run_cmd!(
      Path.join(repo_root, "tools/run_elixir.sh"),
      [
        Path.join(repo_root, "tools/build_subset7a_control_packet_cases_header.exs"),
        "--accept-payload",
        Path.join(repo_root, "fixtures/control/bplist_packet_corpus_v1.json"),
        "--accept-source",
        Path.join(repo_root, "fixtures/control/bplist_packet_corpus_v1.source.json"),
        "--reject-payload",
        Path.join(repo_root, "fixtures/control/bplist_packet_rejection_corpus_v1.json"),
        "--expected",
        Path.join(repo_root, "fixtures/control/subset7a_control_packet_expected_v1.json"),
        "--output",
        output_path
      ],
      cd: repo_root,
      expect_exit: 0
    )

    run_cmd!("diff", ["-u", expected_path, output_path], cd: repo_root, expect_exit: 0)
  end

  defp subset7a_compare_check!(repo_root) do
    expected_path = Path.join(repo_root, "fixtures/control/subset7a_control_packet_expected_v1.json")

    output =
      run_cmd!(
        Path.join(repo_root, "tools/run_elixir.sh"),
        [
          Path.join(repo_root, "tools/compare_subset7a_control_packet_json.exs"),
          "--fx-json",
          expected_path,
          "--expected-json",
          expected_path
        ],
        cd: repo_root,
        expect_exit: 0
      )

    ensure_contains!(output, "- blockers: 0")
    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(output, "`request.control.health` | match")
  end

  defp subset1_mx_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset1-mx-scalar-core-compare"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset1_scalar_core_compare.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_fx_sample.json")},
        {"MX_JSON", Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_mx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset1_scalar_core_fx.json"]),
          Path.join([run_dir, "out", "subset1_scalar_core_mx.json"]),
          Path.join([run_dir, "out", "subset1a_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset1b_mx_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset1b-mx-cfstring-core-compare"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset1b_cfstring_compare.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_fx_sample.json")},
        {"MX_JSON", Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_mx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset1b_cfstring_fx.json"]),
          Path.join([run_dir, "out", "subset1b_cfstring_mx.json"]),
          Path.join([run_dir, "out", "subset1b_cfstring_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 1B compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset2a_mx_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset2a-mx-container-core-compare"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset2a_container_compare.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset2a_container_compare_fx_sample.json")},
        {"MX_JSON", Path.join(repo_root, "tools/fixtures/subset2a_container_compare_mx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset2a_container_fx.json"]),
          Path.join([run_dir, "out", "subset2a_container_mx.json"]),
          Path.join([run_dir, "out", "subset2a_container_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 2A compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset3a_mx_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset3a-mx-bplist-core-compare"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset3a_bplist_compare.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_fx_sample.json")},
        {"MX_JSON", Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_mx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset3a_bplist_fx.json"]),
          Path.join([run_dir, "out", "subset3a_bplist_mx.json"]),
          Path.join([run_dir, "out", "subset3a_bplist_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 3A compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end
  defp subset1_fx_test_target_check!(repo_root, artifacts_root) do
    output =
      run_cmd!(
        "make",
        ["clean", "test"],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    for path <- [
          Path.join([artifacts_root, "cort-fx", "build", "out", "subset0_public_compare_fx.json"]),
          Path.join([artifacts_root, "cort-fx", "build", "out", "subset1_scalar_core_fx.json"]),
          Path.join([artifacts_root, "cort-fx", "build", "out", "subset1b_cfstring_fx.json"]),
          Path.join([artifacts_root, "cort-fx", "build", "out", "subset2a_container_fx.json"]),
          Path.join([artifacts_root, "cort-fx", "build", "out", "subset3a_bplist_fx.json"]),
          Path.join([artifacts_root, "cort-fx", "build", "out", "subset7a_control_packet_fx.json"]),
          Path.join([artifacts_root, "cort-fx", "build", "subset3a-exported-symbols.txt"])
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing FX test output at #{path}"
      end
    end

    ensure_contains!(output, "PASS runtime_ownership_tests")
    ensure_contains!(output, "PASS runtime_abort_tests")
    ensure_contains!(output, "PASS scalar_core_tests")
    ensure_contains!(output, "PASS string_core_tests")
    ensure_contains!(output, "PASS container_core_tests")
    ensure_contains!(output, "PASS bplist_core_tests")
    ensure_contains!(output, "PASS control_packet_tests")
    ensure_contains!(output, "PASS c_consumer_smoke")
  end

  defp subset1_fx_test_installed_check!(repo_root, artifacts_root) do
    output =
      run_cmd!(
        "make",
        ["test-installed"],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    ensure_contains!(output, "PASS c_consumer_smoke")
  end

  defp shared_fx_handoff_artifacts_check!(repo_root, artifacts_root) do
    shared_pairs = [
      {"subset0_public_compare_fx.json", Path.join([artifacts_root, "cort-fx", "build", "out", "subset0_public_compare_fx.json"])},
      {"subset1_scalar_core_fx.json", Path.join([artifacts_root, "cort-fx", "build", "out", "subset1_scalar_core_fx.json"])},
      {"subset1b_cfstring_fx.json", Path.join([artifacts_root, "cort-fx", "build", "out", "subset1b_cfstring_fx.json"])},
      {"subset2a_container_fx.json", Path.join([artifacts_root, "cort-fx", "build", "out", "subset2a_container_fx.json"])},
      {"subset3a_bplist_fx.json", Path.join([artifacts_root, "cort-fx", "build", "out", "subset3a_bplist_fx.json"])}
    ]

    Enum.each(shared_pairs, fn {shared_rel, built_path} ->
      shared_path = Path.join(repo_root, shared_rel)

      unless File.exists?(shared_path) do
        raise RuntimeError, "missing shared FX handoff artifact at #{shared_path}"
      end

      unless File.exists?(built_path) do
        raise RuntimeError, "missing freshly built FX JSON at #{built_path}"
      end

      if File.read!(shared_path) != File.read!(built_path) do
        raise RuntimeError,
              "shared FX handoff artifact #{shared_rel} is out of sync with the current build output; run `make -C cort-fx publish-shared-artifacts`"
      end
    end)
  end

  defp subset0_mx_suite_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset0-mx-suite"])
    summary_path = Path.join(run_dir, "summary.md")
    runtime_summary_path = Path.join([run_dir, "runtime-ownership", "summary.md"])
    runtime_json_path = Path.join([run_dir, "runtime-ownership", "out", "subset0_runtime_ownership.json"])
    allocator_summary_path = Path.join([run_dir, "public-allocator-compare", "summary.md"])
    allocator_json_path = Path.join([run_dir, "public-allocator-compare", "out", "subset0_public_compare_mx.json"])
    allocator_report_path = Path.join([run_dir, "public-allocator-compare", "out", "subset0_public_compare_report.md"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset0_suite.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset0_public_compare_fx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          runtime_summary_path,
          runtime_json_path,
          allocator_summary_path,
          allocator_json_path,
          allocator_report_path
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX subset0 suite output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- runtime-ownership exit status: `0`")
    ensure_contains!(File.read!(summary_path), "- public-allocator-compare exit status: `0`")
    ensure_contains!(File.read!(runtime_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(runtime_summary_path), "- warnings: 0")
    ensure_contains!(File.read!(allocator_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(allocator_summary_path), "- warnings: 0")
  end

  defp subset1_mx_scalar_core_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset1-mx-scalar-core"])
    summary_path = Path.join(run_dir, "summary.md")
    json_path = Path.join([run_dir, "out", "subset1_scalar_core.json"])
    stderr_path = Path.join([run_dir, "out", "report.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset1_scalar_core.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [summary_path, json_path, stderr_path] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX subset1 scalar-core output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- blockers: 0")
    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset1_mx_suite_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset1-mx-suite"])
    suite_summary_path = Path.join(run_dir, "summary.md")
    scalar_summary_path = Path.join([run_dir, "scalar-core", "summary.md"])
    scalar_json_path = Path.join([run_dir, "scalar-core", "out", "subset1_scalar_core.json"])
    compare_summary_path = Path.join([run_dir, "scalar-core-compare", "summary.md"])
    compare_report_path = Path.join([run_dir, "scalar-core-compare", "out", "subset1a_fx_vs_mx_report.md"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset1_suite.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [
          suite_summary_path,
          scalar_summary_path,
          scalar_json_path,
          compare_summary_path,
          compare_report_path
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX subset1 suite output at #{path}"
      end
    end

    ensure_contains!(File.read!(suite_summary_path), "- scalar-core exit status: `0`")
    ensure_contains!(File.read!(suite_summary_path), "- scalar-core-compare exit status: `0`")
    ensure_contains!(File.read!(scalar_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(scalar_summary_path), "- warnings: 0")
    ensure_contains!(File.read!(compare_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(compare_summary_path), "- warnings: 0")
  end

  defp subset1b_mx_cfstring_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset1b-mx-cfstring-core"])
    summary_path = Path.join(run_dir, "summary.md")
    json_path = Path.join([run_dir, "out", "subset1b_cfstring_core.json"])
    stderr_path = Path.join([run_dir, "out", "report.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset1b_cfstring_core.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [summary_path, json_path, stderr_path] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX subset1b CFString output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- blockers: 0")
    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset1b_mx_suite_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset1b-mx-suite"])
    suite_summary_path = Path.join(run_dir, "summary.md")
    cfstring_summary_path = Path.join([run_dir, "cfstring-core", "summary.md"])
    cfstring_json_path = Path.join([run_dir, "cfstring-core", "out", "subset1b_cfstring_core.json"])
    compare_summary_path = Path.join([run_dir, "cfstring-core-compare", "summary.md"])
    compare_report_path = Path.join([run_dir, "cfstring-core-compare", "out", "subset1b_cfstring_fx_vs_mx_report.md"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset1b_suite.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_fx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          suite_summary_path,
          cfstring_summary_path,
          cfstring_json_path,
          compare_summary_path,
          compare_report_path
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 1B suite output at #{path}"
      end
    end

    ensure_contains!(File.read!(suite_summary_path), "- CFString core exit status: `0`")
    ensure_contains!(File.read!(suite_summary_path), "- CFString compare exit status: `0`")
    ensure_contains!(File.read!(cfstring_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(cfstring_summary_path), "- warnings: 0")
    ensure_contains!(File.read!(compare_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(compare_summary_path), "- warnings: 0")
  end

  defp subset2a_mx_container_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset2a-mx-container-core"])
    summary_path = Path.join(run_dir, "summary.md")
    json_path = Path.join([run_dir, "out", "subset2a_container_core.json"])
    stderr_path = Path.join([run_dir, "out", "report.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset2a_container_core.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [summary_path, json_path, stderr_path] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX subset2a container output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- blockers: 0")
    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset2a_mx_suite_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset2a-mx-suite"])
    suite_summary_path = Path.join(run_dir, "summary.md")
    container_summary_path = Path.join([run_dir, "container-core", "summary.md"])
    container_json_path = Path.join([run_dir, "container-core", "out", "subset2a_container_core.json"])
    compare_summary_path = Path.join([run_dir, "container-core-compare", "summary.md"])
    compare_report_path = Path.join([run_dir, "container-core-compare", "out", "subset2a_container_fx_vs_mx_report.md"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset2a_suite.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset2a_container_compare_fx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          suite_summary_path,
          container_summary_path,
          container_json_path,
          compare_summary_path,
          compare_report_path
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 2A suite output at #{path}"
      end
    end

    ensure_contains!(File.read!(suite_summary_path), "- container-core exit status: `0`")
    ensure_contains!(File.read!(suite_summary_path), "- container-core-compare exit status: `0`")
    ensure_contains!(File.read!(container_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(container_summary_path), "- warnings: 0")
    ensure_contains!(File.read!(compare_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(compare_summary_path), "- warnings: 0")
  end

  defp subset3a_mx_bplist_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset3a-mx-bplist-core"])
    summary_path = Path.join(run_dir, "summary.md")
    json_path = Path.join([run_dir, "out", "subset3a_bplist_core.json"])
    stderr_path = Path.join([run_dir, "out", "report.stderr"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset3a_bplist_core.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [summary_path, json_path, stderr_path] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 3A bplist output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- blockers: 0")
    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset3a_mx_suite_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-mx", "runs", "subset3a-mx-suite"])
    suite_summary_path = Path.join(run_dir, "summary.md")
    bplist_summary_path = Path.join([run_dir, "bplist-core", "summary.md"])
    bplist_json_path = Path.join([run_dir, "bplist-core", "out", "subset3a_bplist_core.json"])
    compare_summary_path = Path.join([run_dir, "bplist-core-compare", "summary.md"])
    compare_report_path = Path.join([run_dir, "bplist-core-compare", "out", "subset3a_bplist_fx_vs_mx_report.md"])

    run_cmd!(
      Path.join(repo_root, "cort-mx/scripts/run_subset3a_suite.sh"),
      [],
      cd: Path.join(repo_root, "cort-mx"),
      env: [
        {"CORT_ARTIFACTS_ROOT", artifacts_root},
        {"FX_JSON", Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_fx_sample.json")}
      ],
      expect_exit: 0
    )

    for path <- [
          suite_summary_path,
          bplist_summary_path,
          bplist_json_path,
          compare_summary_path,
          compare_report_path
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing MX Subset 3A suite output at #{path}"
      end
    end

    ensure_contains!(File.read!(suite_summary_path), "- bplist-core exit status: `0`")
    ensure_contains!(File.read!(suite_summary_path), "- bplist-core-compare exit status: `0`")
    ensure_contains!(File.read!(bplist_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(bplist_summary_path), "- warnings: 0")
    ensure_contains!(File.read!(compare_summary_path), "- blockers: 0")
    ensure_contains!(File.read!(compare_summary_path), "- warnings: 0")
  end
  defp subset1_make_compare_check!(repo_root, artifacts_root) do
    report_path = Path.join([artifacts_root, "cort-fx", "build", "out", "subset1_scalar_core_fx_vs_mx_report.md"])

    output =
      run_cmd!(
        "make",
        [
          "compare-subset1a-with-mx",
          "FX_SCALAR_JSON=#{Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_fx_sample.json")}",
          "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_mx_sample.json")}"
        ],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    unless File.exists?(report_path) do
      raise RuntimeError, "missing compare report at #{report_path}"
    end

    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(File.read!(report_path), "`cfnumber_cross_type_equality` | match")
  end

  defp subset1_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-fx", "runs", "subset1a-fx-vs-mx"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      "make",
      [
        "artifact-subset1a-compare",
        "FX_SCALAR_JSON=#{Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_fx_sample.json")}",
        "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset1_scalar_core_compare_mx_sample.json")}"
      ],
      cd: Path.join(repo_root, "cort-fx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset1_scalar_core_fx.json"]),
          Path.join([run_dir, "out", "subset1_scalar_core_mx.json"]),
          Path.join([run_dir, "out", "subset1a_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset1b_make_compare_check!(repo_root, artifacts_root) do
    report_path = Path.join([artifacts_root, "cort-fx", "build", "out", "subset1b_cfstring_fx_vs_mx_report.md"])

    output =
      run_cmd!(
        "make",
        [
          "compare-subset1b-with-mx",
          "FX_STRING_JSON=#{Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_fx_sample.json")}",
          "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset1b_cfstring_compare_mx_sample.json")}"
        ],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    unless File.exists?(report_path) do
      raise RuntimeError, "missing compare report at #{report_path}"
    end

    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(File.read!(report_path), "`cfstring_cstring_invalid_utf8_rejected` | match")
  end

  defp subset2a_make_compare_check!(repo_root, artifacts_root) do
    report_path = Path.join([artifacts_root, "cort-fx", "build", "out", "subset2a_container_fx_vs_mx_report.md"])

    output =
      run_cmd!(
        "make",
        [
          "compare-subset2a-with-mx",
          "FX_CONTAINER_JSON=#{Path.join(repo_root, "tools/fixtures/subset2a_container_compare_fx_sample.json")}",
          "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset2a_container_compare_mx_sample.json")}"
        ],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    unless File.exists?(report_path) do
      raise RuntimeError, "missing compare report at #{report_path}"
    end

    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(File.read!(report_path), "`cfarray_createcopy_borrowed_get` | match")
  end

  defp subset7a_make_compare_check!(repo_root, artifacts_root) do
    json_path = Path.join([artifacts_root, "cort-fx", "build", "out", "subset7a_control_packet_fx.json"])
    report_path = Path.join([artifacts_root, "cort-fx", "build", "out", "subset7a_control_packet_fx_vs_expected_report.md"])

    output =
      run_cmd!(
        "make",
        [
          "compare-subset7a-with-expected",
          "EXPECTED_JSON=#{Path.join(repo_root, "fixtures/control/subset7a_control_packet_expected_v1.json")}"
        ],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    unless File.exists?(json_path) do
      raise RuntimeError, "missing packet compare input at #{json_path}"
    end

    unless File.exists?(report_path) do
      raise RuntimeError, "missing compare report at #{report_path}"
    end

    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(File.read!(report_path), "`request.control.health` | match")
  end

  defp subset2a_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-fx", "runs", "subset2a-fx-vs-mx"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      "make",
      [
        "artifact-subset2a-compare",
        "FX_CONTAINER_JSON=#{Path.join(repo_root, "tools/fixtures/subset2a_container_compare_fx_sample.json")}",
        "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset2a_container_compare_mx_sample.json")}"
      ],
      cd: Path.join(repo_root, "cort-fx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset2a_container_fx.json"]),
          Path.join([run_dir, "out", "subset2a_container_mx.json"]),
          Path.join([run_dir, "out", "subset2a_container_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp subset3a_make_compare_check!(repo_root, artifacts_root) do
    report_path = Path.join([artifacts_root, "cort-fx", "build", "out", "subset3a_bplist_fx_vs_mx_report.md"])

    output =
      run_cmd!(
        "make",
        [
          "compare-subset3a-with-mx",
          "FX_BPLIST_JSON=#{Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_fx_sample.json")}",
          "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_mx_sample.json")}"
        ],
        cd: Path.join(repo_root, "cort-fx"),
        env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
        expect_exit: 0
      )

    unless File.exists?(report_path) do
      raise RuntimeError, "missing compare report at #{report_path}"
    end

    ensure_contains!(output, "- warnings: 0")
    ensure_contains!(File.read!(report_path), "`bplist_string_key_dictionary_roundtrip` | match")
  end

  defp subset3a_compare_artifact_check!(repo_root, artifacts_root) do
    run_dir = Path.join([artifacts_root, "cort-fx", "runs", "subset3a-fx-vs-mx"])
    summary_path = Path.join(run_dir, "summary.md")
    stderr_path = Path.join([run_dir, "out", "compare.stderr"])

    run_cmd!(
      "make",
      [
        "artifact-subset3a-compare",
        "FX_BPLIST_JSON=#{Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_fx_sample.json")}",
        "MX_JSON=#{Path.join(repo_root, "tools/fixtures/subset3a_bplist_compare_mx_sample.json")}"
      ],
      cd: Path.join(repo_root, "cort-fx"),
      env: [{"CORT_ARTIFACTS_ROOT", artifacts_root}],
      expect_exit: 0
    )

    for path <- [
          summary_path,
          Path.join(run_dir, "commands.txt"),
          Path.join(run_dir, "host.txt"),
          Path.join(run_dir, "toolchain.txt"),
          Path.join([run_dir, "out", "subset3a_bplist_fx.json"]),
          Path.join([run_dir, "out", "subset3a_bplist_mx.json"]),
          Path.join([run_dir, "out", "subset3a_bplist_fx_vs_mx_report.md"]),
          Path.join(run_dir, "sha256.txt")
        ] do
      unless File.exists?(path) do
        raise RuntimeError, "missing compare artifact output at #{path}"
      end
    end

    ensure_contains!(File.read!(summary_path), "- warnings: 0")
    ensure_empty_or_whitespace!(File.read!(stderr_path), stderr_path)
  end

  defp run_cmd!(command, args, opts) do
    cd = Keyword.fetch!(opts, :cd)
    env = Keyword.get(opts, :env, [])
    expected_exit = Keyword.get(opts, :expect_exit, 0)

    {output, status} =
      System.cmd(command, args,
        cd: cd,
        env: env,
        stderr_to_stdout: true
      )

    if status != expected_exit do
      raise RuntimeError,
            "command failed (exit #{status}): #{command} #{Enum.join(args, " ")}\n#{output}"
    end

    output
  end

  defp ensure_contains!(text, snippet) do
    unless String.contains?(text, snippet) do
      raise RuntimeError, "expected output to contain #{inspect(snippet)}\n#{text}"
    end
  end

  defp ensure_empty_or_whitespace!(text, path) do
    unless String.trim(text) == "" do
      raise RuntimeError, "expected #{path} to be empty, got:\n#{text}"
    end
  end

  defp maybe_print_host_note(host_os) do
    unless darwin_host?(host_os) do
      IO.puts("note: skipping macOS-only MX script checks on #{host_label(host_os)}")
    end
  end

  defp darwin_host?(host_os), do: host_os == {:unix, :darwin}

  defp host_label({family, name}), do: "#{family}/#{name}"
end

WorkflowSelfcheck.main(System.argv())
