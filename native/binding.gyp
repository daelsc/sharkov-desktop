{
  "targets": [
    {
      "target_name": "process_audio_capture",
      "sources": [
        "src/addon.cpp",
        "src/audio_session_enum.cpp",
        "src/process_capture.cpp"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": [
        "NAPI_VERSION=8",
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],
      "conditions": [
        [
          "OS=='win'",
          {
            "libraries": [
              "ole32.lib",
              "mmdevapi.lib",
              "uuid.lib"
            ],
            "msvs_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": ["/std:c++17"]
              }
            }
          }
        ]
      ]
    }
  ]
}
