#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule BuildSubset7AControlPacketExpected do
  @default_accept_source Path.expand("../fixtures/control/bplist_packet_corpus_v1.source.json", __DIR__)
  @default_reject_source Path.expand("../fixtures/control/bplist_packet_rejection_corpus_v1.source.json", __DIR__)
  @default_output Path.expand("../fixtures/control/subset7a_control_packet_expected_v1.json", __DIR__)

  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        accept_source: :string,
        reject_source: :string,
        output: :string
      )

    accept_path = CortTooling.string_flag(options, :accept_source) || @default_accept_source
    reject_path = CortTooling.string_flag(options, :reject_source) || @default_reject_source
    output_path = CortTooling.string_flag(options, :output) || @default_output

    document = build_document!(accept_path, reject_path)
    CortTooling.write_text!(output_path, encode_pretty(document) <> "\n")
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end

  defp build_document!(accept_path, reject_path) do
    accept = load_source!(accept_path, "accept")
    reject = load_source!(reject_path, "reject")

    corpus_version =
      case {Map.get(accept, "version"), Map.get(reject, "version")} do
        {version, version} when is_integer(version) -> version
        {left, right} -> raise ArgumentError, "source corpus version mismatch: accept=#{inspect(left)} reject=#{inspect(right)}"
      end

    results =
      build_results!(accept, "accept") ++ build_results!(reject, "reject")
      |> Enum.sort_by(&Map.fetch!(&1, "name"))

    %{
      "probe" => "subset7a_control_packet_decode",
      "corpus_version" => corpus_version,
      "format" => "CORT ControlProtocol semantic expected corpus",
      "generated_from" => [
        relative_or_absolute(accept_path),
        relative_or_absolute(reject_path)
      ],
      "results" => results
    }
  end

  defp load_source!(path, expected_mode) do
    data = CortTooling.read_json!(path)

    unless Map.get(data, "validation_mode") == expected_mode do
      raise ArgumentError,
            "#{path} has validation_mode=#{inspect(Map.get(data, "validation_mode"))}, expected #{inspect(expected_mode)}"
    end

    cases = Map.get(data, "cases")

    unless is_list(cases) do
      raise ArgumentError, "#{path} does not contain a top-level cases array"
    end

    data
  end

  defp build_results!(data, validation_mode) do
    data
    |> Map.fetch!("cases")
    |> Enum.map(&build_result!(&1, validation_mode))
  end

  defp build_result!(case_data, "accept") do
    name = fetch_string!(case_data, "id")
    kind = fetch_string!(case_data, "kind")
    packet = fetch_map!(case_data, "packet")

    %{
      "name" => name,
      "classification" => "required",
      "validation_mode" => "accept",
      "kind" => kind,
      "success" => true,
      "primary_value_text" => encode_compact(packet),
      "alternate_value_text" => accepted_summary(kind, packet)
    }
  end

  defp build_result!(case_data, "reject") do
    name = fetch_string!(case_data, "id")
    kind = fetch_string!(case_data, "kind")
    expected_error = fetch_map!(case_data, "expected_error")

    %{
      "name" => name,
      "classification" => "required",
      "validation_mode" => "reject",
      "kind" => kind,
      "success" => true,
      "primary_value_text" => encode_compact(expected_error),
      "alternate_value_text" => rejected_summary(kind, expected_error)
    }
  end

  defp fetch_string!(map, key) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> value
      _ -> raise ArgumentError, "missing or invalid string field #{inspect(key)}"
    end
  end

  defp fetch_map!(map, key) do
    case Map.get(map, key) do
      value when is_map(value) -> value
      _ -> raise ArgumentError, "missing or invalid object field #{inspect(key)}"
    end
  end

  defp accepted_summary("request", packet) do
    service = Map.get(packet, "service")
    method = Map.get(packet, "method")
    "accept request service=#{service} method=#{method}"
  end

  defp accepted_summary("response", packet) do
    case Map.get(packet, "status") do
      "ok" ->
        "accept response status=ok"

      "error" ->
        code =
          packet
          |> Map.get("error", %{})
          |> Map.get("code", "unknown")

        "accept response status=error code=#{code}"

      other ->
        "accept response status=#{other}"
    end
  end

  defp accepted_summary(kind, _packet), do: "accept #{kind}"

  defp rejected_summary(kind, expected_error) do
    case Map.get(expected_error, "type") do
      "missing_key" ->
        "reject #{kind} missing_key #{Map.get(expected_error, "key")}"

      "unsupported_version" ->
        "reject #{kind} unsupported_version #{Map.get(expected_error, "version")}"

      "invalid_packet" ->
        "reject #{kind} invalid_packet #{Map.get(expected_error, "detail")}"

      other ->
        "reject #{kind} #{other}"
    end
  end

  defp relative_or_absolute(path) do
    cwd = File.cwd!()

    case Path.relative_to(path, cwd) do
      relative when relative == path -> path
      relative -> relative
    end
  end

  defp encode_compact(value), do: encode_value(value)

  defp encode_pretty(value), do: pretty_value(value, 0)

  defp pretty_value(map, depth) when is_map(map) do
    keys = Map.keys(map) |> Enum.sort()

    if keys == [] do
      "{}"
    else
      indent = String.duplicate("  ", depth)
      child_indent = String.duplicate("  ", depth + 1)

      body =
        Enum.map_join(keys, ",\n", fn key ->
          "#{child_indent}#{encode_scalar(key)}: #{pretty_value(Map.fetch!(map, key), depth + 1)}"
        end)

      "{\n#{body}\n#{indent}}"
    end
  end

  defp pretty_value(list, depth) when is_list(list) do
    if list == [] do
      "[]"
    else
      indent = String.duplicate("  ", depth)
      child_indent = String.duplicate("  ", depth + 1)

      body =
        Enum.map_join(list, ",\n", fn item ->
          "#{child_indent}#{pretty_value(item, depth + 1)}"
        end)

      "[\n#{body}\n#{indent}]"
    end
  end

  defp pretty_value(value, _depth), do: encode_scalar(value)

  defp encode_value(map) when is_map(map) do
    "{" <>
      (map
       |> Map.keys()
       |> Enum.sort()
       |> Enum.map_join(",", fn key ->
         "#{encode_scalar(key)}:#{encode_value(Map.fetch!(map, key))}"
       end)) <> "}"
  end

  defp encode_value(list) when is_list(list) do
    "[" <> Enum.map_join(list, ",", &encode_value/1) <> "]"
  end

  defp encode_value(value), do: encode_scalar(value)

  defp encode_scalar(value) when is_binary(value) do
    value
    |> :json.encode()
    |> IO.iodata_to_binary()
  end

  defp encode_scalar(value) when is_integer(value) or is_float(value) or is_boolean(value) or is_nil(value) do
    value
    |> :json.encode()
    |> IO.iodata_to_binary()
  end
end

BuildSubset7AControlPacketExpected.main(System.argv())
