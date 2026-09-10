cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PAE_OUTPUT_FILE OR PAE_OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "PAE_OUTPUT_FILE is required.")
endif()

cmake_path(GET PAE_OUTPUT_FILE PARENT_PATH pae_output_directory)
file(MAKE_DIRECTORY "${pae_output_directory}")
file(
    WRITE
    "${PAE_OUTPUT_FILE}"
    [=[{
  "schema_version": "0.5",
  "protocol_id": "synthetic_ui_max",
  "protocol_version": "1.0-public",
  "display_name": "Public UI maximum fixed record",
  "description": "Synthetic maximum Desktop frame and fields for offline UI measurement.",
  "source_ref": "SYNTHETIC_FROM_SCRATCH:pae_ui_c1_max",
  "resource_profile": "desktop",
  "framing_profiles": [{"id":"record","display_name":"Record","description":"Complete record.","source_ref":"SYNTHETIC:max#framing","input_kind":"complete_record"}],
  "pipelines": [{"id":"ui_pipeline","display_name":"UI pipeline","description":"Offline synthetic pipeline.","source_ref":"SYNTHETIC:max#pipeline","direction_id":"synthetic","input_framing_profile_id":"record","message_ids":["maximum_record"]}],
  "messages": [{
    "id": "maximum_record",
    "display_name": "Maximum record",
    "description": "Desktop maximum 64 KiB frame and 2048 fields.",
    "source_ref": "SYNTHETIC:max#message",
    "direction_id": "synthetic",
    "frame_length_bytes": 65536,
    "matcher": {"all":[{"kind":"frame_length_equals","length_bytes":65536}]},
    "fields": [
]=]
)

foreach(pae_field_index RANGE 0 2046)
    file(
        APPEND
        "${PAE_OUTPUT_FILE}"
        "      {\"id\":\"byte_${pae_field_index}\",\"display_name\":\"Byte ${pae_field_index}\",\"description\":\"Synthetic scalar input.\",\"source_ref\":\"SYNTHETIC:max#byte_${pae_field_index}\",\"value_type\":\"UINT64\",\"wire\":{\"codec\":\"unsigned_integer\",\"byte_offset\":${pae_field_index},\"byte_width\":1},\"encode\":{\"source\":\"input\"}},\n"
    )
endforeach()

file(
    APPEND
    "${PAE_OUTPUT_FILE}"
    [=[      {"id":"payload","display_name":"Remaining payload","description":"Synthetic maximum remaining bytes.","source_ref":"SYNTHETIC:max#payload","value_type":"BYTES","wire":{"codec":"bytes","byte_offset":2047,"byte_length":63489},"encode":{"source":"input"}}
    ]
  }]
}
]=]
)
