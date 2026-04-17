#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)
Code.require_file("compare_result_sets.ex", __DIR__)

defmodule CompareSubset7BControlEnvelopeJson do
  @blocking_classifications ["required"]
  @semantic_text_fields ["validation_mode", "kind", "primary_value_text", "alternate_value_text"]

  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        fx_json: :string,
        expected_json: :string,
        fx_label: :string,
        expected_label: :string,
        output: :string
      )

    fx_path = CortTooling.required_flag!(options, :fx_json)
    expected_path = CortTooling.required_flag!(options, :expected_json)
    fx_label = CortTooling.string_flag(options, :fx_label) || fx_path
    expected_label = CortTooling.string_flag(options, :expected_label) || expected_path
    output_path = CortTooling.string_flag(options, :output)

    {report, blockers, _warnings} =
      CompareResultSets.compare_paths!(fx_path, expected_path,
        title: "Subset 7B FX vs Expected Control Envelope Compatibility Comparison",
        left_heading: "FX input",
        right_heading: "Expected input",
        left_side: "FX",
        right_side: "expected",
        left_label: fx_label,
        right_label: expected_label,
        blocking_classifications: @blocking_classifications,
        semantic_text_fields: @semantic_text_fields,
        success_verdict: "- verdict: no blocking mismatches found in the shared control-envelope compatibility surface",
        failure_verdict:
          "- verdict: blocking mismatches found; do not treat FX as semantically aligned for the affected control-envelope rows"
      )

    CortTooling.render_or_write!(report, output_path)
    System.halt(if blockers == 0, do: 0, else: 1)
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end
end

CompareSubset7BControlEnvelopeJson.main(System.argv())
