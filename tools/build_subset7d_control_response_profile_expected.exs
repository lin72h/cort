#!/usr/bin/env elixir
Code.require_file("cort_tooling.ex", __DIR__)

defmodule BuildSubset7DControlResponseProfileExpected do
  @default_accept_source Path.expand("../fixtures/control/bplist_packet_corpus_v1.source.json", __DIR__)
  @default_output Path.expand("../fixtures/control/subset7d_control_response_profile_expected_v1.json", __DIR__)

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
      |> Enum.filter(&(Map.get(&1, "kind") == "response"))
      |> Enum.map(&build_result!/1)
      |> Enum.sort_by(&Map.fetch!(&1, "name"))

    %{
      "probe" => "subset7d_control_response_profile",
      "corpus_version" => Map.fetch!(accept, "version"),
      "format" => "CORT ControlProtocol response-profile expected corpus",
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

  defp build_result!(case_data) do
    name = fetch_string!(case_data, "id")
    packet = fetch_map!(case_data, "packet") |> normalize_nil()
    {primary, alternate} = response_profile_expected(packet)

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

  defp response_profile_expected(packet) do
    version = fetch_integer!(packet, "protocol_version")
    status = fetch_string!(packet, "status")

    case status do
      "error" ->
        payload = fetch_map!(packet, "error")
        code = fetch_string!(payload, "code")
        message = fetch_string!(payload, "message")

        {
          %{
            "kind" => "error",
            "protocol_version" => version,
            "status" => "error",
            "code" => code,
            "message" => message
          },
          "profile error code=#{code}"
        }

      "ok" ->
        result = Map.fetch!(packet, "result") |> normalize_nil()
        {kind, fields, alternate} = classify_success_result!(result)

        {
          Map.merge(%{"kind" => kind, "protocol_version" => version, "status" => "ok"}, fields),
          alternate
        }

      other ->
        raise ArgumentError, "unsupported response status #{inspect(other)}"
    end
  end

  defp classify_success_result!(result) when is_list(result) do
    if Enum.all?(result, &name_object?/1) do
      items = result |> Enum.map(&name_summary/1) |> Enum.sort()
      {"notify.name_list", %{"count" => length(items), "items" => items}, "profile notify.name_list count=#{length(items)}"}
    else
      raise ArgumentError, "unsupported response array result profile"
    end
  end

  defp classify_success_result!(result) when is_map(result) do
    cond do
      Map.has_key?(result, "registration") and Map.has_key?(result, "canceled") and registration_object?(result["registration"]) ->
        token = fetch_integer!(result["registration"], "token")
        valid = fetch_boolean!(result["registration"], "valid")
        {
          "notify.cancel",
          %{
            "canceled" => fetch_boolean!(result, "canceled"),
            "delivery_method" => fetch_string!(result["registration"], "delivery_method"),
            "scope" => fetch_string!(result["registration"], "scope"),
            "token" => token,
            "valid" => valid
          },
          "profile notify.cancel canceled=#{bool_text(fetch_boolean!(result, "canceled"))} token=#{token}"
        }

      Map.has_key?(result, "registration") and Map.has_key?(result, "changed") and Map.has_key?(result, "current_state") and Map.has_key?(result, "generation") and registration_object?(result["registration"]) ->
        generation = fetch_integer!(result, "generation")
        {
          "notify.check",
          %{
            "changed" => fetch_boolean!(result, "changed"),
            "current_state" => fetch_integer!(result, "current_state"),
            "delivery_method" => fetch_string!(result["registration"], "delivery_method"),
            "generation" => generation,
            "scope" => fetch_string!(result["registration"], "scope"),
            "token" => fetch_integer!(result["registration"], "token"),
            "valid" => fetch_boolean!(result["registration"], "valid")
          },
          "profile notify.check changed=#{bool_text(fetch_boolean!(result, "changed"))} generation=#{generation}"
        }

      Map.has_key?(result, "name") and Map.has_key?(result, "delivered_tokens") and Map.has_key?(result, "generation") and Map.has_key?(result, "posted_at") and name_object?(result["name"]) ->
        generation = fetch_integer!(result, "generation")
        {
          "notify.post",
          %{
            "delivered_tokens_count" => length(fetch_list!(result, "delivered_tokens")),
            "generation" => generation,
            "name" => fetch_string!(result["name"], "name"),
            "scope" => fetch_string!(result["name"], "scope"),
            "state" => fetch_integer!(result["name"], "state")
          },
          "profile notify.post delivered=#{length(fetch_list!(result, "delivered_tokens"))} generation=#{generation}"
        }

      Map.has_key?(result, "name") and Map.has_key?(result, "state") and name_object?(result["name"]) ->
        generation = fetch_integer!(result["name"], "generation")
        state = fetch_integer!(result, "state")
        {
          "notify.name_state",
          %{
            "generation" => generation,
            "name" => fetch_string!(result["name"], "name"),
            "scope" => fetch_string!(result["name"], "scope"),
            "state" => state
          },
          "profile notify.name_state generation=#{generation} state=#{state}"
        }

      Map.has_key?(result, "valid") and map_size(result) == 1 ->
        valid = fetch_boolean!(result, "valid")
        {"notify.validity", %{"valid" => valid}, "profile notify.validity valid=#{bool_text(valid)}"}

      Map.has_key?(result, "registration") and registration_object?(result["registration"]) ->
        token = fetch_integer!(result["registration"], "token")
        valid = fetch_boolean!(result["registration"], "valid")
        {
          "notify.registration_wrapper",
          %{
            "delivery_method" => fetch_string!(result["registration"], "delivery_method"),
            "scope" => fetch_string!(result["registration"], "scope"),
            "token" => token,
            "valid" => valid
          },
          "profile notify.registration_wrapper token=#{token}"
        }

      registration_object?(result) ->
        token = fetch_integer!(result, "token")
        valid = fetch_boolean!(result, "valid")
        fields =
          %{
            "delivery_method" => fetch_string!(result, "delivery_method"),
            "scope" => fetch_string!(result, "scope"),
            "token" => token,
            "valid" => valid
          }
          |> maybe_put("pending_generation", Map.get(result, "pending_generation"))

        {"notify.registration", fields, "profile notify.registration token=#{token}"}

      capabilities_object?(result) ->
        flags = result |> fetch_list!("feature_flags") |> Enum.map(&fetch_binary!/1) |> Enum.sort()
        {
          "control.capabilities",
          %{
            "feature_flags" => flags,
            "supports_cluster_overlay" => fetch_boolean!(result, "supports_cluster_overlay"),
            "supports_launchctl_wrapper" => fetch_boolean!(result, "supports_launchctl_wrapper"),
            "supports_native_macos_passthrough" => fetch_boolean!(result, "supports_native_macos_passthrough"),
            "supports_notify_fast_path" => fetch_boolean!(result, "supports_notify_fast_path"),
            "supports_userspace_jobs" => fetch_boolean!(result, "supports_userspace_jobs"),
            "supports_userspace_notify" => fetch_boolean!(result, "supports_userspace_notify")
          },
          "profile control.capabilities flags=#{length(flags)}"
        }

      health_object?(result) ->
        state = fetch_string!(result, "state")
        reason_keys = result |> fetch_list!("reasons") |> Enum.map(&reason_summary/1) |> Enum.sort()
        {
          "control.health",
          %{
            "job_persistence_state" => fetch_string!(result["job_persistence"], "state"),
            "notify_fast_path_state" => fetch_string!(result["notify_fast_path"], "state"),
            "notify_persistence_state" => fetch_string!(result["notify_persistence"], "state"),
            "reason_count" => length(reason_keys),
            "reason_keys" => reason_keys,
            "state" => state
          },
          "profile control.health state=#{state} reasons=#{length(reason_keys)}"
        }

      diagnostics_snapshot_object?(result) ->
        flags = result["capabilities"] |> fetch_list!("feature_flags") |> Enum.map(&fetch_binary!/1) |> Enum.sort()
        jobs_count = result |> fetch_list!("jobs") |> length()
        notify_names_count = result |> fetch_list!("notify_names") |> length()
        {
          "diagnostics.snapshot",
          %{
            "daemon_id" => fetch_string!(result["daemon"], "daemon_id"),
            "feature_flags" => flags,
            "health_state" => fetch_string!(result["health"], "state"),
            "jobs_count" => jobs_count,
            "notify_names_count" => notify_names_count
          },
          "profile diagnostics.snapshot jobs=#{jobs_count} notify_names=#{notify_names_count}"
        }

      true ->
        keys = Map.keys(result) |> Enum.sort()
        {"generic.object", %{"result_keys" => keys}, "profile generic.object keys=#{length(keys)}"}
    end
  end

  defp classify_success_result!(other) do
    raise ArgumentError, "unsupported response result #{inspect(other)}"
  end

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

  defp capabilities_object?(value) when is_map(value) do
    Enum.all?(
      ~w(feature_flags supports_native_macos_passthrough supports_userspace_jobs supports_userspace_notify supports_cluster_overlay supports_notify_fast_path supports_launchctl_wrapper),
      &Map.has_key?(value, &1)
    ) and
      is_list(Map.get(value, "feature_flags")) and Enum.all?(Map.get(value, "feature_flags"), &is_binary/1) and
      Enum.all?(~w(supports_native_macos_passthrough supports_userspace_jobs supports_userspace_notify supports_cluster_overlay supports_notify_fast_path supports_launchctl_wrapper), fn key ->
        is_boolean(Map.get(value, key))
      end)
  end

  defp capabilities_object?(_value), do: false

  defp output_status_object?(value) when is_map(value) do
    Enum.all?(~w(state dirty retry_scheduled), &Map.has_key?(value, &1)) and
      is_binary(Map.get(value, "state")) and
      is_boolean(Map.get(value, "dirty")) and
      is_boolean(Map.get(value, "retry_scheduled"))
  end

  defp output_status_object?(_value), do: false

  defp health_object?(value) when is_map(value) do
    Enum.all?(~w(state reasons observed_at job_persistence notify_persistence notify_fast_path), &Map.has_key?(value, &1)) and
      is_binary(Map.get(value, "state")) and
      is_list(Map.get(value, "reasons")) and Enum.all?(Map.get(value, "reasons"), &reason_object?/1) and
      date_wrapper?(Map.get(value, "observed_at")) and
      output_status_object?(Map.get(value, "job_persistence")) and
      output_status_object?(Map.get(value, "notify_persistence")) and
      output_status_object?(Map.get(value, "notify_fast_path"))
  end

  defp health_object?(_value), do: false

  defp reason_object?(value) when is_map(value) do
    is_binary(Map.get(value, "component")) and is_binary(Map.get(value, "issue"))
  end

  defp reason_object?(_value), do: false

  defp diagnostics_snapshot_object?(value) when is_map(value) do
    Enum.all?(~w(daemon health capabilities jobs notify_names), &Map.has_key?(value, &1)) and
      daemon_object?(Map.get(value, "daemon")) and
      health_object?(Map.get(value, "health")) and
      capabilities_object?(Map.get(value, "capabilities")) and
      is_list(Map.get(value, "jobs")) and
      is_list(Map.get(value, "notify_names")) and
      Enum.all?(Map.get(value, "notify_names"), &name_object?/1)
  end

  defp diagnostics_snapshot_object?(_value), do: false

  defp daemon_object?(value) when is_map(value) do
    Enum.all?(~w(daemon_id node_id hostname runtime_mode version control_socket_path started_at cluster_enabled notify_fast_path_enabled), &Map.has_key?(value, &1)) and
      is_binary(Map.get(value, "daemon_id")) and
      is_binary(Map.get(value, "node_id")) and
      is_binary(Map.get(value, "hostname")) and
      is_binary(Map.get(value, "runtime_mode")) and
      is_binary(Map.get(value, "version")) and
      is_binary(Map.get(value, "control_socket_path")) and
      date_wrapper?(Map.get(value, "started_at")) and
      is_boolean(Map.get(value, "cluster_enabled")) and
      is_boolean(Map.get(value, "notify_fast_path_enabled"))
  end

  defp daemon_object?(_value), do: false

  defp name_summary(value) do
    "#{fetch_string!(value, "scope")}:#{fetch_string!(value, "name")}:#{fetch_integer!(value, "generation")}:#{fetch_integer!(value, "state")}"
  end

  defp reason_summary(value) do
    "#{fetch_string!(value, "component")}:#{fetch_string!(value, "issue")}"
  end

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

  defp fetch_binary!(value) when is_binary(value), do: value
  defp fetch_binary!(value), do: raise(ArgumentError, "invalid binary value #{inspect(value)}")

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
  defp encode_scalar(value) when is_float(value), do: :erlang.float_to_binary(value, [:compact, decimals: 12])
  defp encode_scalar(nil), do: "null"
end

BuildSubset7DControlResponseProfileExpected.main(System.argv())
