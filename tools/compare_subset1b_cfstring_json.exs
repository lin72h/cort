#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)
Code.require_file("compare_result_sets.ex", __DIR__)

defmodule CompareSubset1BCFStringJson do
  @blocking_classifications ["required", "required-but-error-text-flexible"]
  @semantic_boolean_fields ["equal_same_value", "equal_different_value"]
  @semantic_numeric_fields ["length_value"]
  @semantic_text_fields ["primary_value_text", "alternate_value_text"]
  @diagnostic_fields ["type_id", "type_description", "hash_primary", "hash_same_value", "hash_different_value"]

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

    {report, blockers, _warnings} =
      CompareResultSets.compare_paths!(fx_path, mx_path,
        title: "Subset 1B FX vs MX CFString Core Comparison",
        left_heading: "FX input",
        right_heading: "MX input",
        left_side: "FX",
        right_side: "MX",
        left_label: fx_label,
        right_label: mx_label,
        blocking_classifications: @blocking_classifications,
        semantic_boolean_fields: @semantic_boolean_fields,
        semantic_numeric_fields: @semantic_numeric_fields,
        semantic_text_fields: @semantic_text_fields,
        diagnostic_fields: @diagnostic_fields,
        success_verdict: "- verdict: no blocking mismatches found in the shared CFString core comparison surface",
        failure_verdict:
          "- verdict: blocking mismatches found; do not treat FX as semantically aligned for the affected required CFString rows"
      )

    CortTooling.render_or_write!(report, output_path)
    System.halt(if blockers == 0, do: 0, else: 1)
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end
end

CompareSubset1BCFStringJson.main(System.argv())
