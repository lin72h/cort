#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule BuildSubset7BControlEnvelopeExpected do
  @default_accept_source Path.expand("../fixtures/control/bplist_packet_corpus_v1.source.json", __DIR__)
  @default_reject_source Path.expand("../fixtures/control/bplist_packet_rejection_corpus_v1.source.json", __DIR__)
  @default_output Path.expand("../fixtures/control/subset7b_control_envelope_expected_v1.json", __DIR__)

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
      "probe" => "subset7b_control_envelope",
      "corpus_version" => corpus_version,
      "format" => "CORT ControlProtocol envelope expected corpus",
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

    {primary, alternate} =
      case kind do
        "request" -> request_expected(packet)
        "response" -> response_expected(packet)
        other -> raise ArgumentError, "unsupported accept kind #{inspect(other)}"
      end

    %{
      "name" => name,
      "classification" => "required",
      "validation_mode" => "accept",
      "kind" => kind,
      "success" => true,
      "primary_value_text" => encode_compact(primary),
      "alternate_value_text" => alternate
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

  defp request_expected(packet) do
    params = fetch_map!(packet, "params")
    service = fetch_string!(packet, "service")
    method = fetch_string!(packet, "method")
    version = fetch_integer!(packet, "protocol_version")
    param_keys = Map.keys(params) |> Enum.sort()

    {
      %{
        "method" => method,
        "param_keys" => param_keys,
        "params_count" => length(param_keys),
        "protocol_version" => version,
        "service" => service
      },
      "request #{service} #{method} params=#{length(param_keys)}"
    }
  end

  defp response_expected(packet) do
    version = fetch_integer!(packet, "protocol_version")
    status = fetch_string!(packet, "status")

    case status do
      "ok" ->
        value = Map.fetch!(packet, "result")
        {result_fields, summary} = response_success_details(value)

        {
          Map.merge(%{"protocol_version" => version, "status" => "ok"}, result_fields),
          summary
        }

      "error" ->
        payload = fetch_map!(packet, "error")
        code = fetch_string!(payload, "code")
        message = fetch_string!(payload, "message")

        {
          %{
            "code" => code,
            "message" => message,
            "protocol_version" => version,
            "status" => "error"
          },
          "response error code=#{code}"
        }

      other ->
        raise ArgumentError, "unsupported accept response status #{inspect(other)}"
    end
  end

  defp response_success_details(value) do
    case value_kind(value) do
      {:object, keys} ->
        {%{"result_count" => length(keys), "result_keys" => keys, "result_kind" => "object"}, "response ok result=object count=#{length(keys)}"}

      {:array, count} ->
        {%{"result_count" => count, "result_kind" => "array"}, "response ok result=array count=#{count}"}

      {:string, length} ->
        {%{"result_kind" => "string", "result_length" => length}, "response ok result=string length=#{length}"}

      {:data, size} ->
        {%{"result_kind" => "data", "result_size" => size}, "response ok result=data size=#{size}"}

      {:integer, _} ->
        {%{"result_kind" => "integer"}, "response ok result=integer"}

      {:double, _} ->
        {%{"result_kind" => "double"}, "response ok result=double"}

      {:bool, _} ->
        {%{"result_kind" => "bool"}, "response ok result=bool"}

      {:date, _} ->
        {%{"result_kind" => "date"}, "response ok result=date"}
    end
  end

  defp value_kind(%{"$date" => value}) when is_binary(value), do: {:date, value}

  defp value_kind(%{"$data_utf8" => value}) when is_binary(value), do: {:data, byte_size(value)}

  defp value_kind(%{"$data_base64" => value}) when is_binary(value) do
    decoded =
      case Base.decode64(value) do
        {:ok, bytes} -> bytes
        :error -> raise ArgumentError, "invalid base64 data wrapper"
      end

    {:data, byte_size(decoded)}
  end

  defp value_kind(value) when is_map(value) do
    keys =
      value
      |> Enum.reject(fn {_key, child} -> is_nil(child) end)
      |> Enum.map(&elem(&1, 0))
      |> Enum.sort()

    {:object, keys}
  end

  defp value_kind(value) when is_list(value), do: {:array, length(value)}
  defp value_kind(value) when is_integer(value), do: {:integer, value}
  defp value_kind(value) when is_float(value), do: {:double, value}
  defp value_kind(value) when is_boolean(value), do: {:bool, value}
  defp value_kind(value) when is_binary(value), do: {:string, utf16_length(value)}

  defp fetch_string!(map, key) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> value
      _ -> raise ArgumentError, "missing or invalid string field #{inspect(key)}"
    end
  end

  defp fetch_integer!(map, key) do
    case Map.get(map, key) do
      value when is_integer(value) -> value
      _ -> raise ArgumentError, "missing or invalid integer field #{inspect(key)}"
    end
  end

  defp fetch_map!(map, key) do
    case Map.get(map, key) do
      value when is_map(value) -> value
      _ -> raise ArgumentError, "missing or invalid object field #{inspect(key)}"
    end
  end

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

  defp utf16_length(text) do
    text
    |> :unicode.characters_to_binary(:utf8, {:utf16, :big})
    |> byte_size()
    |> div(2)
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

BuildSubset7BControlEnvelopeExpected.main(System.argv())
