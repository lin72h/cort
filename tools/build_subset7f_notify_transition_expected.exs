#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule BuildSubset7FNotifyTransitionExpected do
  @default_outcomes Path.expand("../fixtures/control/subset7e_notify_client_outcome_expected_v1.json", __DIR__)
  @default_scenarios Path.expand("../fixtures/control/subset7f_notify_state_transition_cases_v1.json", __DIR__)
  @default_output Path.expand("../fixtures/control/subset7f_notify_state_transition_expected_v1.json", __DIR__)

  def main(argv) do
    options =
      CortTooling.parse_args!(argv,
        outcomes: :string,
        scenarios: :string,
        output: :string
      )

    outcomes_path = CortTooling.string_flag(options, :outcomes) || @default_outcomes
    scenarios_path = CortTooling.string_flag(options, :scenarios) || @default_scenarios
    output_path = CortTooling.string_flag(options, :output) || @default_output

    document = build_document!(outcomes_path, scenarios_path)
    CortTooling.write_text!(output_path, encode_pretty(document) <> "\n")
  rescue
    error in [ArgumentError, File.Error, ErlangError, RuntimeError] ->
      CortTooling.usage_and_exit!(Exception.message(error))
  end

  defp build_document!(outcomes_path, scenarios_path) do
    outcomes = load_outcomes!(outcomes_path)
    scenarios = load_scenarios!(scenarios_path)

    results =
      scenarios
      |> Map.fetch!("cases")
      |> Enum.map(fn scenario -> build_result!(scenario, outcomes) end)
      |> Enum.sort_by(&Map.fetch!(&1, "name"))

    %{
      "probe" => "subset7f_notify_state_transition",
      "corpus_version" => Map.fetch!(scenarios, "version"),
      "format" => "CORT notify state transition expected corpus",
      "generated_from" => [
        relative_or_absolute(outcomes_path),
        relative_or_absolute(scenarios_path)
      ],
      "results" => results
    }
  end

  defp load_outcomes!(path) do
    results = path |> CortTooling.read_json!() |> Map.get("results")

    unless is_list(results) do
      raise ArgumentError, "#{path} does not contain a top-level results array"
    end

    Enum.reduce(results, %{}, fn entry, acc ->
      name = fetch_string!(entry, "name")
      encoded = fetch_string!(entry, "primary_value_text")
      Map.put(acc, name, decode_json_text!(encoded))
    end)
  end

  defp load_scenarios!(path) do
    data = CortTooling.read_json!(path)
    cases = Map.get(data, "cases")

    unless is_list(cases) do
      raise ArgumentError, "#{path} does not contain a top-level cases array"
    end

    data
  end

  defp build_result!(scenario, outcomes) do
    name = fetch_string!(scenario, "name")
    outcome_name = fetch_string!(scenario, "outcome_case")
    operation = fetch_string!(scenario, "operation")
    outcome = Map.get(outcomes, outcome_name)
    cached = Map.get(scenario, "cached_registration")

    unless is_map(outcome) do
      raise ArgumentError, "missing 7E outcome #{inspect(outcome_name)} for scenario #{inspect(name)}"
    end

    {primary, alternate} = apply_transition!(operation, outcome, cached)

    %{
      "name" => name,
      "classification" => "required",
      "validation_mode" => "accept",
      "kind" => "notify_transition",
      "success" => true,
      "primary_value_text" => encode_compact(primary),
      "alternate_value_text" => alternate
    }
  end

  defp apply_transition!(operation, outcome, cached) do
    status_family = fetch_string!(outcome, "status_family")
    outcome_kind = fetch_string!(outcome, "kind")
    cached_registration = normalize_cached_registration(cached)

    cond do
      outcome_kind == "error" ->
        apply_error_transition!(operation, status_family, cached_registration)

      operation in ["registration_store", "registration_update"] and outcome_kind == "notify.registration" ->
        registration = outcome |> fetch_map!("registration") |> to_cached_registration()
        primary = transition_primary(operation, "store", status_family, registration, nil, nil, nil)
        {primary, summary_for(primary)}

      operation == "check" and outcome_kind == "notify.check" ->
        remote = outcome |> fetch_map!("registration") |> to_cached_registration()
        generation = fetch_integer!(outcome, "generation")
        remote_changed = fetch_boolean!(outcome, "changed")
        cached_registration || raise ArgumentError, "check transition requires cached registration"

        final_registration =
          %{
            "token" => fetch_integer!(remote, "token"),
            "session_id" => fetch_string!(remote, "session_id"),
            "scope" => fetch_string!(remote, "scope"),
            "name" => fetch_string!(remote, "name"),
            "last_seen_generation" =>
              max3(
                fetch_integer!(remote, "last_seen_generation"),
                fetch_integer!(cached_registration, "last_seen_generation"),
                generation
              ),
            "first_check_pending" =>
              fetch_boolean!(remote, "first_check_pending") and
                fetch_boolean!(cached_registration, "first_check_pending")
          }
          |> maybe_put("binding_id", Map.get(remote, "binding_id") || Map.get(cached_registration, "binding_id"))

        changed =
          remote_changed and
            (fetch_boolean!(cached_registration, "first_check_pending") or
               generation > fetch_integer!(cached_registration, "last_seen_generation"))

        primary = transition_primary(operation, "merge", status_family, final_registration, nil, changed, nil)
        {primary, summary_for(primary)}

      operation == "cancel" and outcome_kind == "notify.cancel" ->
        cached_registration || raise ArgumentError, "cancel transition requires cached registration"
        primary =
          transition_primary(
            operation,
            "discard",
            status_family,
            nil,
            Map.get(cached_registration, "binding_id"),
            nil,
            nil
          )

        {primary, summary_for(primary)}

      operation == "get_state" and outcome_kind == "notify.name_state" ->
        cached_registration || raise ArgumentError, "get_state transition requires cached registration"
        primary = transition_primary(operation, "keep", status_family, cached_registration, nil, nil, nil)
        {primary, summary_for(primary)}

      operation == "is_valid_token" and outcome_kind == "notify.validity" ->
        cached_registration || raise ArgumentError, "is_valid_token transition requires cached registration"
        valid = fetch_boolean!(outcome, "valid")

        primary =
          if valid do
            transition_primary(operation, "keep", status_family, cached_registration, nil, nil, true)
          else
            transition_primary(
              operation,
              "discard",
              status_family,
              nil,
              Map.get(cached_registration, "binding_id"),
              nil,
              false
            )
          end

        {primary, summary_for(primary)}

      true ->
        raise ArgumentError,
              "unsupported 7F transition operation=#{inspect(operation)} outcome_kind=#{inspect(outcome_kind)}"
    end
  end

  defp apply_error_transition!(operation, status_family, cached_registration) do
    cond do
      status_family == "invalid_token" ->
        primary =
          transition_primary(
            operation,
            "discard",
            status_family,
            nil,
            binding_id_or_nil(cached_registration),
            nil,
            if(operation == "is_valid_token", do: false, else: nil)
          )

        {primary, summary_for(primary)}

      operation == "is_valid_token" ->
        valid = not is_nil(cached_registration)
        primary = transition_primary(operation, "keep", status_family, cached_registration, nil, nil, valid)
        {primary, summary_for(primary)}

      true ->
        primary = transition_primary(operation, "keep", status_family, cached_registration, nil, nil, nil)
        {primary, summary_for(primary)}
    end
  end

  defp transition_primary(operation, action, status_family, registration, released_binding_id, changed_result, valid_result) do
    %{
      "action" => action,
      "operation" => operation,
      "status_family" => status_family
    }
    |> maybe_put("registration", registration)
    |> maybe_put("released_binding_id", released_binding_id)
    |> maybe_put("changed_result", changed_result)
    |> maybe_put("valid_result", valid_result)
  end

  defp summary_for(primary) do
    operation = fetch_string!(primary, "operation")
    action = fetch_string!(primary, "action")
    status = fetch_string!(primary, "status_family")

    cond do
      is_map(Map.get(primary, "registration")) and Map.has_key?(primary, "changed_result") ->
        registration = fetch_map!(primary, "registration")

        "notify transition #{operation} #{action} token=#{fetch_integer!(registration, "token")} changed=#{bool_text(fetch_boolean!(primary, "changed_result"))} last_seen=#{fetch_integer!(registration, "last_seen_generation")} status=#{status}"

      Map.has_key?(primary, "valid_result") ->
        "notify transition #{operation} #{action} valid=#{bool_text(fetch_boolean!(primary, "valid_result"))} status=#{status}"

      is_map(Map.get(primary, "registration")) ->
        registration = fetch_map!(primary, "registration")
        "notify transition #{operation} #{action} token=#{fetch_integer!(registration, "token")} status=#{status}"

      is_binary(Map.get(primary, "released_binding_id")) ->
        "notify transition #{operation} #{action} release=#{fetch_string!(primary, "released_binding_id")} status=#{status}"

      true ->
        "notify transition #{operation} #{action} status=#{status}"
    end
  end

  defp to_cached_registration(registration) do
    %{
      "first_check_pending" => fetch_boolean!(registration, "first_check_pending"),
      "last_seen_generation" => fetch_integer!(registration, "last_seen_generation"),
      "name" => fetch_string!(registration, "name"),
      "scope" => fetch_string!(registration, "scope"),
      "session_id" => fetch_string!(registration, "session_id"),
      "token" => fetch_integer!(registration, "token")
    }
    |> maybe_put("binding_id", Map.get(registration, "binding_id"))
  end

  defp normalize_cached_registration(nil), do: nil
  defp normalize_cached_registration(registration) when is_map(registration), do: to_cached_registration(registration)
  defp normalize_cached_registration(_other), do: raise(ArgumentError, "cached_registration must be an object when present")

  defp binding_id_or_nil(nil), do: nil
  defp binding_id_or_nil(registration), do: Map.get(registration, "binding_id")

  defp max3(left, middle, right), do: Enum.max([left, middle, right])

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

  defp relative_or_absolute(path) do
    cwd = File.cwd!()

    case Path.relative_to(path, cwd) do
      relative when relative == path -> path
      relative -> relative
    end
  end

  defp decode_json_text!(text) when is_binary(text) do
    text
    |> :json.decode()
    |> CortTooling.normalize_json()
  rescue
    _error -> raise ArgumentError, "invalid compact JSON text in expected corpus"
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

BuildSubset7FNotifyTransitionExpected.main(System.argv())
