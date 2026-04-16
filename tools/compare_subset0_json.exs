#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule CompareSubset0Json do
  @blocking_classifications MapSet.new(["required", "required-but-error-text-flexible"])
  @boolean_semantic_fields ["pointer_identity_same"]
  @note_fields [
    "type_id",
    "retain_count_before",
    "retain_count_after_retain",
    "retain_count_after_release",
    "retain_callback_count",
    "release_callback_count"
  ]

  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        fx_json: :string,
        mx_json: :string,
        fx_label: :string,
        mx_label: :string,
        output: :string
      )

    fx_path = CortTooling.required_flag!(options, :fx_json)
    mx_path = CortTooling.required_flag!(options, :mx_json)
    fx_label = CortTooling.string_flag(options, :fx_label) || fx_path
    mx_label = CortTooling.string_flag(options, :mx_label) || mx_path
    output_path = CortTooling.string_flag(options, :output)

    fx_results = load_results!(fx_path)
    mx_results = load_results!(mx_path)
    {report, blockers, _warnings} = render_report(fx_results, mx_results, fx_label, mx_label)

    CortTooling.render_or_write!(report, output_path)
    System.halt(if blockers == 0, do: 0, else: 1)
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end

  defp load_results!(path) do
    data = CortTooling.read_json!(path)
    results = Map.get(data, "results")

    unless is_list(results) do
      raise ArgumentError, "#{path} does not contain a top-level results array"
    end

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
  end

  defp render_report(fx_results, mx_results, fx_label, mx_label) do
    all_names =
      Map.keys(fx_results)
      |> Enum.concat(Map.keys(mx_results))
      |> Enum.uniq()
      |> Enum.sort()

    initial_lines = [
      "# Subset 0 FX vs MX Comparison",
      "",
      "- FX input: `#{fx_label}`",
      "- MX input: `#{mx_label}`",
      ""
    ]

    if all_names == [] do
      {Enum.join(initial_lines ++ ["No cases were found in either input.", ""], "\n"), 0, 0}
    else
      {rows, blockers, warnings} =
        Enum.reduce(all_names, {[], 0, 0}, fn name, {acc, blocker_count, warning_count} ->
          {line, delta_blockers, delta_warnings} =
            compare_case(name, Map.get(fx_results, name), Map.get(mx_results, name))

          {[line | acc], blocker_count + delta_blockers, warning_count + delta_warnings}
        end)

      verdict =
        if blockers == 0 do
          "- verdict: no blocking mismatches found in the shared comparison surface"
        else
          "- verdict: blocking mismatches found; do not treat FX as semantically aligned for the affected required rows"
        end

      report_lines =
        initial_lines ++
          [
            "| Case | Status | Notes |",
            "| --- | --- | --- |"
          ] ++
          Enum.reverse(rows) ++
          [
            "",
            "- blockers: #{blockers}",
            "- warnings: #{warnings}",
            verdict,
            ""
          ]

      {Enum.join(report_lines, "\n"), blockers, warnings}
    end
  end

  defp compare_case(name, nil, mx_case) do
    missing_case_line(name, "FX", "MX", mx_case)
  end

  defp compare_case(name, fx_case, nil) do
    missing_case_line(name, "MX", "FX", fx_case)
  end

  defp compare_case(name, fx_case, mx_case) do
    fx_class = Map.get(fx_case, "classification")
    mx_class = Map.get(mx_case, "classification")
    blocking_case = blocking?(fx_class) or blocking?(mx_class)

    {notes, status, blockers, warnings} =
      {[], "match", 0, 0}
      |> maybe_add_classification_note(fx_class, mx_class)
      |> maybe_add_success_note(blocking_case, Map.get(fx_case, "success"), Map.get(mx_case, "success"))
      |> maybe_add_boolean_field_notes(blocking_case, fx_case, mx_case)
      |> add_diagnostic_field_notes(fx_case, mx_case)

    final_notes =
      if status == "match" and notes == [] do
        ["semantically aligned on shared blocking fields"]
      else
        notes
      end

    {"| `#{name}` | #{status} | #{Enum.join(final_notes, "; ")} |", blockers, warnings}
  end

  defp missing_case_line(name, missing_side, present_side, present_case) do
    classification =
      if is_map(present_case) do
        Map.get(present_case, "classification")
      else
        nil
      end

    notes = "missing on #{missing_side}; present on #{present_side} as #{classification || "unknown"}"

    if blocking?(classification) do
      {"| `#{name}` | BLOCKER | #{notes} |", 1, 0}
    else
      {"| `#{name}` | warning | #{notes} |", 0, 1}
    end
  end

  defp maybe_add_classification_note({notes, status, blockers, warnings}, fx_class, mx_class) do
    if fx_class != mx_class do
      {
        notes ++ ["classification differs: FX=#{inspect(fx_class)} MX=#{inspect(mx_class)}"],
        if(status == "BLOCKER", do: status, else: "warning"),
        blockers,
        warnings + if(status == "BLOCKER", do: 0, else: 1)
      }
    else
      {notes, status, blockers, warnings}
    end
  end

  defp maybe_add_success_note({notes, status, blockers, warnings}, blocking_case, fx_success, mx_success) do
    if fx_success != mx_success do
      note = "success differs: FX=#{inspect(fx_success)} MX=#{inspect(mx_success)}"

      if blocking_case do
        {
          notes ++ [note],
          "BLOCKER",
          blockers + if(status == "BLOCKER", do: 0, else: 1),
          warnings
        }
      else
        {
          notes ++ [note],
          if(status == "BLOCKER", do: status, else: "warning"),
          blockers,
          warnings + if(status in ["warning", "BLOCKER"], do: 0, else: 1)
        }
      end
    else
      {notes, status, blockers, warnings}
    end
  end

  defp maybe_add_boolean_field_notes({notes, status, blockers, warnings}, blocking_case, fx_case, mx_case) do
    Enum.reduce(@boolean_semantic_fields, {notes, status, blockers, warnings}, fn field, acc ->
      fx_value = Map.get(fx_case, field)
      mx_value = Map.get(mx_case, field)

      if is_nil(fx_value) or is_nil(mx_value) or fx_value == mx_value do
        acc
      else
        {acc_notes, acc_status, acc_blockers, acc_warnings} = acc
        note = "#{field} differs: FX=#{inspect(fx_value)} MX=#{inspect(mx_value)}"

        if blocking_case do
          {
            acc_notes ++ [note],
            "BLOCKER",
            acc_blockers + if(acc_status == "BLOCKER", do: 0, else: 1),
            acc_warnings
          }
        else
          {
            acc_notes ++ [note],
            if(acc_status == "BLOCKER", do: acc_status, else: "warning"),
            acc_blockers,
            acc_warnings + if(acc_status in ["warning", "BLOCKER"], do: 0, else: 1)
          }
        end
      end
    end)
  end

  defp add_diagnostic_field_notes({notes, status, blockers, warnings}, fx_case, mx_case) do
    updated_notes =
      Enum.reduce(@note_fields, notes, fn field, acc ->
        fx_value = Map.get(fx_case, field)
        mx_value = Map.get(mx_case, field)

        if is_nil(fx_value) or is_nil(mx_value) or fx_value == mx_value do
          acc
        else
          acc ++ ["#{field} differs: FX=#{inspect(fx_value)} MX=#{inspect(mx_value)} (diagnostic)"]
        end
      end)

    {updated_notes, status, blockers, warnings}
  end

  defp blocking?(classification), do: MapSet.member?(@blocking_classifications, classification)
end

CompareSubset0Json.main(System.argv())
