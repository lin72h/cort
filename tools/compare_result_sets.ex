defmodule CompareResultSets do
  @moduledoc false

  def compare_paths!(left_path, right_path, config) do
    left_results = load_results!(left_path)
    right_results = load_results!(right_path)

    left_label = Keyword.fetch!(config, :left_label)
    right_label = Keyword.fetch!(config, :right_label)

    render_report(left_results, right_results, left_label, right_label, config)
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

  defp render_report(left_results, right_results, left_label, right_label, config) do
    all_names =
      Map.keys(left_results)
      |> Enum.concat(Map.keys(right_results))
      |> Enum.uniq()
      |> Enum.sort()

    title = Keyword.fetch!(config, :title)
    left_heading = Keyword.fetch!(config, :left_heading)
    right_heading = Keyword.fetch!(config, :right_heading)

    initial_lines = [
      "# #{title}",
      "",
      "- #{left_heading}: `#{left_label}`",
      "- #{right_heading}: `#{right_label}`",
      ""
    ]

    if all_names == [] do
      {Enum.join(initial_lines ++ ["No cases were found in either input.", ""], "\n"), 0, 0}
    else
      {rows, blockers, warnings} =
        Enum.reduce(all_names, {[], 0, 0}, fn name, {acc, blocker_count, warning_count} ->
          {line, delta_blockers, delta_warnings} =
            compare_case(name, Map.get(left_results, name), Map.get(right_results, name), config)

          {[line | acc], blocker_count + delta_blockers, warning_count + delta_warnings}
        end)

      verdict =
        if blockers == 0 do
          Keyword.fetch!(config, :success_verdict)
        else
          Keyword.fetch!(config, :failure_verdict)
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

  defp compare_case(name, nil, right_case, config) do
    missing_case_line(name, Keyword.fetch!(config, :left_side), Keyword.fetch!(config, :right_side), right_case, config)
  end

  defp compare_case(name, left_case, nil, config) do
    missing_case_line(name, Keyword.fetch!(config, :right_side), Keyword.fetch!(config, :left_side), left_case, config)
  end

  defp compare_case(name, left_case, right_case, config) do
    left_class = Map.get(left_case, "classification")
    right_class = Map.get(right_case, "classification")
    blocking_case = blocking?(left_class, config) or blocking?(right_class, config)

    {notes, status, blockers, warnings} =
      {[], "match", 0, 0}
      |> maybe_add_classification_note(left_class, right_class)
      |> maybe_add_success_note(blocking_case, Map.get(left_case, "success"), Map.get(right_case, "success"))
      |> maybe_add_field_notes(blocking_case, left_case, right_case, Keyword.get(config, :semantic_boolean_fields, []))
      |> maybe_add_field_notes(blocking_case, left_case, right_case, Keyword.get(config, :semantic_numeric_fields, []))
      |> maybe_add_field_notes(blocking_case, left_case, right_case, Keyword.get(config, :semantic_text_fields, []))
      |> maybe_add_side_invariant_notes(blocking_case, left_case, Keyword.fetch!(config, :left_side), config)
      |> maybe_add_side_invariant_notes(blocking_case, right_case, Keyword.fetch!(config, :right_side), config)
      |> add_diagnostic_field_notes(left_case, right_case, config)

    final_notes =
      if status == "match" and notes == [] do
        ["semantically aligned on shared semantic fields"]
      else
        notes
      end

    {"| `#{name}` | #{status} | #{Enum.join(final_notes, "; ")} |", blockers, warnings}
  end

  defp missing_case_line(name, missing_side, present_side, present_case, config) do
    classification =
      if is_map(present_case) do
        Map.get(present_case, "classification")
      else
        nil
      end

    notes = "missing on #{missing_side}; present on #{present_side} as #{classification || "unknown"}"

    if blocking?(classification, config) do
      {"| `#{name}` | BLOCKER | #{notes} |", 1, 0}
    else
      {"| `#{name}` | warning | #{notes} |", 0, 1}
    end
  end

  defp maybe_add_classification_note({notes, status, blockers, warnings}, left_class, right_class) do
    if left_class != right_class do
      {
        notes ++ ["classification differs: FX=#{inspect(left_class)} MX=#{inspect(right_class)}"],
        if(status == "BLOCKER", do: status, else: "warning"),
        blockers,
        warnings + if(status == "BLOCKER", do: 0, else: 1)
      }
    else
      {notes, status, blockers, warnings}
    end
  end

  defp maybe_add_success_note({notes, status, blockers, warnings}, blocking_case, left_success, right_success) do
    if left_success != right_success do
      note = "success differs: FX=#{inspect(left_success)} MX=#{inspect(right_success)}"

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

  defp maybe_add_field_notes({notes, status, blockers, warnings}, blocking_case, left_case, right_case, fields) do
    Enum.reduce(fields, {notes, status, blockers, warnings}, fn field, acc ->
      left_value = Map.get(left_case, field)
      right_value = Map.get(right_case, field)

      if is_nil(left_value) or is_nil(right_value) or left_value == right_value do
        acc
      else
        {acc_notes, acc_status, acc_blockers, acc_warnings} = acc
        note = "#{field} differs: FX=#{inspect(left_value)} MX=#{inspect(right_value)}"

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

  defp add_diagnostic_field_notes({notes, status, blockers, warnings}, left_case, right_case, config) do
    updated_notes =
      Enum.reduce(Keyword.get(config, :diagnostic_fields, []), notes, fn field, acc ->
        left_value = Map.get(left_case, field)
        right_value = Map.get(right_case, field)

        if is_nil(left_value) or is_nil(right_value) or left_value == right_value do
          acc
        else
          acc ++ ["#{field} differs: FX=#{inspect(left_value)} MX=#{inspect(right_value)} (diagnostic)"]
        end
      end)

    {updated_notes, status, blockers, warnings}
  end

  defp maybe_add_side_invariant_notes({notes, status, blockers, warnings}, blocking_case, case_data, side_label, config) do
    Enum.reduce(Keyword.get(config, :side_invariants, []), {notes, status, blockers, warnings}, fn invariant, acc ->
      {truth_field, lhs_field, rhs_field, description} = invariant
      truth_value = Map.get(case_data, truth_field)
      lhs_value = Map.get(case_data, lhs_field)
      rhs_value = Map.get(case_data, rhs_field)

      if truth_value === true and not is_nil(lhs_value) and not is_nil(rhs_value) and lhs_value != rhs_value do
        {acc_notes, acc_status, acc_blockers, acc_warnings} = acc
        note = "#{side_label} invariant failed (#{description}): #{lhs_field}=#{inspect(lhs_value)} #{rhs_field}=#{inspect(rhs_value)}"

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
      else
        acc
      end
    end)
  end

  defp blocking?(classification, config) do
    classification in Keyword.fetch!(config, :blocking_classifications)
  end
end
