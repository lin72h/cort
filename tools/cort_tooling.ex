defmodule CortTooling do
  @moduledoc false

  def read_json!(path) do
    path
    |> File.read!()
    |> :json.decode()
    |> normalize_json()
  end

  def write_text!(path, text) do
    path
    |> Path.dirname()
    |> File.mkdir_p!()

    File.write!(path, text)
  end

  def normalize_json(:null), do: nil

  def normalize_json(value) when is_map(value) do
    Map.new(value, fn {key, map_value} -> {key, normalize_json(map_value)} end)
  end

  def normalize_json(value) when is_list(value) do
    Enum.map(value, &normalize_json/1)
  end

  def normalize_json(value), do: value

  def string_flag(options, key) do
    case Keyword.get(options, key) do
      nil -> nil
      value when is_binary(value) and value != "" -> value
      _ -> nil
    end
  end

  def required_flag!(options, key) do
    case string_flag(options, key) do
      nil -> raise ArgumentError, "missing required flag --#{key_to_flag(key)}"
      value -> value
    end
  end

  def parse_args!(argv, switches) do
    {options, rest, invalid} = OptionParser.parse(argv, strict: switches)

    cond do
      invalid != [] ->
        invalid_text =
          Enum.map_join(invalid, ", ", fn {name, _value} -> "--#{name}" end)

        raise ArgumentError, "invalid option(s): #{invalid_text}"

      rest != [] ->
        raise ArgumentError, "unexpected positional arguments: #{Enum.join(rest, " ")}"

      true ->
        options
    end
  end

  def render_or_write!(report, output_path) do
    if output_path do
      write_text!(output_path, report)
    else
      IO.write(report)
    end
  end

  def key_to_flag(key) when is_atom(key) do
    key
    |> Atom.to_string()
    |> String.replace("_", "-")
  end

  def usage_and_exit!(message) do
    IO.puts(:stderr, message)
    System.halt(2)
  end
end
