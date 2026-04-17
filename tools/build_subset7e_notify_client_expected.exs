#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule BuildSubset7ENotifyClientExpected do
  @default_accept_source Path.expand("../fixtures/control/bplist_packet_corpus_v1.source.json", __DIR__)
  @default_output Path.expand("../fixtures/control/subset7e_notify_client_outcome_expected_v1.json", __DIR__)

  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        accept_source: :string,
        output: :string
      )

    accept_path = CortTooling.string_flag(options, :accept_source) || @default_accept_source
    output_path = CortTooling.string_flag(options, :output) || @default_output

    document = build_document!(accept_path)
    CortTooling.write_text!(output_path, encode_pretty(document) <> "\n")
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end

  defp build_document!(accept_path) do
    accept = load_source!(accept_path)

    results =
      accept
      |> Map.fetch!("cases")
      |> Enum.filter(&notify_client_case?/1)
      |> Enum.map(&build_result!/1)
      |> Enum.sort_by(&Map.fetch!(&1, "name"))

    %{
      "probe" => "subset7e_notify_client_outcome",
      "corpus_version" => Map.fetch!(accept, "version"),
      "format" => "CORT notify-client outcome expected corpus",
      "generated_from" => [relative_or_absolute(accept_path)],
      "results" => results
    }
  end

  defp load_source!(path) do
    data = CortTooling.read_json!(path)

    unless Map.get(data, "validation_mode") == "accept" do
      raise ArgumentError, "#{path} has validation_mode=#{inspect(Map.get(data, "validation_mode"))}, expected \"accept\""
    end

    cases = Map.get(data, "cases")

    unless is_list(cases) do
      raise ArgumentError, "#{path} does not contain a top-level cases array"
    end

    data
  end

  defp notify_client_case?(case_data) do
    case_id = fetch_string!(case_data, "id")

    Map.get(case_data, "kind") == "response" and
      String.starts_with?(case_id, "response.notify.") and
      not String.starts_with?(case_id, "response.notify.list_names.")
  end

  defp build_result!(case_data) do
    name = fetch_string!(case_data, "id")
    packet = fetch_map!(case_data, "packet") |> normalize_nil()
    {primary, alternate} = notify_client_expected(packet)

    %{
      "name" => name,
      "classification" => "required",
      "validation_mode" => "accept",
      "kind" => "response",
      "success" => true,
      "primary_value_text" => encode_compact(primary),
      "alternate_value_text" => alternate
    }
  end

  defp notify_client_expected(packet) do
    status = fetch_string!(packet, "status")

    case status do
      "error" ->
        payload = fetch_map!(packet, "error")
        code = fetch_string!(payload, "code")
        message = fetch_string!(payload, "message")
        status_family = notify_status_family(code, message)

        {
          %{
            "kind" => "error",
            "status_family" => status_family,
            "code" => code,
            "message" => message
          },
          "notify error #{status_family} code=#{code}"
        }

      "ok" ->
        result = Map.fetch!(packet, "result") |> normalize_nil()
        classify_success_result!(result)

      other ->
        raise ArgumentError, "unsupported response status #{inspect(other)}"
    end
  end

  defp classify_success_result!(result) when is_map(result) do
    cond do
      Map.has_key?(result, "registration") and Map.has_key?(result, "changed") and Map.has_key?(result, "current_state") and Map.has_key?(result, "generation") and registration_object?(result["registration"]) ->
        generation = fetch_integer!(result, "generation")
        registration = registration_view!(result["registration"])
        changed = fetch_boolean!(result, "changed")

        {
          %{
            "kind" => "notify.check",
            "status_family" => "ok",
            "changed" => changed,
            "current_state" => fetch_integer!(result, "current_state"),
            "generation" => generation,
            "registration" => registration
          },
          "notify ok check token=#{fetch_integer!(registration, "token")} changed=#{bool_text(changed)} generation=#{generation}"
        }

      Map.has_key?(result, "registration") and Map.has_key?(result, "canceled") and registration_object?(result["registration"]) ->
        canceled = fetch_boolean!(result, "canceled")

        {
          %{
            "kind" => "notify.cancel",
            "status_family" => "ok",
            "canceled" => canceled
          },
          "notify ok cancel canceled=#{bool_text(canceled)}"
        }

      Map.has_key?(result, "name") and Map.has_key?(result, "delivered_tokens") and Map.has_key?(result, "generation") and Map.has_key?(result, "posted_at") and name_object?(result["name"]) ->
        generation = fetch_integer!(result, "generation")
        delivered_count = result |> fetch_list!("delivered_tokens") |> length()

        {
          %{
            "kind" => "notify.post",
            "status_family" => "ok",
            "delivered_tokens_count" => delivered_count,
            "generation" => generation,
            "name" => fetch_string!(result["name"], "name"),
            "scope" => fetch_string!(result["name"], "scope"),
            "state" => fetch_integer!(result["name"], "state")
          },
          "notify ok post generation=#{generation} delivered=#{delivered_count}"
        }

      Map.has_key?(result, "name") and Map.has_key?(result, "state") and name_object?(result["name"]) ->
        generation = fetch_integer!(result["name"], "generation")
        state = fetch_integer!(result, "state")

        {
          %{
            "kind" => "notify.name_state",
            "status_family" => "ok",
            "generation" => generation,
            "name" => fetch_string!(result["name"], "name"),
            "scope" => fetch_string!(result["name"], "scope"),
            "state" => state
          },
          "notify ok name_state state=#{state} generation=#{generation}"
        }

      Map.has_key?(result, "valid") and map_size(result) == 1 ->
        valid = fetch_boolean!(result, "valid")

        {
          %{
            "kind" => "notify.validity",
            "status_family" => "ok",
            "valid" => valid
          },
          "notify ok validity valid=#{bool_text(valid)}"
        }

      Map.has_key?(result, "registration") and registration_object?(result["registration"]) ->
        registration = registration_view!(result["registration"])

        {
          %{
            "kind" => "notify.registration",
            "status_family" => "ok",
            "registration" => registration
          },
          "notify ok registration token=#{fetch_integer!(registration, "token")} session=#{fetch_string!(registration, "session_id")}"
        }

      registration_object?(result) ->
        registration = registration_view!(result)

        {
          %{
            "kind" => "notify.registration",
            "status_family" => "ok",
            "registration" => registration
          },
          "notify ok registration token=#{fetch_integer!(registration, "token")} session=#{fetch_string!(registration, "session_id")}"
        }

      true ->
        raise ArgumentError, "unsupported notify response result #{inspect(result)}"
    end
  end

  defp classify_success_result!(other) do
    raise ArgumentError, "unsupported notify response result #{inspect(other)}"
  end

  defp registration_view!(registration) do
    %{
      "first_check_pending" => fetch_boolean!(registration, "first_check_pending"),
      "last_seen_generation" => fetch_integer!(registration, "last_seen_generation"),
      "name" => fetch_string!(registration, "name"),
      "scope" => fetch_string!(registration, "scope"),
      "session_id" => fetch_string!(registration, "session_id"),
      "token" => fetch_integer!(registration, "token")
    }
    |> maybe_put("binding_handle", Map.get(registration, "local_transport_handle"))
    |> maybe_put("binding_id", Map.get(registration, "local_transport_binding_id"))
  end

  defp notify_status_family("notify_failed", message) do
    cond do
      String.contains?(message, "invalid_name(") -> "invalid_name"
      String.contains?(message, "invalid_token(") -> "invalid_token"
      String.contains?(message, "invalid_signal(") -> "invalid_signal"
      String.contains?(message, "invalid_client_registration_id(") -> "invalid_request"
      String.contains?(message, "transport_binding_in_use(") -> "invalid_file"
      String.contains?(message, "unknown_transport_binding(") -> "invalid_file"
      String.contains?(message, "transport_failed(") -> "invalid_file"
      true -> "failed"
    end
  end

  defp notify_status_family("invalid_request", _message), do: "invalid_request"
  defp notify_status_family("unsupported_method", _message), do: "invalid_request"
  defp notify_status_family(_code, _message), do: "failed"

  defp name_object?(value) when is_map(value) do
    required_keys = ~w(scope name generation state updated_at last_posted_at)

    Enum.all?(required_keys, &Map.has_key?(value, &1)) and
      is_binary(Map.get(value, "scope")) and
      is_binary(Map.get(value, "name")) and
      is_integer(Map.get(value, "generation")) and
      is_integer(Map.get(value, "state")) and
      date_wrapper?(Map.get(value, "updated_at")) and
      date_wrapper?(Map.get(value, "last_posted_at"))
  end

  defp name_object?(_value), do: false

  defp registration_object?(value) when is_map(value) do
    required_keys = ~w(created_at delivery_method delivery_state first_check_pending last_seen_generation name scope session_id suspend_depth token valid)

    Enum.all?(required_keys, &Map.has_key?(value, &1)) and
      date_wrapper?(Map.get(value, "created_at")) and
      is_binary(Map.get(value, "delivery_method")) and
      is_binary(Map.get(value, "delivery_state")) and
      is_boolean(Map.get(value, "first_check_pending")) and
      is_integer(Map.get(value, "last_seen_generation")) and
      is_binary(Map.get(value, "name")) and
      is_binary(Map.get(value, "scope")) and
      is_binary(Map.get(value, "session_id")) and
      is_integer(Map.get(value, "suspend_depth")) and
      is_integer(Map.get(value, "token")) and
      is_boolean(Map.get(value, "valid"))
  end

  defp registration_object?(_value), do: false

  defp date_wrapper?(%{"$date" => value}) when is_binary(value), do: true
  defp date_wrapper?(_value), do: false

  defp normalize_nil(value) when is_map(value) do
    value
    |> Enum.reduce(%{}, fn {key, child}, acc ->
      if is_nil(child) do
        acc
      else
        Map.put(acc, key, normalize_nil(child))
      end
    end)
  end

  defp normalize_nil(value) when is_list(value), do: Enum.map(value, &normalize_nil/1)
  defp normalize_nil(value), do: value

  defp maybe_put(map, _key, nil), do: map
  defp maybe_put(map, key, value), do: Map.put(map, key, value)

  defp bool_text(true), do: "true"
  defp bool_text(false), do: "false"

  defp fetch_string!(map, key) when is_map(map) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> value
      _ -> raise ArgumentError, "missing or invalid string field #{inspect(key)}"
    end
  end

  defp fetch_integer!(map, key) when is_map(map) do
    case Map.get(map, key) do
      value when is_integer(value) -> value
      _ -> raise ArgumentError, "missing or invalid integer field #{inspect(key)}"
    end
  end

  defp fetch_boolean!(map, key) when is_map(map) do
    case Map.get(map, key) do
      value when is_boolean(value) -> value
      _ -> raise ArgumentError, "missing or invalid boolean field #{inspect(key)}"
    end
  end

  defp fetch_map!(map, key) when is_map(map) do
    case Map.get(map, key) do
      value when is_map(value) -> value
      _ -> raise ArgumentError, "missing or invalid object field #{inspect(key)}"
    end
  end

  defp fetch_list!(map, key) when is_map(map) do
    case Map.get(map, key) do
      value when is_list(value) -> value
      _ -> raise ArgumentError, "missing or invalid array field #{inspect(key)}"
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
    escaped =
      value
      |> String.replace("\\", "\\\\")
      |> String.replace("\"", "\\\"")
      |> String.replace("\n", "\\n")
      |> String.replace("\r", "\\r")
      |> String.replace("\t", "\\t")

    "\"#{escaped}\""
  end

  defp encode_scalar(value) when is_boolean(value), do: if(value, do: "true", else: "false")
  defp encode_scalar(value) when is_integer(value), do: Integer.to_string(value)
  defp encode_scalar(nil), do: "null"
end

BuildSubset7ENotifyClientExpected.main(System.argv())
