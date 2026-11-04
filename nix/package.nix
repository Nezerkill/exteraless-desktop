{
  pkgs,
  apiId ? "611335",
  apiHash ? "d524b414d21f4d37f08684c1df41ac9c",
  version ? "7.3.1-exteraless.0.1",
}:
let
  base = pkgs.telegram-desktop;
  unwrapped = base.unwrapped.overrideAttrs (old: {
    pname = "exteraless-desktop-unwrapped";
    inherit version;
    __intentionallyOverridingVersion = true;
    patches = (old.patches or []) ++ [ ./patches/exteraless.patch ];
    cmakeFlags = [
      "-DTDESKTOP_API_ID:STRING=${apiId}"
      "-DTDESKTOP_API_HASH:STRING=${apiHash}"
      "-DDESKTOP_APP_DISABLE_SWIFT6:BOOL=TRUE"
      "-DDESKTOP_APP_DISABLE_AUTOUPDATE:BOOL=TRUE"
      "-DDESKTOP_APP_DISABLE_CRASH_REPORTS:BOOL=TRUE"
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
      --prefix PATH : ${pkgs.lib.makeBinPath [ pkgs.bubblewrap pkgs.python3 ]}
  '';
})
