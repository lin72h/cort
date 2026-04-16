#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule ReportSubset0RuntimeOwnership do
  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        json: :string,
        expected: :string,
        json_label: :string,
        expected_label: :string,
        output: :string
      )

    actual_path = CortTooling.required_flag!(options, :json)
    expected_path = CortTooling.required_flag!(options, :expected)
    actual_label = CortTooling.string_flag(options, :json_label) || actual_path
    expected_label = CortTooling.string_flag(options, :expected_label) || expected_path
    output_path = CortTooling.string_flag(options, :output)

    {report, blockers, _warnings} = render_report(actual_path, expected_path, actual_label, expected_label)
    CortTooling.render_or_write!(report, output_path)
    System.halt(if blockers == 0, do: 0, else: 1)
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end

  defp render_report(actual_path, expected_path, actual_label, expected_label) do
    expected = CortTooling.read_json!(expected_path)
    expected_probe = Map.get(expected, "probe")
    expected_cases = Map.get(expected, "cases")

    unless is_list(expected_cases) do
      raise ArgumentError, "#{expected_path} does not contain a top-level cases array"
    end

    {actual_probe, actual_results} = load_results!(actual_path)

    initial_lines = [
      "# Subset 0 Runtime Ownership Report",
      "",
      "- actual JSON: `#{actual_label}`",
      "- expected manifest: `#{expected_label}`"
    ]

    lines_with_probe =
      initial_lines
      |> maybe_add_probe_line("- expected probe: `#{expected_probe}`", expected_probe)
      |> maybe_add_probe_line("- actual probe: `#{actual_probe}`", actual_probe)
      |> Kernel.++([""])

    {lines_with_mismatch, initial_warnings} =
      if not is_nil(expected_probe) and not is_nil(actual_probe) and expected_probe != actual_probe do
        {
          lines_with_probe ++
            ["Probe name mismatch: expected `#{expected_probe}`, actual `#{actual_probe}`.", ""],
          1
        }
      else
        {lines_with_probe, 0}
      end

    {rows, expected_names, blockers, warnings} =
      Enum.reduce(expected_cases, {[], MapSet.new(), 0, initial_warnings}, fn expected_case,
                                                                              {acc_rows, acc_names, acc_blockers, acc_warnings} ->
        name = Map.get(expected_case, "name")

        unless is_binary(name) and name != "" do
          raise ArgumentError, "#{expected_path} contains a case without a valid name"
        end

        {line, delta_blockers, delta_warnings} = compare_case(expected_case, Map.get(actual_results, name))

        {
          [line | acc_rows],
          MapSet.put(acc_names, name),
          acc_blockers + delta_blockers,
          acc_warnings + delta_warnings
        }
      end)

    extra_rows =
      actual_results
      |> Map.keys()
      |> Enum.reject(&MapSet.member?(expected_names, &1))
      |> Enum.sort()
      |> Enum.map(fn name -> "| `#{name}` | warning | extra case present in actual results |" end)

    total_warnings = warnings + length(extra_rows)

    verdict =
      if blockers == 0 do
        "- verdict: no blocking issues found against the current Subset 0 runtime-ownership manifest"
      else
        "- verdict: blocking issues found; required runtime-ownership rows need review before advancing"
      end

    report_lines =
      lines_with_mismatch ++
        [
          "| Case | Status | Notes |",
          "| --- | --- | --- |"
        ] ++
        Enum.reverse(rows) ++
        extra_rows ++
        [
          "",
          "- blockers: #{blockers}",
          "- warnings: #{total_warnings}",
          verdict,
          ""
        ]

    {Enum.join(report_lines, "\n"), blockers, total_warnings}
  end

  defp load_results!(path) do
    data = CortTooling.read_json!(path)
    probe = Map.get(data, "probe")
    results = Map.get(data, "results")

    unless is_list(results) do
      raise ArgumentError, "#{path} does not contain a top-level results array"
    end

    by_name =
      Enum.reduce(results, %{}, fn entry, acc ->
        name = Map.get(entry, "name")

        cond do
          not (is_binary(name) and name != "") ->
            raise ArgumentError, "#{path} contains a result without a valid name"

          Map.has_key?(acc, name) ->
            raise ArgumentError, "#{path} contains duplicate result name #{inspect(name)}"

          true ->
            Map.put(acc, name, entry)
        end
      end)

    {if(is_binary(probe), do: probe, else: nil), by_name}
  end

  defp compare_case(expected_case, nil) do
    blocking = truthy?(Map.get(expected_case, "blocking"))

    if blocking do
      {"| `#{Map.get(expected_case, "name")}` | BLOCKER | missing from actual results |", 1, 0}
    else
      {"| `#{Map.get(expected_case, "name")}` | warning | missing from actual results |", 0, 1}
    end
  end

  defp compare_case(expected_case, actual_case) do
    name = Map.get(expected_case, "name")
    classification = Map.get(expected_case, "classification")
    fx_match_level = Map.get(expected_case, "fx_match_level")
    blocking = truthy?(Map.get(expected_case, "blocking"))

    {notes, status, blockers, warnings} =
      {[], "match", 0, 0}
      |> maybe_add_note(
        Map.get(actual_case, "classification") != classification,
        "classification differs: expected #{inspect(classification)}, actual #{inspect(Map.get(actual_case, "classification"))}"
      )
      |> maybe_add_note(
        Map.get(actual_case, "fx_match_level") != fx_match_level,
        "fx_match_level differs: expected #{inspect(fx_match_level)}, actual #{inspect(Map.get(actual_case, "fx_match_level"))}"
      )
      |> maybe_add_success_note(blocking, Map.get(actual_case, "success"))

    final_notes =
      if status == "match" and notes == [] do
        ["present with expected classification and success"]
      else
        notes
      end

    {"| `#{name}` | #{status} | #{Enum.join(final_notes, "; ")} |", blockers, warnings}
  end

  defp maybe_add_note({notes, status, blockers, warnings}, false, _note) do
    {notes, status, blockers, warnings}
  end

  defp maybe_add_note({notes, status, blockers, warnings}, true, note) do
    {
      notes ++ [note],
      if(status == "BLOCKER", do: status, else: "warning"),
      blockers,
      warnings + if(status == "BLOCKER", do: 0, else: 1)
    }
  end

  defp maybe_add_success_note({notes, status, blockers, warnings}, _blocking, success) when success === true do
    {notes, status, blockers, warnings}
  end

  defp maybe_add_success_note({notes, status, blockers, warnings}, true, success) do
    {
      notes ++ ["success is #{inspect(success)}"],
      "BLOCKER",
      blockers + if(status == "BLOCKER", do: 0, else: 1),
      warnings
    }
  end

  defp maybe_add_success_note({notes, status, blockers, warnings}, false, success) do
    {
      notes ++ ["success is #{inspect(success)}"],
      if(status == "BLOCKER", do: status, else: "warning"),
      blockers,
      warnings + if(status in ["warning", "BLOCKER"], do: 0, else: 1)
    }
  end

  defp maybe_add_probe_line(lines, _line, nil), do: lines
  defp maybe_add_probe_line(lines, line, _value), do: lines ++ [line]

  defp truthy?(true), do: true
  defp truthy?("true"), do: true
  defp truthy?(_), do: false
end

ReportSubset0RuntimeOwnership.main(System.argv())
