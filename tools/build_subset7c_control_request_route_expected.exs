#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule BuildSubset7CControlRequestRouteExpected do
  @default_accept_source Path.expand("../fixtures/control/bplist_packet_corpus_v1.source.json", __DIR__)
  @default_reject_source Path.expand("../fixtures/control/bplist_packet_rejection_corpus_v1.source.json", __DIR__)
  @default_output Path.expand("../fixtures/control/subset7c_control_request_route_expected_v1.json", __DIR__)

  @known_param_keys [
    "client_registration_id",
    "expected_session_id",
    "name",
    "queue_name",
    "reuse_existing_binding",
    "scope",
    "signal",
    "state",
    "target_pid",
    "token"
  ]

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
      build_accept_results!(accept) ++ build_reject_results!(reject)
      |> Enum.sort_by(&Map.fetch!(&1, "name"))

    %{
      "probe" => "subset7c_control_request_route",
      "corpus_version" => corpus_version,
      "format" => "CORT ControlProtocol request-route expected corpus",
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

  defp build_accept_results!(data) do
    data
    |> Map.fetch!("cases")
    |> Enum.filter(&(Map.get(&1, "kind") == "request"))
    |> Enum.map(fn case_data ->
      name = fetch_string!(case_data, "id")
      packet = fetch_map!(case_data, "packet")
      params = fetch_map!(packet, "params")
      service = fetch_string!(packet, "service")
      method = fetch_string!(packet, "method")
      route_kind = route_kind!(service, method)
      param_keys = Map.keys(params) |> Enum.sort()

      json_map =
        %{
          "kind" => route_kind,
          "method" => method,
          "param_keys" => param_keys,
          "protocol_version" => fetch_integer!(packet, "protocol_version"),
          "service" => service
        }
        |> Map.merge(route_optional_fields(params))

      %{
        "name" => name,
        "classification" => "required",
        "validation_mode" => "accept",
        "kind" => "request",
        "success" => true,
        "primary_value_text" => encode_compact(json_map),
        "alternate_value_text" => route_summary(route_kind, param_keys)
      }
    end)
  end

  defp build_reject_results!(data) do
    data
    |> Map.fetch!("cases")
    |> Enum.filter(&(Map.get(&1, "kind") == "request"))
    |> Enum.map(fn case_data ->
      name = fetch_string!(case_data, "id")
      expected_error = fetch_map!(case_data, "expected_error")

      %{
        "name" => name,
        "classification" => "required",
        "validation_mode" => "reject",
        "kind" => "request",
        "success" => true,
        "primary_value_text" => encode_compact(expected_error),
        "alternate_value_text" => rejected_summary(expected_error)
      }
    end)
  end

  defp route_optional_fields(params) do
    Enum.reduce(@known_param_keys, %{}, fn key, acc ->
      case Map.fetch(params, key) do
        {:ok, value} -> Map.put(acc, key, value)
        :error -> acc
      end
    end)
  end

  defp route_summary(route_kind, []), do: "route #{route_kind} params=none"
  defp route_summary(route_kind, param_keys), do: "route #{route_kind} params=#{Enum.join(param_keys, ",")}"

  defp route_kind!("control", "capabilities"), do: "control.capabilities"
  defp route_kind!("control", "health"), do: "control.health"
  defp route_kind!("diagnostics", "snapshot"), do: "diagnostics.snapshot"
  defp route_kind!("notify", "cancel"), do: "notify.cancel"
  defp route_kind!("notify", "check"), do: "notify.check"
  defp route_kind!("notify", "get_state"), do: "notify.get_state"
  defp route_kind!("notify", "is_valid_token"), do: "notify.is_valid_token"
  defp route_kind!("notify", "list_names"), do: "notify.list_names"
  defp route_kind!("notify", "post"), do: "notify.post"
  defp route_kind!("notify", "prepare_file_descriptor_delivery"), do: "notify.prepare_file_descriptor_delivery"
  defp route_kind!("notify", "register_check"), do: "notify.register_check"
  defp route_kind!("notify", "register_dispatch"), do: "notify.register_dispatch"
  defp route_kind!("notify", "register_file_descriptor"), do: "notify.register_file_descriptor"
  defp route_kind!("notify", "register_signal"), do: "notify.register_signal"
  defp route_kind!("notify", "resume"), do: "notify.resume"
  defp route_kind!("notify", "set_state"), do: "notify.set_state"
  defp route_kind!("notify", "suspend"), do: "notify.suspend"

  defp route_kind!(service, method) do
    raise ArgumentError, "unsupported request route #{inspect(service)}.#{inspect(method)} in accepted corpus"
  end

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

  defp rejected_summary(expected_error) do
    case Map.get(expected_error, "type") do
      "missing_key" ->
        "reject request missing_key #{Map.get(expected_error, "key")}"

      "unsupported_version" ->
        "reject request unsupported_version #{Map.get(expected_error, "version")}"

      "invalid_packet" ->
        "reject request invalid_packet #{Map.get(expected_error, "detail")}"

      other ->
        "reject request #{other}"
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

BuildSubset7CControlRequestRouteExpected.main(System.argv())
