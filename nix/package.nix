{
  pkgs,
  apiId ? "611335",
  apiHash ? "d524b414d21f4d37f08684c1df41ac9c",
  version ? "7.3.1-exteraless.0.1",
}:
let
  base = pkgs.telegram-desktop;
  sttModel = pkgs.fetchzip {
    url = "https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-streaming-zipformer-small-bilingual-zh-en-2023-02-16.tar.bz2";
    sha256 = "sha256-ce0aFqfyv0aUSx70Mcr8WMaQxR8JBkBMoCsMIAztKHY=";
  };
  unwrapped = base.unwrapped.overrideAttrs (old: {
    pname = "exteraless-desktop-unwrapped";
    inherit version;
    __intentionallyOverridingVersion = true;
    patches = (old.patches or []) ++ [ ./patches/exteraless.patch ];
    buildInputs = (old.buildInputs or []) ++ [ pkgs.sherpa-onnx ];
    cmakeFlags = [
      "-DTDESKTOP_API_ID:STRING=${apiId}"
      "-DTDESKTOP_API_HASH:STRING=${apiHash}"
      "-DDESKTOP_APP_DISABLE_SWIFT6:BOOL=TRUE"
      "-DDESKTOP_APP_DISABLE_AUTOUPDATE:BOOL=TRUE"
      "-DDESKTOP_APP_DISABLE_CRASH_REPORTS:BOOL=TRUE"
      "-DSHERPA_ONNX_ROOT:STRING=${pkgs.sherpa-onnx}"
    ];
  });
in
(base.override {
  pname = "exteraless-desktop";
  inherit unwrapped;
}).overrideAttrs (old: {
  nativeBuildInputs = (old.nativeBuildInputs or []) ++ [ pkgs.makeWrapper ];

  postInstall = (old.postInstall or "") + ''
    if [ -e "$out/share/applications/org.telegram.desktop.desktop" ]; then
      chmod -R u+w "$out/share/applications"
      mv "$out/share/applications/org.telegram.desktop.desktop" \
        "$out/share/applications/org.exteraless.desktop.desktop"
      substituteInPlace "$out/share/applications/org.exteraless.desktop.desktop" \
        --replace-fail "Name=Telegram" "Name=Exteraless Desktop" \
        --replace-fail "Comment=New era of messaging" "Comment=Telegram client with exteraless features"
    fi
  '';

  postFixup = (old.postFixup or "") + ''
    wrapProgram "$out/bin/Telegram" \
      --prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.bubblewrap pkgs.python3 ]} \
      --set-default EXTERALESS_STT_MODEL "${sttModel}"
  '';
})
