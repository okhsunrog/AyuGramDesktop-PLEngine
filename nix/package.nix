{
  lib,
  ayugram-desktop,
  telegram-desktop,
  source,
}:
let
  unwrapped = ayugram-desktop.unwrapped.overrideAttrs (previous: {
    version = "7.2.9-plengine";
    src = lib.cleanSourceWith {
      src = source;
      filter =
        path: type:
        lib.cleanSourceFilter path type
        && !(builtins.elem (lib.removePrefix "${source}/" path) [
          "flake.nix"
          "flake.lock"
          "nix"
          "AGENTS.md"
          "CLAUDE.md"
          "README.md"
          "README-RU.md"
          "README-pluginEngine.md"
        ]);
    };
    cmakeBuildType = "None";
    outputs = [
      "out"
      "plugins"
    ];
    cmakeFlags = (previous.cmakeFlags or [ ]) ++ [
      (lib.cmakeBool "DESKTOP_APP_DISABLE_AUTOUPDATE" true)
    ];
    postBuild = (previous.postBuild or "") + ''
      ayugram_build="$PWD"
      for plugin in plengine_smoke_plugin plengine_channel_reader; do
        cmake -S "$NIX_BUILD_TOP/$sourceRoot/tools/$plugin" -B "$plugin-build" -G Ninja \
          -DCMAKE_BUILD_TYPE=None -DAYUGRAM_BUILD_DIR="$ayugram_build"
        cmake --build "$plugin-build" --parallel "$NIX_BUILD_CORES"
      done
    '';
    postInstall = (previous.postInstall or "") + ''
      install -Dm755 plengine_smoke_plugin-build/ayu-plengine-smoke-plugin.so \
        "$plugins/lib/ayugram/plugins/ayu-plengine-smoke-plugin.so"
      install -Dm755 plengine_channel_reader-build/ayu-plengine-channel-reader.so \
        "$plugins/lib/ayugram/plugins/ayu-plengine-channel-reader.so"
    '';
  });
in
telegram-desktop.override {
  pname = "ayugram-plengine";
  inherit unwrapped;
}
